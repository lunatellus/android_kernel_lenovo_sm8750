// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 */

#include <soc/qcom/watchdog.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>

struct wdt_register_map {
	u32 wdt0_en;
	u32 wdt0_rst;
	u32 wdt0_sts;
	u32 wdt0_bark_time;
	u32 wdt0_bite_time;
	unsigned long wdt_hz;
};

static struct wdt_register_map wdt_map = {
	.wdt0_en = 0x08,
	.wdt0_rst = 0x04,
	.wdt0_sts = 0x0C,
	.wdt0_bark_time = 0x10,
	.wdt0_bite_time = 0x14,
	.wdt_hz = 32765,
};

static inline int qcom_soc_set_wdt_bark(u32 time,
					struct msm_watchdog_data *wdog_dd)
{
	__raw_writel((time * wdt_map.wdt_hz)/1000, wdog_dd->base + wdt_map.wdt0_bark_time);
	/* Make sure register write is complete before proceeding */
	mb();
	return 0;
}

static inline int qcom_soc_set_wdt_bite(u32 time,
					struct msm_watchdog_data *wdog_dd)
{
	__raw_writel((time * wdt_map.wdt_hz)/1000, wdog_dd->base + wdt_map.wdt0_bite_time);
	/* Make sure register write is complete before proceeding */
	mb();
	return 0;
}



static inline int qcom_soc_reset_wdt(struct msm_watchdog_data *wdog_dd)
{
	__raw_writel(1, wdog_dd->base + wdt_map.wdt0_rst);
	/* Make sure register write is complete before proceeding */
	mb();
	return 0;
}

static inline int qcom_soc_enable_wdt(u32 val,
					 struct msm_watchdog_data *wdog_dd)
{
	__raw_writel(val, wdog_dd->base + wdt_map.wdt0_en);
	/* Make sure register write is complete before proceeding */
	mb();
	return 0;
}

static inline int qcom_soc_disable_wdt(struct msm_watchdog_data *wdog_dd)
{
	__raw_writel(0, wdog_dd->base + wdt_map.wdt0_en);
	/* Make sure register write is complete before proceeding */
	mb();
	return 0;
}

static inline int qcom_soc_show_wdt_status(struct msm_watchdog_data *wdog_dd)
{
	dev_err(wdog_dd->dev, "Wdog - STS: 0x%x, CTL: 0x%x, BARK TIME: 0x%x, BITE TIME: 0x%x\n",
			__raw_readl(wdog_dd->base + wdt_map.wdt0_sts),
			__raw_readl(wdog_dd->base + wdt_map.wdt0_en),
			__raw_readl(wdog_dd->base + wdt_map.wdt0_bark_time),
			__raw_readl(wdog_dd->base + wdt_map.wdt0_bite_time));
	return 0;
}

static struct qcom_wdt_ops qcom_soc_wdt_ops = {
	.set_bark_time     = qcom_soc_set_wdt_bark,
	.set_bite_time     = qcom_soc_set_wdt_bite,
	.reset_wdt         = qcom_soc_reset_wdt,
	.enable_wdt        = qcom_soc_enable_wdt,
	.disable_wdt       = qcom_soc_disable_wdt,
	.show_wdt_status   = qcom_soc_show_wdt_status
};

static int qcom_soc_wdt_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct msm_watchdog_data *wdog_dd;
	struct device_node *np = pdev->dev.of_node;
	u32 temp_hz;

	wdog_dd = devm_kzalloc(&pdev->dev, sizeof(*wdog_dd), GFP_KERNEL);
	if (!wdog_dd)
		return -ENOMEM;
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "wdt-base");
	if (!res)
		return -ENODEV;

	wdog_dd->base  = devm_ioremap_resource(&pdev->dev, res);
	if (!wdog_dd->base) {
		dev_err(&pdev->dev, "%s cannot map wdog register space\n",
				__func__);
		return -ENXIO;
	}

	of_property_read_u32(np, "qcom,wdt-en-offset", &wdt_map.wdt0_en);
	of_property_read_u32(np, "qcom,wdt-rst-offset", &wdt_map.wdt0_rst);
	of_property_read_u32(np, "qcom,wdt-sts-offset", &wdt_map.wdt0_sts);
	of_property_read_u32(np, "qcom,wdt-bark-offset", &wdt_map.wdt0_bark_time);
	of_property_read_u32(np, "qcom,wdt-bite-offset", &wdt_map.wdt0_bite_time);
	if (!of_property_read_u32(np, "qcom,wdt-hz", &temp_hz))
		wdt_map.wdt_hz = (unsigned long)temp_hz;

	wdog_dd->ops = &qcom_soc_wdt_ops;

	return qcom_wdt_register(pdev, wdog_dd, "msm-watchdog");
}

static const struct dev_pm_ops qcom_soc_dev_pm_ops = {
#ifdef CONFIG_PM_SLEEP
	.suspend_late = qcom_wdt_pet_suspend,
	.resume_early = qcom_wdt_pet_resume,
#endif
	.freeze_late = qcom_wdt_pet_suspend,
	.restore_early = qcom_wdt_pet_resume,
};

static const struct of_device_id qcom_soc_match_table[] = {
	{ .compatible = "qcom,msm-watchdog" },
	{}
};

static struct platform_driver qcom_soc_wdt_driver = {
	.probe = qcom_soc_wdt_probe,
	.remove = qcom_wdt_remove,
	.driver = {
		.name = "msm_watchdog",
		.pm = &qcom_soc_dev_pm_ops,
		.of_match_table = qcom_soc_match_table,
	},
};

static int __init init_watchdog(void)
{
	return platform_driver_register(&qcom_soc_wdt_driver);
}

#if IS_MODULE(CONFIG_QCOM_SOC_WATCHDOG)
module_init(init_watchdog);
#else
pure_initcall(init_watchdog);
#endif

static __exit void exit_watchdog(void)
{
	platform_driver_unregister(&qcom_soc_wdt_driver);
}
module_exit(exit_watchdog);
MODULE_DESCRIPTION("QCOM Soc Watchdog Driver");
MODULE_LICENSE("GPL");
