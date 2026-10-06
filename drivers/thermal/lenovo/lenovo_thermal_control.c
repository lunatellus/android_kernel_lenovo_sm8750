// SPDX-License-Identifier: GPL-2.0
/*
 * lenovo_thermal_control - CPU0 "thermal/control" sysfs mailbox
 *
 * Reimplementation of the vendor lenovo_thermal_control.ko for crDroid on the
 * Lenovo Legion Tab TB322FC ("elden").  There is no public source; this was
 * recovered from the stock AArch64 object.
 *
 * The module creates /sys/devices/system/cpu/cpu0/thermal/control.  Nothing
 * in the kernel ever reads it: userspace (and init.target.rc, which relaxes
 * the mode to 0666 at boot) writes a command string into it and something
 * else is expected to act on it.  The stock module performs no parsing, no
 * validation and no cpufreq/thermal-policy work at all, so neither does this
 * one.
 *
 * Copyright (C) 2024 crDroid contributors
 */

#include <linux/cpu.h>
#include <linux/errno.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/sysfs.h>

/* Stock .bss control_info, 144 bytes: text at +0, group at +128, kobj at +136. */
#define CONTROL_TEXT_LEN	128
#define CONTROL_READ_LEN	4096

static struct {
	char text[CONTROL_TEXT_LEN];
	const struct attribute_group *group;
	struct kobject *kobj;
} control_info;

/*
 * Stock's store keeps at most 127 bytes plus NUL, preserves trailing newlines,
 * and returns the input length even when the string was truncated.
 */
static ssize_t store_control(struct kobject *kobj, struct kobj_attribute *attr,
			     const char *buffer, size_t count)
{
	snprintf(control_info.text, CONTROL_TEXT_LEN, "%s", buffer);
	sysfs_notify(control_info.kobj, NULL, "control");
	return count;
}

/*
 * Stock .text 0x04..0x38: snprintf(buffer, 4096, "%s\n", text).
 */
static ssize_t show_control(struct kobject *kobj, struct kobj_attribute *attr,
			    char *buffer)
{
	return snprintf(buffer, CONTROL_READ_LEN, "%s\n", control_info.text);
}


/* Mode verified from stock .data control_attr + 8, not guessed from usage. */
static struct kobj_attribute control_attr =
	__ATTR(control, 0600, show_control, store_control);
static struct attribute *control_attrs[] = { &control_attr.attr, NULL };
static const struct attribute_group control_attr_group = {
	.attrs = control_attrs,
};

/* Stock .init.text 0x04..0x84. */
static int __init lenovo_thermal_control_init(void)
{
	int status;

	control_info.group = &control_attr_group;
	control_info.kobj = kobject_create_and_add("thermal",
						   &get_cpu_device(0)->kobj);
	if (!control_info.kobj)
		return -ENOMEM;

	status = sysfs_create_group(control_info.kobj, control_info.group);
	if (status)
		kobject_put(control_info.kobj);
	else
		kobject_uevent(control_info.kobj, KOBJ_ADD);

	return status;
}

module_init(lenovo_thermal_control_init);

MODULE_DESCRIPTION("Lenovo Thermal Control");
MODULE_LICENSE("GPL");