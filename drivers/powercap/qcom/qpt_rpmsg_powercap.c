// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * Qualcomm Powercap RPMSG Driver
 *
 * Hierarchy:
 *   /sys/class/powercap/qpt/qpt.0/sys/
 *     power_uw
 *     constraint_0 -> CAP (power limit)
 *     constraint_1 -> BAP (power limit + time_window_us AKA BWD)
 *
 *   /sys/class/powercap/qpt/qpt.0.0/CL0/power_uw (read-only)
 *   /sys/class/powercap/qpt/qpt.0.1/CL1/power_uw (read-only)
 *   /sys/class/powercap/qpt/qpt.0.2/CL2/power_uw (read-only)
 *
 * CAP/BAP/BWD are sent over RPMSG.
 * power_uw for sys/CL0/CL1/CL2 is exposed by the powercap core via
 * .get_power_uw().
 * CL0/CL1/CL2 power_uw is read from reserved memory.
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>
#include <linux/powercap.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/byteorder/generic.h>

#define DRV_NAME			"qpt-rpmsg-powercap"
#define CTRL_NAME			"qpt"

#define SHMEM_OFFSET_WRITTEN_1S		10552
#define SHMEM_OFFSET_MAX_1S		10556
#define SHMEM_OFFSET_MEAS_1S		10560
#define SHMEM_MEAS_1S_ENTRY_SIZE	40

#define ZONE_SYS			0
#define ZONE_CL0			1
#define ZONE_CL1			2
#define ZONE_CL2			3
#define NR_ZONES			4

#define CONSTR_CAP			0
#define CONSTR_BAP			1
#define NR_SYS_CONSTRAINTS		2

/*
 * Reserved memory layout
 */
#define PLD_PEP_SHARED_MEMORY_START_ADDRESS	0x81F30000
#define PLD_PEP_SHARED_MEMORY_SIZE_BYTES	0x6000

/*
 * Assumed order inside processor_power[]:
 *   [0] = CL0
 *   [1] = CL1
 *   [2] = CL2
 *   [3] = GPU
 */
#define PLD_NUM_MEASUREMENTS		4
#define PLD_NUM_1_SEC			128	/* ~2 minutes */

/*
 * Shared-memory power unit to uW conversion.
 * If reserved memory stores mW, keep this as 1000ULL.
 * If reserved memory already stores uW, set this to 1ULL.
 */
#define PLD_POWER_TO_UW			1000ULL

#define PLD_READ_INDEX(written_field, max_size, offset_from_latest) \
	(((written_field) + (max_size) - 1 - (offset_from_latest)) % \
	 (max_size))

enum sample_index {
	PLD_CPU_CLUSTER_0,
	PLD_CPU_CLUSTER_1,
	PLD_CPU_CLUSTER_2,
};

enum qpt_rsp_id {
	QPT_RSP_CL0_POWER = 20,
	QPT_RSP_CL1_POWER,
	QPT_RSP_CL2_POWER,
};

struct measurement_results {
	__le64 time;
	__le16 processor_power[PLD_NUM_MEASUREMENTS];
	__le16 reserved0[PLD_NUM_MEASUREMENTS];
	__le16 system_power;
	__le16 reserved1;
	__le16 reserved2;
	__le16 reserved3;
	__le16 reserved4;
} __packed;

enum qcpep_power_limit_type_v1 {
	QCPEP_CAP = 0,
	QCPEP_BAP,
	QCPEP_BWD,
	QCPEP_NUM_POWER_LIMIT_TYPES,
};

struct qcpep_power_limit {
	u32 size;
	enum qcpep_power_limit_type_v1 power_limit_type;
	u32 power_limit_value;
} __packed;

enum qcpep_to_pld_notification_type {
	QCPEP_NOTIF_POWER_LIMIT_SET_VALUE = 0,
	QCPEP_NOTIF_MODERN_STANDBY_STATE,
	QCPEP_NUM_PEP_TO_PLD_NOTIFICATIONS,
};

struct pep_modern_standby_notif {
	u32 version;
	u32 modern_standby_state;
	u64 reserved;
} __packed;

struct pep_to_pld_tx_msg {
	u32 size;
	enum qcpep_to_pld_notification_type notification_type;
	union {
		struct qcpep_power_limit power_limit;
		struct pep_modern_standby_notif ms_state;
	} __packed notif;
} __packed;

struct qpt_rpmsg_rx_msg {
	u32 id;
	u64 value;
} __packed;

struct qpt_zone {
	struct powercap_zone zone;
	const char *name;
	int zone_id;
	int nr_constraints;
	u64 last_power_uw;
	bool registered;
};

struct qpt_dev {
	struct powercap_control_type *pct;
	struct qpt_zone zones[NR_ZONES];
	struct rpmsg_endpoint *ept;
	struct mutex lock;
	u64 cap_limit_uw;
	u64 bap_limit_uw;
	u64 bap_window_us;
	void __iomem *shmem_base;
	bool connection_active;
};

static struct qpt_dev *qpt;

static int qpt_refresh_powers_from_shmem_locked(void)
{
	struct measurement_results res;
	u32 written_1s;
	u32 max_1s;
	u32 index;
	size_t entry_offset;
	void __iomem *base;

	if (!qpt || !qpt->shmem_base)
		return -ENODEV;

	base = qpt->shmem_base;

	memcpy_fromio(&written_1s, base + SHMEM_OFFSET_WRITTEN_1S,
		      sizeof(written_1s));
	memcpy_fromio(&max_1s, base + SHMEM_OFFSET_MAX_1S, sizeof(max_1s));

	if (!max_1s || max_1s > PLD_NUM_1_SEC)
		return -EINVAL;

	index = PLD_READ_INDEX(written_1s, max_1s, 0);
	entry_offset = SHMEM_OFFSET_MEAS_1S +
		       (index * SHMEM_MEAS_1S_ENTRY_SIZE);

	if (entry_offset + sizeof(res) > PLD_PEP_SHARED_MEMORY_SIZE_BYTES)
		return -EINVAL;

	memcpy_fromio(&res, base + entry_offset, sizeof(res));

	qpt->zones[ZONE_SYS].last_power_uw =
		(u64)le16_to_cpu(res.system_power) * PLD_POWER_TO_UW * 10;
	qpt->zones[ZONE_CL0].last_power_uw =
		(u64)le16_to_cpu(res.processor_power[PLD_CPU_CLUSTER_0]) *
		PLD_POWER_TO_UW * 10;
	qpt->zones[ZONE_CL1].last_power_uw =
		(u64)le16_to_cpu(res.processor_power[PLD_CPU_CLUSTER_1]) *
		PLD_POWER_TO_UW * 10;
	qpt->zones[ZONE_CL2].last_power_uw =
		(u64)le16_to_cpu(res.processor_power[PLD_CPU_CLUSTER_2]) *
		PLD_POWER_TO_UW * 10;

	return 0;
}

static int qpt_rpmsg_send_power_limit(enum qcpep_power_limit_type_v1 type,
				      u32 value)
{
	struct pep_to_pld_tx_msg msg;

	if (!qpt)
		return -ENODEV;

	if (!qpt->connection_active || !qpt->ept)
		return -EAGAIN;

	memset(&msg, 0, sizeof(msg));
	msg.size = sizeof(msg);
	msg.notification_type = QCPEP_NOTIF_POWER_LIMIT_SET_VALUE;
	msg.notif.power_limit.size = sizeof(struct qcpep_power_limit);
	msg.notif.power_limit.power_limit_type = type;
	msg.notif.power_limit.power_limit_value = value;

	return rpmsg_send(qpt->ept, &msg, sizeof(msg));
}

static int qpt_rpmsg_cb(struct rpmsg_device *rpdev, void *data, int len,
			void *priv, u32 src)
{
	const struct qpt_rpmsg_rx_msg *msg = data;

	if (!qpt)
		return -ENODEV;

	if (len < (int)sizeof(*msg))
		return -EINVAL;

	mutex_lock(&qpt->lock);

	switch (msg->id) {
	case QPT_RSP_CL0_POWER:
		qpt->zones[ZONE_CL0].last_power_uw = msg->value;
		break;
	case QPT_RSP_CL1_POWER:
		qpt->zones[ZONE_CL1].last_power_uw = msg->value;
		break;
	case QPT_RSP_CL2_POWER:
		qpt->zones[ZONE_CL2].last_power_uw = msg->value;
		break;
	default:
		break;
	}

	mutex_unlock(&qpt->lock);

	return 0;
}

static int qpt_set_power_limit(struct powercap_zone *pz, int cid, u64 val)
{
	struct qpt_zone *z = container_of(pz, struct qpt_zone, zone);
	int ret = 0;

	if (!qpt)
		return -ENODEV;

	if (z->zone_id != ZONE_SYS)
		return -EOPNOTSUPP;

	mutex_lock(&qpt->lock);

	switch (cid) {
	case CONSTR_CAP:
		ret = qpt_rpmsg_send_power_limit(QCPEP_CAP, (u32)val);
		if (!ret)
			qpt->cap_limit_uw = val;
		break;
	case CONSTR_BAP:
		ret = qpt_rpmsg_send_power_limit(QCPEP_BAP, (u32)val);
		if (!ret)
			qpt->bap_limit_uw = val;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&qpt->lock);

	return ret;
}

static int qpt_get_power_limit(struct powercap_zone *pz, int cid, u64 *val)
{
	struct qpt_zone *z = container_of(pz, struct qpt_zone, zone);

	if (!qpt)
		return -ENODEV;

	if (z->zone_id != ZONE_SYS)
		return -EOPNOTSUPP;

	mutex_lock(&qpt->lock);

	switch (cid) {
	case CONSTR_CAP:
		*val = qpt->cap_limit_uw;
		break;
	case CONSTR_BAP:
		*val = qpt->bap_limit_uw;
		break;
	default:
		mutex_unlock(&qpt->lock);
		return -EINVAL;
	}

	mutex_unlock(&qpt->lock);

	return 0;
}

static int qpt_set_time_window(struct powercap_zone *pz, int cid, u64 tw)
{
	struct qpt_zone *z = container_of(pz, struct qpt_zone, zone);
	int ret;

	if (!qpt)
		return -ENODEV;

	if (z->zone_id != ZONE_SYS)
		return -EOPNOTSUPP;

	if (cid != CONSTR_BAP)
		return -EINVAL;

	mutex_lock(&qpt->lock);
	ret = qpt_rpmsg_send_power_limit(QCPEP_BWD, (u32)tw);
	if (!ret)
		qpt->bap_window_us = tw;
	mutex_unlock(&qpt->lock);

	return ret;
}

static int qpt_get_time_window(struct powercap_zone *pz, int cid, u64 *tw)
{
	struct qpt_zone *z = container_of(pz, struct qpt_zone, zone);

	if (!qpt)
		return -ENODEV;

	if (z->zone_id != ZONE_SYS)
		return -EOPNOTSUPP;

	if (cid != CONSTR_BAP)
		return -EINVAL;

	mutex_lock(&qpt->lock);
	*tw = qpt->bap_window_us;
	mutex_unlock(&qpt->lock);

	return 0;
}

static int qpt_get_power_uw(struct powercap_zone *pz, u64 *uw)
{
	struct qpt_zone *z = container_of(pz, struct qpt_zone, zone);
	int ret;

	if (!qpt)
		return -ENODEV;

	mutex_lock(&qpt->lock);

	ret = qpt_refresh_powers_from_shmem_locked();
	if (ret) {
		mutex_unlock(&qpt->lock);
		return ret;
	}

	*uw = z->last_power_uw;

	mutex_unlock(&qpt->lock);

	return 0;
}

static int qpt_release(struct powercap_zone *pz)
{
	return 0;
}

static const char *qpt_constraint_name(struct powercap_zone *pz, int cid)
{
	struct qpt_zone *z = container_of(pz, struct qpt_zone, zone);

	if (z->zone_id != ZONE_SYS)
		return NULL;

	switch (cid) {
	case CONSTR_CAP:
		return "CAP";
	case CONSTR_BAP:
		return "BAP";
	default:
		return NULL;
	}
}

static const struct powercap_zone_constraint_ops qpt_constraint_ops = {
	.set_power_limit_uw = qpt_set_power_limit,
	.get_power_limit_uw = qpt_get_power_limit,
	.set_time_window_us = qpt_set_time_window,
	.get_time_window_us = qpt_get_time_window,
	.get_name = qpt_constraint_name,
};

static const struct powercap_zone_ops qpt_zone_ops = {
	.get_power_uw = qpt_get_power_uw,
	.release = qpt_release,
};

static void qpt_unregister_zone(int id)
{
	struct qpt_zone *z;

	if (!qpt)
		return;

	z = &qpt->zones[id];

	if (!z->registered)
		return;

	powercap_unregister_zone(qpt->pct, &z->zone);
	z->registered = false;
}

static int qpt_register_zone(int id, const char *name, int nconstr,
			     struct powercap_zone *parent)
{
	struct qpt_zone *z = &qpt->zones[id];
	struct powercap_zone *pcz;

	memset(z, 0, sizeof(*z));
	z->name = name;
	z->zone_id = id;
	z->nr_constraints = nconstr;

	pcz = powercap_register_zone(&z->zone, qpt->pct, name, parent,
				     &qpt_zone_ops, nconstr,
				     nconstr ? &qpt_constraint_ops : NULL);
	if (IS_ERR(pcz))
		return PTR_ERR(pcz);

	z->registered = true;

	dev_info(&z->zone.dev, "registered powercap zone %s\n", name);

	return 0;
}

static int qpt_register_all_zones(void)
{
	int ret;
	struct powercap_zone *parent_zone;

	ret = qpt_register_zone(ZONE_SYS, "sys", NR_SYS_CONSTRAINTS, NULL);
	if (ret)
		return ret;

	parent_zone = &qpt->zones[ZONE_SYS].zone;

	/* Register child zones (CL0, CL1, CL2) under sys with 0 constraints */
	ret = qpt_register_zone(ZONE_CL0, "CL0", 1, parent_zone);
	if (ret)
		goto err_unregister_sys;

	ret = qpt_register_zone(ZONE_CL1, "CL1", 1, parent_zone);
	if (ret)
		goto err_unregister_cl0;

	ret = qpt_register_zone(ZONE_CL2, "CL2", 1, parent_zone);
	if (ret)
		goto err_unregister_cl1;

	return 0;

err_unregister_cl1:
	qpt_unregister_zone(ZONE_CL1);
err_unregister_cl0:
	qpt_unregister_zone(ZONE_CL0);
err_unregister_sys:
	qpt_unregister_zone(ZONE_SYS);

	return ret;
}

static void qpt_unregister_all_zones(void)
{
	int i;

	if (!qpt)
		return;

	for (i = NR_ZONES - 1; i >= 0; i--)
		qpt_unregister_zone(i);
}

static int qpt_rpmsg_probe(struct rpmsg_device *rpdev)
{
	if (!qpt)
		return -EPROBE_DEFER;

	mutex_lock(&qpt->lock);

	if (qpt->connection_active) {
		mutex_unlock(&qpt->lock);
		return -EEXIST;
	}

	qpt->ept = rpdev->ept;
	qpt->connection_active = true;

	mutex_unlock(&qpt->lock);

	dev_info(&rpdev->dev, "QPT RPMSG connected\n");

	return 0;
}

static void qpt_rpmsg_remove(struct rpmsg_device *rpdev)
{
	if (!qpt)
		return;

	mutex_lock(&qpt->lock);
	qpt->connection_active = false;
	qpt->ept = NULL;
	mutex_unlock(&qpt->lock);

	dev_info(&rpdev->dev, "QPT RPMSG disconnected\n");
}

static const struct rpmsg_device_id qpt_rpmsg_ids[] = {
	{ .name = "APPS_ADSP_PWR_LMTS_GLINK_PORT" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, qpt_rpmsg_ids);

static const struct of_device_id qpt_platform_of_match[] = {
	{ .compatible = "qcom,qptrpmsg" },
	{ },
};
MODULE_DEVICE_TABLE(of, qpt_platform_of_match);

static struct rpmsg_driver qpt_rpmsg_driver = {
	.drv = {
		.name = DRV_NAME,
	},
	.id_table = qpt_rpmsg_ids,
	.probe = qpt_rpmsg_probe,
	.callback = qpt_rpmsg_cb,
	.remove = qpt_rpmsg_remove,
};

static int qpt_platform_probe(struct platform_device *pdev)
{
	int ret;

	if (qpt)
		return -EEXIST;

	qpt = devm_kzalloc(&pdev->dev, sizeof(*qpt), GFP_KERNEL);
	if (!qpt)
		return -ENOMEM;

	mutex_init(&qpt->lock);
	qpt->connection_active = false;
	qpt->ept = NULL;

	qpt->shmem_base = devm_ioremap(&pdev->dev,
				       PLD_PEP_SHARED_MEMORY_START_ADDRESS,
				       PLD_PEP_SHARED_MEMORY_SIZE_BYTES);
	if (!qpt->shmem_base) {
		dev_err(&pdev->dev, "failed to map PLD-PEP shared memory\n");
		ret = -ENOMEM;
		goto err_clear_qpt;
	}

	qpt->pct = powercap_register_control_type(NULL, CTRL_NAME, NULL);
	if (IS_ERR(qpt->pct)) {
		ret = PTR_ERR(qpt->pct);
		qpt->pct = NULL;
		goto err_clear_qpt;
	}

	ret = qpt_register_all_zones();
	if (ret)
		goto err_unregister_control_type;

	dev_info(&pdev->dev, "QPT platform initialized\n");

	return 0;

err_unregister_control_type:
	powercap_unregister_control_type(qpt->pct);
	qpt->pct = NULL;
err_clear_qpt:
	qpt = NULL;
	return ret;
}

static void qpt_platform_remove(struct platform_device *pdev)
{
	if (!qpt)
		return;

	mutex_lock(&qpt->lock);
	qpt->connection_active = false;
	qpt->ept = NULL;
	mutex_unlock(&qpt->lock);

	qpt_unregister_all_zones();

	if (qpt->pct) {
		powercap_unregister_control_type(qpt->pct);
		qpt->pct = NULL;
	}

	qpt = NULL;

	dev_info(&pdev->dev, "QPT platform removed\n");
}

static struct platform_driver qpt_platform_driver = {
	.driver = {
		.name = "qpt-powercap",
		.of_match_table = qpt_platform_of_match,
	},
	.probe = qpt_platform_probe,
	.remove = qpt_platform_remove,
};

static int __init qpt_init(void)
{
	int ret;

	ret = platform_driver_register(&qpt_platform_driver);
	if (ret)
		return ret;

	ret = register_rpmsg_driver(&qpt_rpmsg_driver);
	if (ret) {
		platform_driver_unregister(&qpt_platform_driver);
		return ret;
	}

	return 0;
}
module_init(qpt_init);

static void __exit qpt_exit(void)
{
	unregister_rpmsg_driver(&qpt_rpmsg_driver);
	platform_driver_unregister(&qpt_platform_driver);
}
module_exit(qpt_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. RPMSG Powercap Driver");
