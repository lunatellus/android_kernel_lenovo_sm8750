// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/cpu_phys_log_map.h>
#include <linux/mailbox_client.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched/walt.h>

#include <linux/scmi_protocol.h>
#include <linux/qcom_scmi_vendor.h>
#include <linux/debugfs.h>
#define FAST_ALGO_STR	(0x46415354) /*FAST*/

/* SCMI MESSAGE IDS */
enum fast_set_param_ids {
	PARAM_SET_TRH_CONFIG = 1,
	PARAM_SET_TRH_POLICY = 2,
	PARAM_SET_TRH_THRESHOLD = 3,
	PARAM_SET_HBPM_THRESHOLD = 4,
	PARAM_SET_HBPM_HYSTERESIS = 5,
	PARAM_SET_HBPM_OFFSET = 6,
};

enum fast_get_param_ids {
	PARAM_GET_VERSION = 1,
	PARAM_GET_TRH_CONFIG = 2,
	PARAM_GET_TRH_POLICIES = 3,
	PARAM_GET_TRH_CURRENT_POLICY = 4,
	PARAM_GET_TRH_CURRENT_PROFILE = 5,
	PARAM_GET_TRH_THRESHOLD = 6,
	PARAM_GET_HBPM_ENABLED = 7,
	PARAM_GET_HBPM_THRESHOLD = 8,
	PARAM_GET_HBPM_HYSTERESIS = 9,
	PARAM_GET_HBPM_OFFSET = 10,
};

#define PARAM_TRH_START_STOP	1
#define PARAM_HBPM_START_STOP	2

/* SCMI Handlers */
struct fast_config {
	uint32_t cpumask;
	uint32_t time_interval_ms;
} __packed;

static struct scmi_protocol_handle *ph;
static const struct qcom_scmi_vendor_ops *ops;
static struct scmi_device *sdev;

/* DEBUGFS Handlers */
#define FAST_HBPM_START		1
#define FAST_HBPM_THRESHOLD	2
#define FAST_HBPM_HYSTERESIS	3
#define FAST_HBPM_OFFSET	4

static u64 fast_hbpm_start = FAST_HBPM_START;
static u64 fast_hbpm_threshold = FAST_HBPM_THRESHOLD;
static u64 fast_hbpm_hysteresis = FAST_HBPM_HYSTERESIS;
static u64 fast_hbpm_offset = FAST_HBPM_OFFSET;

/* FAST internals */
struct qcom_cpucp_fast {
	uint32_t version;
	uint32_t period_msec;
	cpumask_t fast_cpus;
	uint8_t active;
	uint8_t num_policies;
	uint8_t last_cpu;
	struct mbox_client cl;
	struct mbox_chan *ch;
};

#define FAST_VERSION_MIN 0x10000
#define FAST_MAX_POLICIES 3
#define FAST_MBOX_CPUMASK 0xFF

static struct qcom_cpucp_fast fast_data;

static inline
struct qcom_cpucp_fast *to_qcom_cpucp_fast_info(struct mbox_client *cl)
{
	return container_of(cl, struct qcom_cpucp_fast, cl);
}

static void qcom_cpucp_fast_rx(struct mbox_client *cl, void *msg)
{
	struct qcom_cpucp_fast *data = &fast_data;
	uint64_t mbox_rx_data = *((uint64_t *)msg);
	int logical_cpu, cpu = mbox_rx_data & FAST_MBOX_CPUMASK;

	if (!cpumask_weight(&data->fast_cpus)) {
		dev_dbg(cl->dev, "No CPUS are enabled for FAST\n");
		return;
	}

	logical_cpu = cpu;
	if (cpu != FAST_MBOX_CPUMASK)
		logical_cpu = cpu_phys_to_logical(cpu);

	if (logical_cpu < 0) {
		dev_err(cl->dev, "Invalid cpu=%d logical=%d\n", cpu, logical_cpu);
		return;
	}

	if (cpumask_test_cpu(logical_cpu, &data->fast_cpus) ||
			((logical_cpu == FAST_MBOX_CPUMASK) && (data->last_cpu < nr_cpu_ids))) {
		data->last_cpu = logical_cpu;
		sched_walt_oscillate(logical_cpu);
	}
}

static int get_hbpm_start(void *data, u64 *val)
{
	uint32_t curr_sts = 0;
	int ret;

	ret =  ops->get_param(ph, &curr_sts, FAST_ALGO_STR,
			PARAM_GET_HBPM_ENABLED, 0, sizeof(uint32_t));
	if (ret < 0) {
		pr_err("failed to get current status, ret = %d\n", ret);
		return ret;
	}

	*val = curr_sts;
	return 0;
}

static int set_hbpm_start(void *data, u64 val)
{
	int ret = -EAGAIN;

	if (val) {
		ret = ops->start_activity(ph, &val, FAST_ALGO_STR,
				PARAM_HBPM_START_STOP, sizeof(uint64_t));
		if (ret < 0)
			pr_err("Failed to start the FAST HBPM ret=%d\n", ret);
	} else {
		ret = ops->stop_activity(ph, &val, FAST_ALGO_STR,
				PARAM_HBPM_START_STOP, sizeof(uint64_t));
		if (ret < 0)
			pr_err("Failed to stop the FAST HBPM ret=%d\n", ret);
	}

	return ret;
}

static int set_hbpm_threshold(void *data, u64 val)
{
	unsigned int data_type = *(unsigned int *)data;
	int ret = -EAGAIN, param_id;

	if (val < 0)
		return -ERANGE;

	switch (data_type) {
	case FAST_HBPM_THRESHOLD:
		param_id = PARAM_SET_HBPM_THRESHOLD;
		break;
	case FAST_HBPM_HYSTERESIS:
		param_id = PARAM_SET_HBPM_HYSTERESIS;
		break;
	case FAST_HBPM_OFFSET:
		param_id = PARAM_SET_HBPM_OFFSET;
		break;
	default:
		param_id = 0;
	}

	if (!param_id)
		return -EINVAL;

	ret =  ops->set_param(ph, &val, FAST_ALGO_STR, param_id,
				sizeof(uint32_t));
	if (ret < 0) {
		pr_err("FAST: failed to set tunable (%d), ret=%d\n",
				data_type, ret);
		return ret;
	}

	return 0;
}

static int get_hbpm_threshold(void *data, u64 *val)
{
	uint32_t curr_thresh = 0;
	unsigned int data_type = *(unsigned int *)data;
	int ret, param_id;

	switch (data_type) {
	case FAST_HBPM_START:
		param_id = PARAM_GET_HBPM_ENABLED;
		break;
	case FAST_HBPM_THRESHOLD:
		param_id = PARAM_GET_HBPM_THRESHOLD;
		break;
	case FAST_HBPM_HYSTERESIS:
		param_id = PARAM_GET_HBPM_HYSTERESIS;
		break;
	case FAST_HBPM_OFFSET:
		param_id = PARAM_GET_HBPM_OFFSET;
		break;
	default:
		param_id = 0;
	}

	if (!param_id)
		return -EINVAL;

	ret = ops->get_param(ph, &curr_thresh, FAST_ALGO_STR, param_id, 0,
			sizeof(uint32_t));
	if (ret < 0) {
		pr_err("failed to get current tunable (%d), ret=%d\n",
				data_type, ret);
		return ret;
	}

	*val = curr_thresh;
	return 0;
}

DEFINE_DEBUGFS_ATTRIBUTE(fast_hbpm_start_ops, get_hbpm_start,
				set_hbpm_start, "%llu\n");
DEFINE_DEBUGFS_ATTRIBUTE(fast_hbpm_ops, get_hbpm_threshold,
				set_hbpm_threshold, "%llu\n");

static int qcom_fast_create_debug_entries(void)
{
	struct dentry *ret, *dir;
	struct qcom_cpucp_fast *data = &fast_data;

	dir = debugfs_create_dir("cpucp_fast", 0);
	if (IS_ERR(dir)) {
		dev_err(data->cl.dev, "Debugfs cpucp dir creation failed\n");
		return -ENOENT;
	}

	ret = debugfs_create_file("hbpm_threshold", 0644, dir,
					&fast_hbpm_threshold, &fast_hbpm_ops);
	if (IS_ERR(ret)) {
		dev_err(data->cl.dev, "threshold file creation failed\n");
		debugfs_remove_recursive(dir);
		return -ENOENT;
	}

	ret = debugfs_create_file("hbpm_hysteresis", 0644, dir,
					&fast_hbpm_hysteresis, &fast_hbpm_ops);
	if (IS_ERR(ret)) {
		dev_err(data->cl.dev, "hbpm hysteresis file creation failed\n");
		debugfs_remove_recursive(dir);
		return -ENOENT;
	}

	ret = debugfs_create_file("hbpm_offset", 0644, dir,
			&fast_hbpm_offset, &fast_hbpm_ops);
	if (IS_ERR(ret)) {
		dev_err(data->cl.dev, "offset file creation failed\n");
		debugfs_remove_recursive(dir);
		return -ENOENT;
	}

	ret = debugfs_create_file("hbpm_start", 0644,
				dir, &fast_hbpm_start, &fast_hbpm_start_ops);
	if (IS_ERR(ret)) {
		dev_err(data->cl.dev, "hbpm_start file creation failed\n");
		debugfs_remove_recursive(dir);
		return -ENOENT;
	}

	return 0;
}

static int qcom_fast_scmi_discovery(struct platform_device *pdev)
{
	int ret = 0;
	struct qcom_cpucp_fast *data = &fast_data;

	sdev = get_qcom_scmi_device();
	if (IS_ERR(sdev)) {
		ret = PTR_ERR(sdev);
		if (ret != -EPROBE_DEFER)
			dev_info(&pdev->dev, "Error getting scmi_dev ret=%d\n", ret);
		return ret;
	}

	ops = sdev->handle->devm_protocol_get(sdev, QCOM_SCMI_VENDOR_PROTOCOL, &ph);
	if (IS_ERR(ops)) {
		ret = PTR_ERR(ops);
		ops = NULL;
		dev_info(&pdev->dev, "Error getting vendor protocol ops: %d\n", ret);
		return ret;
	}

	ret = ops->get_param(ph, &data->version, FAST_ALGO_STR, PARAM_GET_VERSION,
			0, sizeof(uint32_t));
	if (ret)
		return ret;

	if (data->version < FAST_VERSION_MIN)
		return -EOPNOTSUPP;

	ret = ops->get_param(ph, &data->num_policies, FAST_ALGO_STR,
				PARAM_GET_TRH_POLICIES,	0, sizeof(uint32_t));
	if (ret || (data->num_policies != FAST_MAX_POLICIES)) {
		pr_err("FAST: Invalid policy information\n");
		return ret;
	}

	pr_info("FAST version=%d no.of policies:%d\n", data->version, data->num_policies);

	return ret;
}

static int qcom_cpucp_fast_probe(struct platform_device *pdev)
{
	struct qcom_cpucp_fast *data = &fast_data;
	struct device *dev = &pdev->dev;
	struct cpufreq_policy *policy = NULL;
	int ret, cpu;

	data->cl.dev = dev;
	data->cl.rx_callback = qcom_cpucp_fast_rx;

	data->ch = mbox_request_channel(&data->cl, 0);
	if (IS_ERR(data->ch)) {
		ret = PTR_ERR(data->ch);
		if (ret != -EPROBE_DEFER) {
			dev_err(dev, "Error getting mailbox %d\n", ret);
			goto err_ch;
		}
	}

	ret = of_property_read_u32(dev->of_node, "qcom,policy-cpus", &cpu);
	if (ret) {
		dev_err(dev, "Error getting policy%d CPU: %d\n", cpu, ret);
		goto err;
	}

	cpu = cpu_phys_to_logical(cpu);
	if (cpu >= nr_cpu_ids || !cpu_present(cpu)) {
		dev_err(dev, "Invalid CPU%d\n", cpu);
		goto err;
	}

	policy = cpufreq_cpu_get(cpu);
	if (!policy) {
		dev_err(dev, "No policy for CPU:%d. Defer.\n", cpu);
		ret = -EPROBE_DEFER;
		goto err;
	}

	if (cpumask_weight(policy->related_cpus) <= 1) {
		cpufreq_cpu_put(policy);
		goto err;
	}

	cpumask_copy(&data->fast_cpus, policy->related_cpus);
	cpufreq_cpu_put(policy);

	/* SCMI support */
	ret = qcom_fast_scmi_discovery(pdev);
	if (!ret) {

		ret = qcom_fast_create_debug_entries();
		if (ret)
			dev_err(dev, "Not able to create debugfs entries(%d)\n", ret);

		/* Start FAST with default parameters */
		data->active = 1;
		ops->start_activity(ph, &data->active, FAST_ALGO_STR,
				PARAM_TRH_START_STOP, sizeof(uint64_t));
	}

	dev_info(dev, "Probe successful, FAST cpus=0x%lx\n", cpumask_bits(&data->fast_cpus)[0]);

	return 0;
err:
	mbox_free_channel(data->ch);
err_ch:
	return ret;
}

static void qcom_cpucp_fast_remove(struct platform_device *pdev)
{
	struct qcom_cpucp_fast *data = &fast_data;
	mbox_free_channel(data->ch);
};

static const struct of_device_id qcom_cpucp_fast_match[] = {
	{ .compatible = "qcom,cpucp_fast" },
	{}
};
MODULE_DEVICE_TABLE(of, qcom_cpucp_fast_match);

static struct platform_driver qcom_cpucp_fast_driver = {
	.probe = qcom_cpucp_fast_probe,
	.remove = qcom_cpucp_fast_remove,
	.driver = {
		.name = "qcom-cpucp-fast",
		.of_match_table = qcom_cpucp_fast_match,
	},
};
module_platform_driver(qcom_cpucp_fast_driver);

#if IS_ENABLED(CONFIG_QTI_SCMI_VENDOR_PROTOCOL)
MODULE_SOFTDEP("pre: qcom_scmi_client");
#endif
MODULE_DESCRIPTION("QCOM CPUCP FAST Driver");
MODULE_LICENSE("GPL");
