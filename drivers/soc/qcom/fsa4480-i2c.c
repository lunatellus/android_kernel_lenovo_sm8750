// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/usb/typec.h>
#include <linux/usb/ucsi_glink.h>
#include <linux/soc/qcom/fsa4480-i2c.h>
#include <linux/sysfs.h>

bool fsa4480_switch_C1_enable;
EXPORT_SYMBOL(fsa4480_switch_C1_enable);
unsigned int audio_4480_headset_ref;
EXPORT_SYMBOL(audio_4480_headset_ref);
struct fsa4480_priv *audio_4480_priv1;
EXPORT_SYMBOL(audio_4480_priv1);
struct fsa4480_sub_priv *audio_4480_priv2;
EXPORT_SYMBOL(audio_4480_priv2);
int direction_value;

#include <linux/qti-regmap-debugfs.h>

#define FSA4480_I2C_NAME	"fsa4480-driver"

#define FSA4480_SWITCH_SETTINGS 0x04
#define FSA4480_SWITCH_CONTROL  0x05
#define FSA4480_SWITCH_STATUS1  0x07
#define FSA4480_SLOW_L          0x08
#define FSA4480_SLOW_R          0x09
#define FSA4480_SLOW_MIC        0x0A
#define FSA4480_SLOW_SENSE      0x0B
#define FSA4480_SLOW_GND        0x0C
#define FSA4480_DELAY_L_R       0x0D
#define FSA4480_DELAY_L_MIC     0x0E
#define FSA4480_DELAY_L_SENSE   0x0F
#define FSA4480_DELAY_L_AGND    0x10
#define FSA4480_RESET           0x1E

extern unsigned int cc_port_num;

struct fsa4480_reg_val {
	u16 reg;
	u8 val;
};

static const struct regmap_config fsa4480_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = FSA4480_RESET,
};

static const struct fsa4480_reg_val fsa_reg_i2c_defaults[] = {
	{FSA4480_SLOW_L, 0x00},
	{FSA4480_SLOW_R, 0x00},
	{FSA4480_SLOW_MIC, 0x00},
	{FSA4480_SLOW_SENSE, 0x00},
	{FSA4480_SLOW_GND, 0x00},
	{FSA4480_DELAY_L_R, 0x00},
	{FSA4480_DELAY_L_MIC, 0x00},
	{FSA4480_DELAY_L_SENSE, 0x00},
	{FSA4480_DELAY_L_AGND, 0x09},
	{FSA4480_SWITCH_SETTINGS, 0x98},
};

static void fsa4480_usbc_update_settings(struct fsa4480_priv *fsa_priv,
		u32 switch_control, u32 switch_enable)
{
	u32 prev_control, prev_enable;

	if (!fsa_priv->regmap) {
		dev_err(fsa_priv->dev, "%s: regmap invalid\n", __func__);
		return;
	}

	regmap_read(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, &prev_control);
	regmap_read(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, &prev_enable);
	if (prev_control == switch_control && prev_enable == switch_enable) {
		dev_dbg(fsa_priv->dev, "%s: settings unchanged\n", __func__);
		return;
	}

	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, 0x80);
	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, switch_control);
	/* FSA4480 chip hardware requirement */
	usleep_range(50, 55);
	regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, switch_enable);
}

static int fsa4480_usbc_event_changed(struct notifier_block *nb,
				      unsigned long evt, void *ptr)
{
	struct fsa4480_priv *fsa_priv =
			container_of(nb, struct fsa4480_priv, ucsi_nb);
	struct device *dev;
	enum typec_accessory acc = ((struct ucsi_glink_constat_info *)ptr)->acc;

	if (!fsa_priv)
		return -EINVAL;

	dev = fsa_priv->dev;
	if (!dev)
		return -EINVAL;

	if (cc_port_num != 1)
		return 0;

	switch (acc) {
	case TYPEC_ACCESSORY_AUDIO:
	case TYPEC_ACCESSORY_NONE:
		if (atomic_read(&(fsa_priv->usbc_mode)) == acc)
			break; /* filter notifications received before */
		atomic_set(&(fsa_priv->usbc_mode), acc);
		if (acc == TYPEC_ACCESSORY_AUDIO)
			audio_4480_headset_ref++;
		else if (audio_4480_headset_ref)
			audio_4480_headset_ref--;
		pm_stay_awake(fsa_priv->dev);
		queue_work(system_freezable_wq, &fsa_priv->usbc_analog_work);
		break;
	default:
		break;
	}

	return 0;
}

static int fsa4480_usbc_analog_setup_switches(struct fsa4480_priv *fsa_priv)
{
	unsigned int reg6, reg4, reg17, tmp;
	unsigned int priv2_reg6, reg_value;
	int mode, i;

	if (!fsa_priv->dev)
		return -EINVAL;
	reg6 = 0;
	reg4 = 0;
	reg17 = 0;
	tmp = 0;
	priv2_reg6 = 0;
	reg_value = 0;

	mutex_lock(&fsa_priv->notification_lock);
	/* get latest mode again within locked context */
	mode = atomic_read(&fsa_priv->usbc_mode);

	switch (mode) {
	case TYPEC_ACCESSORY_AUDIO:
		regmap_write(fsa_priv->regmap, FSA4480_RESET, 1);
		for (i = FSA4480_SLOW_L; i <= FSA4480_SLOW_GND; i++) {
			regmap_write(fsa_priv->regmap, i, 0x4f);
			regmap_read(fsa_priv->regmap, i, &tmp);
		}
		regmap_read(fsa_priv->regmap, 0, &reg_value);
		if (reg_value == 0xf1)
			regmap_write(fsa_priv->regmap, 0x1c, 0);

		regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, 0x9f);
		regmap_read(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, &tmp);
		regmap_write(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, 0);
		regmap_read(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, &tmp);
		regmap_write(fsa_priv->regmap, 0x12, 0x0d);
		regmap_read(fsa_priv->regmap, 0x12, &tmp);
		for (i = 0; i < 10; i++) {
			msleep(5);
			if (tmp == 0x0c)
				break;
			regmap_read(fsa_priv->regmap, 0x12, &tmp);
		}
		msleep(1000);

		regmap_read(fsa_priv->regmap, 0x17, &reg17);
		regmap_read(fsa_priv->regmap, 6, &reg6);
		regmap_read(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, &reg4);
		dev_info(fsa_priv->dev, "%s: reg4: %d, reg_value: %d\n",
			 __func__, reg4, reg6);
		regmap_read(fsa_priv->regmap, 0, &reg_value);
		if (reg17 == 1 || reg6 == 5) {
			regmap_write(fsa_priv->regmap, FSA4480_RESET, 1);
			fsa4480_usbc_update_settings(fsa_priv, 0x00, 0x9f);
			msleep(5);
		}
		if (reg17 == 2) {
			regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, 0x87);
			msleep(1);
			regmap_write(fsa_priv->regmap, FSA4480_SWITCH_CONTROL, 0x07);
			msleep(5);
			regmap_write(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, 0x9f);
			msleep(5);
		}

		if (reg6 == 5 || reg6 == 0x1a)
			direction_value = 1;
		else if (reg6 == 0x2a)
			direction_value = 2;
		else
			direction_value = 0;

		/* notify call chain on event */
		blocking_notifier_call_chain(&fsa_priv->fsa4480_notifier,
					     mode, NULL);
		break;
	case TYPEC_ACCESSORY_NONE:
		fsa4480_switch_C1_enable = 0;
		/* notify call chain on event */
		blocking_notifier_call_chain(&fsa_priv->fsa4480_notifier,
				TYPEC_ACCESSORY_NONE, NULL);

		/* deactivate switches */
		regmap_write(fsa_priv->regmap, FSA4480_RESET, 1);
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);

		if (!audio_4480_headset_ref)
			break;

		/* the other port still carries a headset: set the sub switch up again */
		regmap_write(audio_4480_priv2->regmap, FSA4480_RESET, 1);
		for (i = FSA4480_SLOW_L; i <= FSA4480_SLOW_GND; i++)
			regmap_write(audio_4480_priv2->regmap, i, 0x4f);
		regmap_read(audio_4480_priv2->regmap, 0, &reg_value);
		if (reg_value == 0xf1)
			regmap_write(audio_4480_priv2->regmap, 0x1c, 0);
		regmap_write(audio_4480_priv2->regmap, FSA4480_SWITCH_SETTINGS, 0x9f);
		regmap_write(audio_4480_priv2->regmap, FSA4480_SWITCH_CONTROL, 0);
		regmap_write(audio_4480_priv2->regmap, 0x12, 0x0d);
		regmap_read(audio_4480_priv2->regmap, 0x12, &tmp);
		for (i = 0; i < 10; i++) {
			msleep(5);
			if (tmp == 0x0c)
				break;
			regmap_read(audio_4480_priv2->regmap, 0x12, &tmp);
		}
		msleep(1000);

		regmap_read(audio_4480_priv2->regmap, 0x17, &reg17);
		regmap_read(audio_4480_priv2->regmap, 6, &priv2_reg6);
		if (reg17 == 1 || priv2_reg6 == 5) {
			regmap_write(audio_4480_priv2->regmap, FSA4480_RESET, 1);
			regmap_write(audio_4480_priv2->regmap, FSA4480_SWITCH_SETTINGS, 0x80);
			regmap_write(audio_4480_priv2->regmap, FSA4480_SWITCH_CONTROL, 0);
			usleep_range(50, 55);
			regmap_write(audio_4480_priv2->regmap, FSA4480_SWITCH_SETTINGS, 0x9f);
			msleep(5);
		}
		if (reg17 == 2) {
			regmap_write(audio_4480_priv2->regmap, FSA4480_SWITCH_SETTINGS, 0x87);
			msleep(1);
			regmap_write(audio_4480_priv2->regmap, FSA4480_SWITCH_CONTROL, 0x07);
			msleep(5);
			regmap_write(audio_4480_priv2->regmap, FSA4480_SWITCH_SETTINGS, 0x9f);
			msleep(5);
		}
		blocking_notifier_call_chain(&audio_4480_priv2->fsa4480_sub_notifier,
					     TYPEC_ACCESSORY_AUDIO, NULL);
		break;
	default:
		/* ignore other usb connection modes */
		break;
	}

	mutex_unlock(&fsa_priv->notification_lock);
	return 0;
}

/*
 * fsa4480_reg_notifier - register notifier block with fsa driver
 *
 * @nb - notifier block of fsa4480
 * @node - phandle node to fsa4480 device
 *
 * Returns 0 on success, or error code
 */
int fsa4480_reg_notifier(struct notifier_block *nb,
			 struct device_node *node)
{
	int rc = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct fsa4480_priv *fsa_priv;

	if (!client)
		return -EINVAL;

	fsa_priv = (struct fsa4480_priv *)i2c_get_clientdata(client);
	if (!fsa_priv)
		return -EINVAL;

	rc = blocking_notifier_chain_register
				(&fsa_priv->fsa4480_notifier, nb);

	dev_dbg(fsa_priv->dev, "%s: registered notifier for %s\n",
		__func__, node->name);
	if (rc)
		return rc;

	/*
	 * as part of the init sequence check if there is a connected
	 * USB C analog adapter
	 */
	if (atomic_read(&(fsa_priv->usbc_mode)) == TYPEC_ACCESSORY_AUDIO) {
		dev_dbg(fsa_priv->dev, "%s: analog adapter already inserted\n",
			__func__);
		rc = fsa4480_usbc_analog_setup_switches(fsa_priv);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(fsa4480_reg_notifier);

/*
 * fsa4480_unreg_notifier - unregister notifier block with fsa driver
 *
 * @nb - notifier block of fsa4480
 * @node - phandle node to fsa4480 device
 *
 * Returns 0 on pass, or error code
 */
int fsa4480_unreg_notifier(struct notifier_block *nb,
			     struct device_node *node)
{
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct fsa4480_priv *fsa_priv;

	if (!client)
		return -EINVAL;

	fsa_priv = (struct fsa4480_priv *)i2c_get_clientdata(client);
	if (!fsa_priv)
		return -EINVAL;

	fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
	return blocking_notifier_chain_unregister
					(&fsa_priv->fsa4480_notifier, nb);
}
EXPORT_SYMBOL_GPL(fsa4480_unreg_notifier);

static int fsa4480_validate_display_port_settings(struct fsa4480_priv *fsa_priv)
{
	u32 switch_status = 0;

	regmap_read(fsa_priv->regmap, FSA4480_SWITCH_STATUS1, &switch_status);

	if ((switch_status != 0x23) && (switch_status != 0x1C)) {
		pr_err("AUX SBU1/2 switch status is invalid = %u\n",
				switch_status);
		return -EIO;
	}

	return 0;
}
/*
 * fsa4480_switch_event - configure FSA switch position based on event
 *
 * @node - phandle node to fsa4480 device
 * @event - fsa_function enum
 *
 * Returns int on whether the switch happened or not
 */
int fsa4480_switch_event(struct device_node *node,
			 enum fsa_function event)
{
	int switch_control = 0;
	struct i2c_client *client = of_find_i2c_device_by_node(node);
	struct fsa4480_priv *fsa_priv;

	if (!client)
		return -EINVAL;

	fsa_priv = (struct fsa4480_priv *)i2c_get_clientdata(client);
	if (!fsa_priv)
		return -EINVAL;
	if (!fsa_priv->regmap)
		return -EINVAL;

	pr_info("%s: event = %d\n", __func__, event);
	switch (event) {
	case FSA_MIC_GND_SWAP:
		regmap_read(fsa_priv->regmap, FSA4480_SWITCH_CONTROL,
				&switch_control);
		if ((switch_control & 0x07) == 0x07)
			switch_control = 0x0;
		else
			switch_control = 0x7;
		fsa4480_usbc_update_settings(fsa_priv, switch_control, 0x9F);
		break;
	case FSA_USBC_ORIENTATION_CC1:
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0xF8);
		return fsa4480_validate_display_port_settings(fsa_priv);
	case FSA_USBC_ORIENTATION_CC2:
		fsa4480_usbc_update_settings(fsa_priv, 0x78, 0xF8);
		return fsa4480_validate_display_port_settings(fsa_priv);
	case FSA_USBC_DISPLAYPORT_DISCONNECTED:
		fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fsa4480_switch_event);

static ssize_t direction_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct fsa4480_priv *fsa_priv = dev_get_drvdata(dev);
	unsigned int reg_value = 0, reg4 = 0;

	regmap_read(fsa_priv->regmap, 6, &reg_value);
	regmap_read(fsa_priv->regmap, FSA4480_SWITCH_SETTINGS, &reg4);
	dev_info(fsa_priv->dev, "%s: reg4: %d, reg_value: %d\n",
		 __func__, reg4, reg_value);

	if (reg_value == 0x1a)
		direction_value = 1;
	else if (reg_value == 0x2a)
		direction_value = 2;
	else
		direction_value = 0;
	return snprintf(buf, PAGE_SIZE, "%d\n", direction_value);
}

static ssize_t direction_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct fsa4480_priv *fsa_priv = dev_get_drvdata(dev);
	unsigned int data[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &data[0], &data[1]) == 2)
		regmap_write(fsa_priv->regmap, data[0], data[1]);

	dev_info(fsa_priv->dev, "%s: databuf[0]:%x, databuf[1]:%x\n",
		 __func__, data[0], data[1]);
	return count;
}
static DEVICE_ATTR_RW(direction);

static ssize_t reg_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct fsa4480_priv *fsa_priv = dev_get_drvdata(dev);
	unsigned int reg, value;
	ssize_t len = 0;
	int ret;

	if (!fsa_priv->regmap)
		printk("%s: regmap invalid\n", __func__);

	for (reg = FSA4480_SWITCH_SETTINGS; reg <= FSA4480_RESET; reg++) {
		regmap_read(fsa_priv->regmap, reg, &value);
		ret = snprintf(buf + len, PAGE_SIZE - len,
			       "reg[0x%x]=0x%x.\n", reg, value);
		len += ret;
	}

	return len;
}
static DEVICE_ATTR_RO(reg);

static struct attribute *fsa4480_attributes[] = {
	&dev_attr_direction.attr,
	&dev_attr_reg.attr,
	NULL,
};

static struct attribute_group fsa4480_attribute_group = {
	.attrs = fsa4480_attributes,
};

static void fsa4480_usbc_analog_work_fn(struct work_struct *work)
{
	struct fsa4480_priv *fsa_priv =
		container_of(work, struct fsa4480_priv, usbc_analog_work);

	if (!fsa_priv) {
		pr_err("%s: fsa container invalid\n", __func__);
		return;
	}
	fsa4480_usbc_analog_setup_switches(fsa_priv);
	pm_relax(fsa_priv->dev);
}

static void fsa4480_update_reg_defaults(struct regmap *regmap)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(fsa_reg_i2c_defaults); i++)
		regmap_write(regmap, fsa_reg_i2c_defaults[i].reg,
				   fsa_reg_i2c_defaults[i].val);
}

static int fsa4480_probe(struct i2c_client *i2c)
{
	struct fsa4480_priv *fsa_priv;
	int rc = 0;

	fsa_priv = devm_kzalloc(&i2c->dev, sizeof(*fsa_priv),
				GFP_KERNEL);
	if (!fsa_priv)
		return -ENOMEM;

	fsa_priv->dev = &i2c->dev;

	fsa_priv->regmap = devm_regmap_init_i2c(i2c, &fsa4480_regmap_config);
	if (IS_ERR_OR_NULL(fsa_priv->regmap)) {
		dev_err(fsa_priv->dev, "%s: Failed to initialize regmap: %d\n",
			__func__, rc);
		if (!fsa_priv->regmap) {
			rc = -EINVAL;
			goto err_data;
		}
		rc = PTR_ERR(fsa_priv->regmap);
		goto err_data;
	}

	fsa4480_update_reg_defaults(fsa_priv->regmap);
	devm_regmap_qti_debugfs_register(fsa_priv->dev, fsa_priv->regmap);

	fsa_priv->ucsi_nb.notifier_call = fsa4480_usbc_event_changed;
	fsa_priv->ucsi_nb.priority = 0;
	rc = register_ucsi_glink_notifier(&fsa_priv->ucsi_nb);
	if (rc) {
		dev_err(fsa_priv->dev, "%s: ucsi glink notifier registration failed: %d\n",
			__func__, rc);
		goto err_data;
	}

	mutex_init(&fsa_priv->notification_lock);
	i2c_set_clientdata(i2c, fsa_priv);

	rc = sysfs_create_group(&i2c->dev.kobj, &fsa4480_attribute_group);
	if (rc < 0)
		dev_err(fsa_priv->dev, "error creating sysfs attr files");
	audio_4480_priv1 = fsa_priv;

	INIT_WORK(&fsa_priv->usbc_analog_work,
		  fsa4480_usbc_analog_work_fn);

	BLOCKING_INIT_NOTIFIER_HEAD(&fsa_priv->fsa4480_notifier);

	return 0;

err_data:
	return rc;
}

static void fsa4480_remove(struct i2c_client *i2c)
{
	struct fsa4480_priv *fsa_priv =
			(struct fsa4480_priv *)i2c_get_clientdata(i2c);

	if (!fsa_priv)
		return;

	unregister_ucsi_glink_notifier(&fsa_priv->ucsi_nb);
	fsa4480_usbc_update_settings(fsa_priv, 0x18, 0x98);
	cancel_work_sync(&fsa_priv->usbc_analog_work);
	pm_relax(fsa_priv->dev);
	mutex_destroy(&fsa_priv->notification_lock);
	dev_set_drvdata(&i2c->dev, NULL);
}

static const struct of_device_id fsa4480_i2c_dt_match[] = {
	{
		.compatible = "qcom,fsa4480-i2c",
	},
	{}
};

static struct i2c_driver fsa4480_i2c_driver = {
	.driver = {
		.name = FSA4480_I2C_NAME,
		.of_match_table = fsa4480_i2c_dt_match,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
	.probe = fsa4480_probe,
	.remove = fsa4480_remove,
};

static int __init fsa4480_init(void)
{
	int rc;

	rc = i2c_add_driver(&fsa4480_i2c_driver);
	if (rc)
		pr_err("fsa4480: Failed to register I2C driver: %d\n", rc);

	return rc;
}
module_init(fsa4480_init);

static void __exit fsa4480_exit(void)
{
	i2c_del_driver(&fsa4480_i2c_driver);
}
module_exit(fsa4480_exit);

MODULE_DESCRIPTION("FSA4480 I2C driver");
MODULE_LICENSE("GPL");
