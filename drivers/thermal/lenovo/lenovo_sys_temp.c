// SPDX-License-Identifier: GPL-2.0
/*
 * lenovo_sys_temp - Lenovo virtual thermal zones
 *
 * Reimplementation of the vendor lenovo_sys_temp.ko for crDroid on the
 * Lenovo Legion Tab TB322FC ("elden").  There is no public source; this was
 * recovered instruction by instruction from the stock AArch64 object.
 *
 * The driver registers one *virtual* thermal zone per entry of
 * "lenovo,temperature-names".  Those zones have no trips and never sample
 * anything themselves: a delayed worker reads a list of *real* zones named by
 * "lenovo,sensor-listener-names", feeds them into a fixed integer model and
 * caches the five model outputs into the virtual zones, where the thermal core
 * picks them up through ->get_temp().
 *
 * Copyright (C) 2024 crDroid contributors
 */

#include <linux/dev_printk.h>
#include <linux/errno.h>
#include <linux/hq_hw_info.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/thermal.h>
#include <linux/workqueue.h>

#include "temperature_model.h"


#define LENOVO_SYS_TEMP_DT_COMPAT	"lenovo,sys-temp"
#define LENOVO_SYS_TEMP_DRV_NAME	"lenovo_sys_temp"


#define PROP_TEMP_NAMES		"lenovo,temperature-names"
#define PROP_LISTENER_NAMES	"lenovo,sensor-listener-names"
#define PROP_ALL_NTC_NAMES	"cust,all-ntc-names"

/* Stock .text 0x578 passes 1250 straight to queue_delayed_work_on() as
 * jiffies - there is no msecs_to_jiffies() call on that path.  1250 jiffies
 * is 5 s at the kernel's HZ=250.
 */
#define LENOVO_SYS_TEMP_FIRST_DELAY_JIFFIES	1250
/* Stock .text 0xbe0: msecs_to_jiffies(5000) for the steady state period. */
#define LENOVO_SYS_TEMP_PERIOD_MS	5000
/* Stock .text 0xc48/0xc70: both retry branches load 200 and jump to 0xbe4,
 * which is *past* the "mov w0, #0x1388" of the steady-state path and lands
 * directly on the __msecs_to_jiffies() call.  200 is milliseconds here.
 */
#define LENOVO_SYS_TEMP_RETRY_MS	200
/* Stock .text 0x698: tz_count values 0..10 are admitted and incremented, so
 * resolution is attempted 11 times before the worker gives up for good.
 */
#define LENOVO_SYS_TEMP_MAX_RETRY	10

/* Stock .text 0x04: kmalloc(1024) used to build the hw-info summary. */
#define LENOVO_SYS_TEMP_SUMMARY_LEN	1024

/**
 * struct lenovo_sys_temp_sensor - one cached thermal value
 * @tz_dev: registered thermal zone (virtual for @sensor, real for the
 *	    listener and all-NTC lists; unused/NULL where noted)
 * @name: DT string, owned by the device node
 * @temp: last value handed to the thermal core, in milli-degree Celsius
 *
 * Size 24, fields at +0/+8/+16 (stock BTF local lenovo_sys_temp_sensor).
 */
struct lenovo_sys_temp_sensor {
	struct thermal_zone_device *tz_dev;
	const char *name;
	int temp;
};

/**
 * struct lenovo_sys_temp_dev - per-platform-device private data
 * @pdev: owning platform device
 * @num_sensors: entries in @sensor
 * @num_sensors_listener: entries in @sensor_listener
 * @sensor: the virtual (output) zones
 * @sensor_listener: real zones sampled by the worker
 * @temp_changed_work: the sampling worker
 * @all_ntc_num: entries in @all_ntc_sensors
 * @all_ntc_sensors: extra names reported to hqsysfs only, never sampled
 */
struct lenovo_sys_temp_dev {
	struct platform_device *pdev;
	int num_sensors;
	int num_sensors_listener;
	struct lenovo_sys_temp_sensor *sensor;
	struct lenovo_sys_temp_sensor *sensor_listener;
	struct delayed_work temp_changed_work;
	int all_ntc_num;
	struct lenovo_sys_temp_sensor *all_ntc_sensors;
};

/* Stock .bss+0x8 / +0x10 / +0x14.  Single instance, as in stock. */
static struct lenovo_sys_temp_dev *sys_temp_dev;
static bool tz_init;
static int tz_count;


/**
 * get_all_ntc_thermal_temp - list the sensor names known to this device
 *
 * Despite the name this returns sensor *names*, not temperatures; they are
 * handed to hq_regiser_hw_info() as the thermal/NTC hardware description.
 * The buffer is handed over to hqsysfs and therefore deliberately never freed
 * (stock .text 0x04..0x120 allocates into the same static pointer and leaks
 * the previous allocation).  Called exactly once per probe.
 *
 * Return: the summary string, or the literal "unKnow!" if allocation failed.
 */
char *get_all_ntc_thermal_temp(void)
{
	static char *summary;
	size_t used = 0;
	int i;

	summary = kmalloc(LENOVO_SYS_TEMP_SUMMARY_LEN, GFP_KERNEL);
	if (!summary)
		return "unKnow!";

	memset(summary, 0, LENOVO_SYS_TEMP_SUMMARY_LEN);

	for (i = 0; i < sys_temp_dev->num_sensors; i++)
		used += sprintf(summary + used, "%s ",
				sys_temp_dev->sensor[i].name);
	for (i = 0; i < sys_temp_dev->all_ntc_num; i++)
		used += sprintf(summary + used, "%s ",
				sys_temp_dev->all_ntc_sensors[i].name);

	return summary;
}

/* Stock .text 0xc84..0xca0: purely a readback of the cached value. */
static int lenovo_sys_temp_get(struct thermal_zone_device *thermal, int *temp)
{
	struct lenovo_sys_temp_sensor *sensor = thermal_zone_device_priv(thermal);

	if (!sensor)
		return -EINVAL;

	*temp = sensor->temp;
	return 0;
}

static struct thermal_zone_device_ops lenovo_sys_temp_ops = {
	.get_temp = lenovo_sys_temp_get,
};

/*
 * Stock .text 0x638..0xc7c.  The work argument is unused: stock is a
 * singleton that always reaches for the global sys_temp_dev.
 */
static void temp_changed_work_func(struct work_struct *work)
{
	struct device_node *np;
	char buf[1024];
	int i, len = 0, count, delay;
	struct lenovo_temp_result r;
	int t[7], back_temp, user_temp;

	if (!sys_temp_dev)
		return;

	memset(buf, 0, sizeof(buf));
	count = sys_temp_dev->num_sensors_listener;

	if (!tz_init) {
		if (tz_count > LENOVO_SYS_TEMP_MAX_RETRY)
			return;
		tz_count++;

		np = sys_temp_dev->pdev->dev.of_node;
		for (i = 0; i < count; i++) {
			if (of_property_read_string_index(np, PROP_LISTENER_NAMES, i,
					&sys_temp_dev->sensor_listener[i].name)) {
				dev_err(&sys_temp_dev->pdev->dev,
					"Unable to read of_prop string\n");
				goto retry;
			}
			sys_temp_dev->sensor_listener[i].temp = 0;
			if (!sys_temp_dev->sensor_listener[i].name) {
				dev_err(&sys_temp_dev->pdev->dev,
					"Invalid sensor listener name\n");
				goto retry;
			}
			sys_temp_dev->sensor_listener[i].tz_dev =
				thermal_zone_get_zone_by_name(
					sys_temp_dev->sensor_listener[i].name);
			if (IS_ERR(sys_temp_dev->sensor_listener[i].tz_dev)) {
				dev_err(&sys_temp_dev->pdev->dev,
					"thermal_zone_get_zone_by_name() failed.name %s, i %d\n",
					sys_temp_dev->sensor_listener[i].name, i);
				goto retry;
			}
		}
		tz_init = true;
	}

	for (i = 0; i < count; i++) {
		if (!sys_temp_dev->sensor_listener[i].tz_dev) {
			dev_err(&sys_temp_dev->pdev->dev, "Invalid thermal zone\n");
			return;
		}
		/* Stock deliberately drops the return code: a failed read
		 * leaves the previous value in place. */
		thermal_zone_get_temp(sys_temp_dev->sensor_listener[i].tz_dev,
				      &sys_temp_dev->sensor_listener[i].temp);
	}

	for (i = 0; i < count; i++)
		len += sprintf(&buf[len], "[%d]%s=%d ", i,
			       sys_temp_dev->sensor_listener[i].name,
			       sys_temp_dev->sensor_listener[i].temp);

	/* Stock reads listener[0..6] without a listener-count check. */
	for (i = 0; i < 7; i++)
		t[i] = sys_temp_dev->sensor_listener[i].temp;
	lenovo_calc_temp(t, &r);
	back_temp = r.front > r.back_candidate ? r.front : r.back_candidate;
	user_temp = r.user_front > r.user_backleft ? r.user_front : r.user_backleft;
	user_temp = user_temp > r.user_backright ? user_temp : r.user_backright;

	dev_info(&sys_temp_dev->pdev->dev,
		 "temperature model V=%d prod=%d front_temp=%d back_temp=%d user_temp=%d user_front=%d user_backleft=%d user_backright=%d\n",
		 3, 1, r.front, r.back_candidate, user_temp,
		 r.user_front, r.user_backleft, r.user_backright);

	for (i = 0; i < sys_temp_dev->num_sensors_listener; i++) {
		const char *name = sys_temp_dev->sensor_listener[i].name;

		if (!strcmp(name, "lcm-thermal") || !strcmp(name, "quiet-therm"))
			dev_info(&sys_temp_dev->pdev->dev,
				 "[Thermal] %s temp: %d\n", name,
				 sys_temp_dev->sensor_listener[i].temp);
	}

	/* Stock .text 0xaf8..0xb74: dispatch by DT name, 20 byte case
	 * insensitive compare.  An unrecognised name keeps its old value. */
	for (i = 0; i < sys_temp_dev->num_sensors; i++) {
		struct lenovo_sys_temp_sensor *s = &sys_temp_dev->sensor[i];

		if (!strncasecmp(s->name, "front_temp", 20))
			s->temp = r.front;
		else if (!strncasecmp(s->name, "back_temp", 20))
			s->temp = back_temp;
		else if (!strncasecmp(s->name, "user_temp", 20))
			s->temp = user_temp;
		else if (!strncasecmp(s->name, "user_front_temp", 20))
			s->temp = r.user_front;
		else if (!strncasecmp(s->name, "user_back_temp", 20))
			s->temp = r.user_backright > r.user_backleft ?
				  r.user_backright : r.user_backleft;
	}

	/* The listener summary is not reset: stock appends here. */
	for (i = 0; i < sys_temp_dev->num_sensors; i++)
		len += sprintf(&buf[len], "[%d]%s=%d ", i,
			       sys_temp_dev->sensor[i].name,
			       sys_temp_dev->sensor[i].temp);

	delay = LENOVO_SYS_TEMP_PERIOD_MS;
out:
	schedule_delayed_work(&sys_temp_dev->temp_changed_work,
			      msecs_to_jiffies(delay));
	return;

retry:
	delay = LENOVO_SYS_TEMP_RETRY_MS;
	goto out;
}

/* Stock .text 0x128..0x5b0. */
static int lenovo_sys_temp_probe(struct platform_device *pdev)
{
	struct device_node *np;
	int num_sensors, num_listener, num_ntc;
	int ret, i, j, registered = 0;

	/* Stock logs through &pdev->dev before testing pdev for NULL. */
	dev_err(&pdev->dev, "Enter\n");
	if (!pdev)
		return -ENODEV;

	np = pdev->dev.of_node;
	if (!np) {
		dev_err(&pdev->dev, "bad of_node\n");
		return -ENODEV;
	}

	num_sensors = of_property_count_strings(np, PROP_TEMP_NAMES);
	if (num_sensors <= 0) {
		dev_err(&pdev->dev, "bad number of sensors: %d\n", num_sensors);
		return -EINVAL;
	}

	num_listener = of_property_count_strings(np, PROP_LISTENER_NAMES);
	if (num_listener > 0 && num_listener < 7) {
		dev_err(&pdev->dev, "bad number of sensors-listener: %d\n",
			num_listener);
		return -EINVAL;
	}

	num_ntc = of_property_count_strings(np, PROP_ALL_NTC_NAMES);
	if (num_ntc < 0)
		num_ntc = 0;

	sys_temp_dev = devm_kzalloc(&pdev->dev, sizeof(*sys_temp_dev), GFP_KERNEL);
	if (!sys_temp_dev) {
		dev_err(&pdev->dev, "Unable to alloc memory for sys_temp_dev\n");
		return -ENOMEM;
	}
	sys_temp_dev->pdev = pdev;
	sys_temp_dev->num_sensors = num_sensors;
	INIT_DELAYED_WORK(&sys_temp_dev->temp_changed_work,
			  temp_changed_work_func);

	sys_temp_dev->sensor = devm_kzalloc(&pdev->dev,
			sizeof(*sys_temp_dev->sensor) * num_sensors, GFP_KERNEL);
	if (!sys_temp_dev->sensor) {
		dev_err(&pdev->dev, "Unable to alloc memory for sensor\n");
		ret = -ENOMEM;
		goto err_unregister;
	}

	for (i = 0; i < num_sensors; i++) {
		ret = of_property_read_string_index(np, PROP_TEMP_NAMES, i,
					&sys_temp_dev->sensor[i].name);
		if (ret) {
			dev_err(&pdev->dev, "Unable to read of_prop string\n");
			goto err_unregister;
		}
		sys_temp_dev->sensor[i].temp = 0;
		sys_temp_dev->sensor[i].tz_dev =
			thermal_zone_device_register_with_trips(
					sys_temp_dev->sensor[i].name, NULL, 0,
					&sys_temp_dev->sensor[i],
					&lenovo_sys_temp_ops, NULL, 0, 0);
		if (IS_ERR(sys_temp_dev->sensor[i].tz_dev)) {
			dev_err(&pdev->dev,
				"thermal_zone_device_register_with_trips() failed.\n");
			ret = PTR_ERR(sys_temp_dev->sensor[i].tz_dev);
			goto err_unregister;
		}
		registered++;
	}
	platform_set_drvdata(pdev, sys_temp_dev);

	/* Stock registers the virtual zones first and then, if no listener is
	 * configured, succeeds without ever starting the worker. */
	if (num_listener <= 0) {
		dev_info(&sys_temp_dev->pdev->dev,
			 "No configure sensors listener !\n");
		return 0;
	}
	sys_temp_dev->num_sensors_listener = num_listener;

	sys_temp_dev->sensor_listener = devm_kzalloc(&pdev->dev,
			sizeof(*sys_temp_dev->sensor_listener) * num_listener,
			GFP_KERNEL);
	if (!sys_temp_dev->sensor_listener) {
		dev_err(&pdev->dev,
			"Unable to alloc memory for sensor_listener\n");
		ret = -ENOMEM;
		goto err_unregister;
	}

	sys_temp_dev->all_ntc_num = num_ntc;
	sys_temp_dev->all_ntc_sensors = devm_kzalloc(&pdev->dev,
			sizeof(*sys_temp_dev->all_ntc_sensors) * num_ntc,
			GFP_KERNEL);
	if (!sys_temp_dev->all_ntc_sensors) {
		ret = -ENOMEM;
		goto err_unregister;
	}

	for (i = 0; i < num_ntc; i++) {
		ret = of_property_read_string_index(np, PROP_ALL_NTC_NAMES, i,
					&sys_temp_dev->all_ntc_sensors[i].name);
		if (ret) {
			dev_err(&pdev->dev,
				"Unable to read all_ntc_sensors string\n");
			goto err_unregister;
		}
		sys_temp_dev->all_ntc_sensors[i].temp = 0;
		sys_temp_dev->all_ntc_sensors[i].tz_dev =
			thermal_zone_get_zone_by_name(
					sys_temp_dev->all_ntc_sensors[i].name);
		if (IS_ERR(sys_temp_dev->all_ntc_sensors[i].tz_dev))
			dev_err(&pdev->dev, "%s tz_dev is null!\n",
				sys_temp_dev->all_ntc_sensors[i].name);
	}

	hq_regiser_hw_info(HWID_THERMAL, get_all_ntc_thermal_temp());

	schedule_delayed_work(&sys_temp_dev->temp_changed_work,
			      LENOVO_SYS_TEMP_FIRST_DELAY_JIFFIES);
	/* Stock .text 0x584 logs this before returning success. */
	dev_err(&pdev->dev, "Done\n");
	return 0;

err_unregister:
	for (j = 0; j < registered; j++)
		thermal_zone_device_unregister(sys_temp_dev->sensor[j].tz_dev);
	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, sys_temp_dev);
	sys_temp_dev = NULL;
	return ret;
}

/* Stock .text 0x5b8..0x630. */
static void lenovo_sys_temp_remove(struct platform_device *pdev)
{
	struct lenovo_sys_temp_dev *dev = platform_get_drvdata(pdev);
	int i;

	cancel_delayed_work_sync(&dev->temp_changed_work);
	for (i = 0; i < dev->num_sensors; i++)
		thermal_zone_device_unregister(dev->sensor[i].tz_dev);

	platform_set_drvdata(pdev, NULL);
	sys_temp_dev = NULL;
	tz_init = false;
	tz_count = 0;
}

static const struct of_device_id lenovo_sys_temp_match_table[] = {
	{ .compatible = LENOVO_SYS_TEMP_DT_COMPAT },
	{ }
};
MODULE_DEVICE_TABLE(of, lenovo_sys_temp_match_table);

static struct platform_driver lenovo_sys_temp_driver = {
	.probe = lenovo_sys_temp_probe,
	.remove = lenovo_sys_temp_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = LENOVO_SYS_TEMP_DRV_NAME,
		.of_match_table = lenovo_sys_temp_match_table,
	},
};
module_platform_driver(lenovo_sys_temp_driver);

MODULE_SOFTDEP("pre: qcom_spmi_adc5_gen3");
/*
 * Stock carries alias=platform:lenovo_sys_temp but has *no*
 * __mod_platform__*_device_table symbol, so this is a hand written
 * MODULE_ALIAS() and not a platform_device_id table - declaring the table
 * would add a global symbol stock does not have and push the alias behind
 * the of: ones.  Declared right after MODULE_SOFTDEP to match the order
 * the tags appear in the stock .modinfo.
 */
MODULE_ALIAS("platform:" LENOVO_SYS_TEMP_DRV_NAME);

MODULE_AUTHOR("Lenovo");
MODULE_DESCRIPTION("Lenovo Thermal");
MODULE_LICENSE("GPL");