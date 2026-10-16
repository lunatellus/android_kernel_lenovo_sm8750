// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/device.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/remoteproc/qcom_rproc.h>
#include <soc/qcom/qcom_ramdump.h>
#include <linux/types.h>
#include <soc/qcom/rpm-smd.h>

#define RPM_MINIDUMP_REQ	0x6373696d
#define RPM_MINIDUMP_ID		0x0
#define RPM_MINIDUMP_KEY	0x64676F6C
#define RPM_MINIDUMP_ENTER	0x1
#define RPM_MINIDUMP_EXIT	0x0

struct rpm_data {
	struct device *dev;
	struct resource *res;
	void *ssr_handler;
	struct notifier_block modem_ssr_nb;
	bool crashed;
};

static int modem_ssr_notifier_nb(struct notifier_block *nb,
				 unsigned long code, void *data)
{
	struct qcom_ssr_notify_data *notif_data = data;
	struct qcom_dump_segment ramdump_entry;
	struct rpm_data *rpm_data;
	struct msm_rpm_kvp kvp_req;
	struct list_head head;
	int ret = 0;
	u32 val;

	if (!notif_data)
		return NOTIFY_OK;

	rpm_data = container_of(nb, struct rpm_data, modem_ssr_nb);
	if (code == QCOM_SSR_BEFORE_SHUTDOWN && notif_data->crashed) {
		val = RPM_MINIDUMP_ENTER;
		kvp_req.key = RPM_MINIDUMP_KEY;
		kvp_req.data = (void *)&val;
		kvp_req.length = sizeof(val);

		ret = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET, RPM_MINIDUMP_REQ,
								RPM_MINIDUMP_ID, &kvp_req, 1);
		if (ret) {
			dev_err(rpm_data->dev,
					"Failed to send RPM message before SSR shutdown\n");
			return NOTIFY_BAD;
		}

		rpm_data->crashed = notif_data->crashed;
	} else if (code == QCOM_SSR_AFTER_SHUTDOWN && rpm_data->crashed) {
		memset(&ramdump_entry, 0, sizeof(ramdump_entry));

		rpm_data->crashed = notif_data->crashed;

		ramdump_entry.da = rpm_data->res->start;
		ramdump_entry.size = resource_size(rpm_data->res);
		ramdump_entry.name = "rpm";

		INIT_LIST_HEAD(&head);
		list_add(&ramdump_entry.node, &head);

		ret = qcom_elf_dump_using_section(&head, rpm_data->dev, ELF_CLASS);
		if (ret)
			dev_err(rpm_data->dev,
					"Failed to collect RPM dump after SSR: %d\n", ret);

		val = RPM_MINIDUMP_EXIT;
		kvp_req.key = RPM_MINIDUMP_KEY;
		kvp_req.data = (void *)&val;
		kvp_req.length = sizeof(val);

		ret = msm_rpm_send_message(MSM_RPM_CTX_ACTIVE_SET, RPM_MINIDUMP_REQ,
								RPM_MINIDUMP_ID, &kvp_req, 1);
		if (ret) {
			dev_err(rpm_data->dev,
					"Failed to send RPM message after SSR shutdown\n");
			return NOTIFY_BAD;
		}
	}
	return NOTIFY_OK;
}


static int rpm_dump_driver_probe(struct platform_device *pdev)
{
	struct rpm_data *rpm_data;
	struct device_node *node;
	int ret;

	rpm_data = devm_kzalloc(&pdev->dev, sizeof(*rpm_data), GFP_KERNEL);
	if (!rpm_data)
		return -ENOMEM;

	rpm_data->dev = &pdev->dev;
	rpm_data->modem_ssr_nb.notifier_call = modem_ssr_notifier_nb;

	rpm_data->res = devm_kzalloc(rpm_data->dev, sizeof(*rpm_data->res),
				     GFP_KERNEL);
	if (!rpm_data->res)
		return -ENOMEM;

	node = of_parse_phandle(rpm_data->dev->of_node, "memory-region", 0);
	if (!node) {
		dev_err(rpm_data->dev, "missing shareable memory-region\n");
		return -EINVAL;
	}

	ret = of_address_to_resource(node, 0, rpm_data->res);
	of_node_put(node);
	if (ret)
		return ret;

	rpm_data->ssr_handler = qcom_register_ssr_notifier("mpss", &rpm_data->modem_ssr_nb);
	if (IS_ERR(rpm_data->ssr_handler)) {
		ret = PTR_ERR(rpm_data->ssr_handler);
		dev_err(rpm_data->dev, "Modem register notifier failed: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, rpm_data);
	return 0;
}

static void rpm_dump_driver_remove(struct platform_device *pdev)
{
	struct rpm_data *rpm_data = platform_get_drvdata(pdev);

	if (!rpm_data || IS_ERR(rpm_data->ssr_handler))
		return;

	qcom_unregister_ssr_notifier(rpm_data->ssr_handler,
				     &rpm_data->modem_ssr_nb);
}

static const struct of_device_id rpm_dump_of_match[] = {
	{ .compatible = "qcom,rpm-minidump", },
	{},
};

static struct platform_driver rpm_dump_driver = {
	.probe = rpm_dump_driver_probe,
	.remove = rpm_dump_driver_remove,
	.driver  = {
		.name   = "rpm-minidump",
		.of_match_table = rpm_dump_of_match,
		.suppress_bind_attrs = true,
	},
};

static int __init msm_rpm_dump_driver_init(void)
{
	return platform_driver_register(&rpm_dump_driver);
}

module_init(msm_rpm_dump_driver_init);
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. RPM dump Driver");
MODULE_LICENSE("GPL");
