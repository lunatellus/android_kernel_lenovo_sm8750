// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/errno.h>

static struct platform_device *pdev;
char power_mode_buf[64] = "NONE";
EXPORT_SYMBOL_GPL(power_mode_buf);
static bool device_file_created;

static ssize_t power_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	//If empty, print nothing
	if (power_mode_buf[0] == '\0')
		return 0;
	return sysfs_emit(buf, "%s\n", power_mode_buf);
}

static ssize_t power_mode_store(struct device *dev, struct device_attribute *attr, const char *buf,
			 size_t count)
{
	size_t n = min(count, sizeof(power_mode_buf) - 1);

	memcpy(power_mode_buf, buf, n);
	power_mode_buf[n] = '\0';

	//Simple trim of newline at end
	while (n > 0 && (power_mode_buf[n - 1] == '\n' || power_mode_buf[n - 1] == '\r'))
		power_mode_buf[--n] = '\0';
	sysfs_notify(&dev->kobj, NULL, "power_mode");

	return count;
}

//Defines dev_attr_power_mode
static DEVICE_ATTR_RW(power_mode);

static int __init qcom_power_mode_init(void)
{
	int ret;

	pdev = platform_device_register_simple("qcom_power_mode", -1, NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);
	ret = device_create_file(&pdev->dev, &dev_attr_power_mode);
	if (ret) {
		pr_err("qcom_power_mode: Failed to create device file: %d\n", ret);
		platform_device_unregister(pdev);
		return ret;
	}

	device_file_created = true;

	return 0;
}

static void __exit qcom_power_mode_exit(void)
{
	if (device_file_created)
		device_remove_file(&pdev->dev, &dev_attr_power_mode);
	platform_device_unregister(pdev);
}

module_init(qcom_power_mode_init);
module_exit(qcom_power_mode_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Power mode driver to distinguish between Suspend-to-RAM(S2R) and Deep Sleep states");
