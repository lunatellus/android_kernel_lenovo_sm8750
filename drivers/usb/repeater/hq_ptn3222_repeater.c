// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * eUSB2 repeater driver for i2c-attached repeaters (NXP PTN3222 and the
 * TI eUSB2 family).
 *
 * Derived from drivers/usb/repeater/repeater-qti-pmic-eusb2.c of the msm
 * (Qualcomm) kernel KERNEL.PLATFORM.4.0.r1-14700-kernel.0.  The repeater is
 * reached over I2C instead of being a register range of the PMIC, the PMIC
 * tuning registers are replaced by DT-provided write sequences, and reset is a
 * GPIO rather than the RPTR_EN bit.
 */

#include <linux/bitops.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/qti-regmap-debugfs.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/repeater.h>

/* from <linux/usb/dwc3-msm.h> of the msm kernel, not available in GKI */
#define PHY_HOST_MODE		BIT(0)

/* Presence flags of the two repeater dies of a USB-C port, maintained by the
 * UCSI/PD stack (ucsi_qti_glink).
 */
extern int C1_d_present;
extern int C2_d_present;

#define EUSB2_3P0_VOL_MIN			3075000	/* uV */
#define EUSB2_3P0_VOL_MAX			3300000	/* uV */
#define EUSB2_3P0_HPM_LOAD			3500	/* uA */

#define EUSB2_1P8_VOL_MIN			1800000	/* uV */
#define EUSB2_1P8_VOL_MAX			1800000	/* uV */
#define EUSB2_1P8_HPM_LOAD			32000	/* uA */

/* Revision register of the TI eUSB2 family */
#define EUSB2_REVISION_TI			0xb0
/* Revision register of the NXP PTN3222 */
#define EUSB2_REVISION_NXP			0x13

/* A repeater package holds two dies (C1 and C2) behind one I2C address */
#define EUSB2_NUM_REPEATERS			2
/* Registers snapshot taken on power down and replayed on the next init */
#define EUSB2_NUM_SAVE_REGS			11

/* Silicone revision, derived from the OF compatible string */
enum eusb2_repeater_type {
	TI_REPEATER,
	NXP_REPEATER,
};

/* Which repeater die of the USB-C port this instance drives */
enum eusb2_repeater_id {
	C1,
	C2,
};

/* Addresses of the enum values, referenced from struct of_device_id.data */
static enum eusb2_repeater_type repeater_chip[] = {
	[TI_REPEATER]	= TI_REPEATER,
	[NXP_REPEATER]	= NXP_REPEATER,
};

static const struct of_device_id eusb2_repeater_id_table[] = {
	{
		.compatible = "nxp,eusb2-repeater",
		.data = &repeater_chip[NXP_REPEATER],
	},
	{
		.compatible = "ti,eusb2-repeater",
		.data = &repeater_chip[TI_REPEATER],
	},
	{ },
};
MODULE_DEVICE_TABLE(of, eusb2_repeater_id_table);

/*
 * Index of the repeater whose registers may be snapshot and restored.  When C1
 * is present it owns the shared regulators, so only C2 may be power cycled
 * behind its back.
 */
static enum eusb2_repeater_id pre_present;

struct i2c_repeater_chip {
	struct device		*dev;
	struct usb_repeater	ur;
	struct regmap		*regmap;
	enum eusb2_repeater_type *repeater_type;
	u16			reg_base;
	struct regulator	*vdd18;
	struct regulator	*vdd3;
	bool			power_enabled;
	u8			reg_read_inited[EUSB2_NUM_REPEATERS];
	u8			dts_value_inited[EUSB2_NUM_REPEATERS];
	struct gpio_desc	*reset;
	u32			*param_override_seq_c1;
	u8			param_override_seq_cnt_c1;
	u32			*param_override_seq_c2;
	u8			param_override_seq_cnt_c2;
	u8			reg_save[EUSB2_NUM_REPEATERS][EUSB2_NUM_SAVE_REGS];
};

static const struct regmap_config eusb2_i2c_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xff,
};


static int eusb2_repeater_reg_read(struct i2c_repeater_chip *er,
					u8 *val, u8 reg)
{
	unsigned int reg_val = 0;
	int ret;

	ret = regmap_read(er->regmap, reg, &reg_val);
	if (ret < 0) {
		dev_err(er->dev, "Failed to read reg:0x%02x ret=%d\n", reg, ret);
		return ret;
	}

	*val = reg_val;
	return 0;
}

static int eusb2_repeater_reg_write(struct i2c_repeater_chip *er,
					u8 reg, u8 val)
{
	int ret;

	ret = regmap_write(er->regmap, reg, val);
	if (ret < 0)
		dev_err(er->dev, "failed to write 0x%02x to reg: 0x%02x ret=%d\n",
			val, reg, ret);
	return ret;
}

/* Write a DT-provided sequence of (value, register) pairs */
static void eusb2_repeater_write_seq(struct i2c_repeater_chip *er,
					u32 *seq, u8 cnt)
{
	int i;

	for (i = 0; i < cnt; i += 2)
		eusb2_repeater_reg_write(er, seq[i + 1], seq[i]);
}

/* Replay a register snapshot taken by eusb2_repeater_power() on power down */
static void eusb2_repeater_restore_seq(struct i2c_repeater_chip *er,
					enum eusb2_repeater_id id)
{
	int i;

	for (i = 0; i < EUSB2_NUM_SAVE_REGS; i++) {
		/* registers 2 and 3 are not restored */
		if (i != 2 && i != 3)
			eusb2_repeater_reg_write(er, i, er->reg_save[id][i]);
	}
}

static int eusb2_repeater_power(struct i2c_repeater_chip *er, bool on)
{
	int i, ret = 0;

	/*
	 * Snapshot the tuning registers before the rails collapse, so that the
	 * next init() can replay them instead of the DT sequence.
	 */
	if (!on && er->reg_read_inited[pre_present] &&
	    !er->dts_value_inited[pre_present]) {
		for (i = 0; i < EUSB2_NUM_SAVE_REGS; i++)
			eusb2_repeater_reg_read(er,
					&er->reg_save[pre_present][i], i);
		er->dts_value_inited[pre_present] = true;
	}

	if (er->power_enabled == on)
		return 0;

	if (!on)
		goto disable_vdd3;

	ret = regulator_set_load(er->vdd18, EUSB2_1P8_HPM_LOAD);
	if (ret < 0) {
		dev_err(er->ur.dev, "Unable to set HPM of vdd12:%d\n", ret);
		goto err_vdd18;
	}

	ret = regulator_set_voltage(er->vdd18, EUSB2_1P8_VOL_MIN,
						EUSB2_1P8_VOL_MAX);
	if (ret) {
		dev_err(er->ur.dev,
				"Unable to set voltage for vdd18:%d\n", ret);
		goto put_vdd18_lpm;
	}

	ret = regulator_enable(er->vdd18);
	if (ret) {
		dev_err(er->ur.dev, "Unable to enable vdd18:%d\n", ret);
		goto unset_vdd18;
	}

	ret = regulator_set_load(er->vdd3, EUSB2_3P0_HPM_LOAD);
	if (ret < 0) {
		dev_err(er->ur.dev, "Unable to set HPM of vdd3:%d\n", ret);
		goto disable_vdd18;
	}

	ret = regulator_set_voltage(er->vdd3, EUSB2_3P0_VOL_MIN,
						EUSB2_3P0_VOL_MAX);
	if (ret) {
		dev_err(er->ur.dev,
				"Unable to set voltage for vdd3:%d\n", ret);
		goto put_vdd3_lpm;
	}

	ret = regulator_enable(er->vdd3);
	if (ret) {
		dev_err(er->ur.dev, "Unable to enable vdd3:%d\n", ret);
		goto unset_vdd3;
	}

	er->power_enabled = true;
	return ret;

disable_vdd3:
	ret = regulator_disable(er->vdd3);
	if (ret)
		dev_err(er->ur.dev, "Unable to disable vdd3:%d\n", ret);

unset_vdd3:
	ret = regulator_set_voltage(er->vdd3, 0, EUSB2_3P0_VOL_MAX);
	if (ret)
		dev_err(er->ur.dev,
			"Unable to set (0) voltage for vdd3:%d\n", ret);

put_vdd3_lpm:
	ret = regulator_set_load(er->vdd3, 0);
	if (ret < 0)
		dev_err(er->ur.dev, "Unable to set (0) HPM of vdd3\n");

disable_vdd18:
	ret = regulator_disable(er->vdd18);
	if (ret)
		dev_err(er->ur.dev, "Unable to disable vdd18:%d\n", ret);

unset_vdd18:
	ret = regulator_set_voltage(er->vdd18, 0, EUSB2_1P8_VOL_MAX);
	if (ret)
		dev_err(er->ur.dev,
			"Unable to set (0) voltage for vdd18:%d\n", ret);

put_vdd18_lpm:
	ret = regulator_set_load(er->vdd18, 0);
	if (ret < 0)
		dev_err(er->ur.dev, "Unable to set LPM of vdd18\n");

	/* case handling when regulator turning on failed */
	if (!er->power_enabled)
		return -EINVAL;

err_vdd18:
	er->power_enabled = false;
	return ret;
}

static int eusb2_repeater_init(struct usb_repeater *ur)
{
	struct i2c_repeater_chip *er =
			container_of(ur, struct i2c_repeater_chip, ur);
	u8 version = 0;
	int ret;

	switch (*er->repeater_type) {
	case TI_REPEATER:
		ret = eusb2_repeater_reg_read(er, &version, EUSB2_REVISION_TI);
		/* TI revision 3 needs the host-mode CM.Lx workaround */
		if (!ret && version == 3)
			ur->flags |= PHY_HOST_MODE;
		break;
	case NXP_REPEATER:
		eusb2_repeater_reg_read(er, &version, EUSB2_REVISION_NXP);
		break;
	default:
		dev_err(ur->dev, "Invalid repeater\n");
		break;
	}

	dev_info(ur->dev, "eUSB2 repeater version = 0x%x ur->flags:0x%x, C1_d_present:%d, C2_d_present:%d\n",
			version, ur->flags, C1_d_present, C2_d_present);

	if (C1_d_present) {
		dev_info(ur->dev, "write C1 sequence\n");
		if (er->param_override_seq_cnt_c1) {
			if (!er->reg_read_inited[C1]) {
				dev_info(ur->dev, "write C1 sequence from dtsi\n");
				eusb2_repeater_write_seq(er,
						er->param_override_seq_c1,
						er->param_override_seq_cnt_c1);
				er->reg_read_inited[C1] = true;
			} else if (er->dts_value_inited[C1]) {
				dev_info(ur->dev,
					"write C1 sequence from reg save value\n");
				eusb2_repeater_restore_seq(er, C1);
				er->dts_value_inited[C1] = false;
			} else {
				dev_info(ur->dev, "no need to write C1 sequence\n");
			}
		}
		pre_present = C1;
	} else if (C2_d_present) {
		dev_info(ur->dev, "write C2 sequence\n");
		if (er->param_override_seq_cnt_c2) {
			if (!er->reg_read_inited[C2]) {
				dev_info(ur->dev, "write C2 sequence from dtsi\n");
				eusb2_repeater_write_seq(er,
						er->param_override_seq_c2,
						er->param_override_seq_cnt_c2);
				er->reg_read_inited[C2] = true;
			} else if (er->dts_value_inited[C2]) {
				dev_info(ur->dev,
					"write C2 sequence from reg save value\n");
				eusb2_repeater_restore_seq(er, C2);
				er->dts_value_inited[C2] = false;
			} else {
				dev_info(ur->dev, "no need to write C2 sequence\n");
			}
		}
		pre_present = C2;
	}

	dev_info(ur->dev, "eUSB2 repeater init\n");

	return 0;
}

static int eusb2_repeater_reset(struct usb_repeater *ur,
				bool bring_out_of_reset)
{
	struct i2c_repeater_chip *er =
			container_of(ur, struct i2c_repeater_chip, ur);

	gpiod_set_value_cansleep(er->reset, bring_out_of_reset);
	return 0;
}

static int eusb2_repeater_powerup(struct usb_repeater *ur)
{
	struct i2c_repeater_chip *er =
			container_of(ur, struct i2c_repeater_chip, ur);

	return eusb2_repeater_power(er, true);
}

static int eusb2_repeater_powerdown(struct usb_repeater *ur)
{
	struct i2c_repeater_chip *er =
			container_of(ur, struct i2c_repeater_chip, ur);

	return eusb2_repeater_power(er, false);
}

static int eusb2_repeater_i2c_probe(struct i2c_client *client)
{
	struct i2c_repeater_chip *er;
	struct device *dev = &client->dev;
	const struct of_device_id *match;
	int ret, num_elem;

	er = devm_kzalloc(dev, sizeof(*er), GFP_KERNEL);
	if (!er)
		return -ENOMEM;

	er->dev = dev;

	match = of_match_node(eusb2_repeater_id_table, client->dev.of_node);
	er->repeater_type = (enum eusb2_repeater_type *)match->data;

	er->regmap = devm_regmap_init_i2c(client, &eusb2_i2c_regmap);
	if (!er->regmap) {
		dev_err(dev, "failed to allocate register map\n");
		return -EINVAL;
	}

	devm_regmap_qti_debugfs_register(dev, er->regmap);

	i2c_set_clientdata(client, er);

	ret = of_property_read_u16(client->dev.of_node, "reg", &er->reg_base);
	if (ret < 0) {
		dev_err(dev, "failed to get reg base address:%d\n", ret);
		return ret;
	}

	er->vdd3 = devm_regulator_get(dev, "vdd3");
	if (IS_ERR(er->vdd3)) {
		dev_err(dev, "unable to get vdd3 supply\n");
		return PTR_ERR(er->vdd3);
	}

	er->vdd18 = devm_regulator_get(dev, "vdd18");
	if (IS_ERR(er->vdd18)) {
		dev_err(dev, "unable to get vdd18 supply\n");
		return PTR_ERR(er->vdd18);
	}

	er->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(er->reset))
		return PTR_ERR(er->reset);

	num_elem = of_property_count_elems_of_size(client->dev.of_node,
			"qcom,param-override-seq-c1", sizeof(u32));
	dev_err(dev, "eusb2_repeater_i2c:num_elem:%d\n", num_elem);
	if (num_elem > 0) {
		if (num_elem % 2) {
			dev_err(dev, "invalid param_override_seq_len_c1\n");
			return -EINVAL;
		}

		dev_info(dev, "get c1 param override seq from dtsi\n");
		er->param_override_seq_cnt_c1 = num_elem;
		er->param_override_seq_c1 = devm_kcalloc(dev,
				er->param_override_seq_cnt_c1, sizeof(u32), GFP_KERNEL);
		if (!er->param_override_seq_c1)
			return -ENOMEM;

		ret = of_property_read_u32_array(client->dev.of_node,
				"qcom,param-override-seq-c1",
				er->param_override_seq_c1,
				er->param_override_seq_cnt_c1);
		if (ret < 0) {
			dev_err(dev, "qcom,param-override-seq-c1 read failed %d\n",
					ret);
			return ret;
		}
	}

	num_elem = of_property_count_elems_of_size(client->dev.of_node,
			"qcom,param-override-seq-c2", sizeof(u32));
	if (num_elem > 0) {
		if (num_elem % 2) {
			dev_err(dev, "invalid param_override_seq_len_c2\n");
			return -EINVAL;
		}

		dev_info(dev, "get c2 param override seq from dtsi\n");
		er->param_override_seq_cnt_c2 = num_elem;
		er->param_override_seq_c2 = devm_kcalloc(dev,
				er->param_override_seq_cnt_c2, sizeof(u32), GFP_KERNEL);
		if (!er->param_override_seq_c2)
			return -ENOMEM;

		ret = of_property_read_u32_array(client->dev.of_node,
				"qcom,param-override-seq-c2",
				er->param_override_seq_c2,
				er->param_override_seq_cnt_c2);
		if (ret < 0) {
			dev_err(dev, "qcom,param-override-seq-c2 read failed %d\n",
					ret);
			return ret;
		}
	}

	er->ur.dev = dev;
	er->ur.init = eusb2_repeater_init;
	er->ur.reset = eusb2_repeater_reset;
	er->ur.powerup = eusb2_repeater_powerup;
	er->ur.powerdown = eusb2_repeater_powerdown;

	ret = usb_add_repeater_dev(&er->ur);
	if (ret)
		return ret;

	dev_err(dev, "nxp probe success\n");

	return 0;
}

static void eusb2_repeater_i2c_remove(struct i2c_client *client)
{
	struct i2c_repeater_chip *er = i2c_get_clientdata(client);

	if (!er)
		return;

	usb_remove_repeater_dev(&er->ur);
	eusb2_repeater_power(er, false);
}

static struct i2c_driver eusb2_i2c_repeater_driver = {
	.probe = eusb2_repeater_i2c_probe,
	.remove = eusb2_repeater_i2c_remove,
	.driver = {
		.name = "eusb2-repeater-nxp",
		.of_match_table = eusb2_repeater_id_table,
	},
};

module_i2c_driver(eusb2_i2c_repeater_driver);
MODULE_DESCRIPTION("eUSB2 i2c repeater driver");
MODULE_LICENSE("GPL v2");