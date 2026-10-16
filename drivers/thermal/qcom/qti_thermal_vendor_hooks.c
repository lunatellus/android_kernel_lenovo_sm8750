// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/thermal.h>
#include <linux/suspend.h>
#include <trace/hooks/thermal.h>

static atomic_t in_hibernate;

static int qti_thermal_pm_notifier(struct notifier_block *nb,
				unsigned long mode, void *_unused)
{
	switch (mode) {
	case PM_HIBERNATION_PREPARE:
		atomic_set(&in_hibernate, 1);
		pr_debug("%s: hibernate prepare mode %lu\n", __func__, mode);
		break;
	case PM_POST_HIBERNATION:
		atomic_set(&in_hibernate, 0);
		pr_debug("%s: hibernate post mode %lu\n", __func__, mode);
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block qti_thermal_pm_nb = {
	.notifier_call = qti_thermal_pm_notifier,
};

static void qti_thermal_pm_notify(void *unused,
		struct thermal_zone_device *tz, int *irq_wakeable)
{
	/* Make IRQ non-wakeable for deep sleep or hibernation */
	if ((pm_suspend_target_state == PM_SUSPEND_MEM) || (atomic_read(&in_hibernate)))
		*irq_wakeable = false;
	else
		*irq_wakeable = true;
	pr_debug("%s: irq_wakeable=%d\n", __func__, *irq_wakeable);
}

static int __init qcom_thermal_vendor_hook_driver_init(void)
{
	int ret;

	register_pm_notifier(&qti_thermal_pm_nb);

	ret = register_trace_android_vh_thermal_pm_notify_suspend(
			qti_thermal_pm_notify, NULL);
	if (ret)
		pr_err("Failed to register thermal_pm_notify hook, err:%d\n",
			ret);

	return 0;
}

static void __exit qcom_thermal_vendor_hook_driver_exit(void)
{
	unregister_trace_android_vh_thermal_pm_notify_suspend(
			qti_thermal_pm_notify, NULL);
	unregister_pm_notifier(&qti_thermal_pm_nb);
}

#if IS_MODULE(CONFIG_QTI_THERMAL_VENDOR_HOOK)
module_init(qcom_thermal_vendor_hook_driver_init);
#else
pure_initcall(qcom_thermal_vendor_hook_driver_init);
#endif
module_exit(qcom_thermal_vendor_hook_driver_exit);

MODULE_DESCRIPTION("QCOM Thermal Vendor Hooks Driver");
MODULE_LICENSE("GPL");
