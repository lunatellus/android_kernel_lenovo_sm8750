// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/of_gpio.h>
#include <linux/util_macros.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#if IS_ENABLED(CONFIG_REGULATOR_DEBUG_CONTROL)
#include <linux/regulator/debug-regulator.h>
#endif

#define REN_SLG5BR_RSC_CTRL 0x110D

/* VOUT selection registers (8-bit VSEL written to reg<7:0>) */
#define REN_SLG5BR_LDO1_VSEL 0x2000 /*VOUT = 1200 mV + 10 mV * VSEL */
#define REN_SLG5BR_LDO2_VSEL 0x2200 /*VOUT = 1200 mV + 10 mV * VSEL */
#define REN_SLG5BR_LDO3_VSEL 0x2300 /*VOUT = 1200 mV + 10 mV * VSEL */
#define REN_SLG5BR_LDO4_VSEL 0x2500 /*VOUT = 1200 mV + 10 mV * VSEL */
#define REN_SLG5BR_LDO5_VSEL 0x2700 /*VOUT = 1200 mV + 10 mV * VSEL */
#define REN_SLG5BR_LDO6_VSEL 0x2900 /*VOUT = 400 mV +   5 mV * VSEL */
#define REN_SLG5BR_LDO7_VSEL 0x3100 /*VOUT = 400 mV +   5 mV * VSEL */
#define REN_SLG5BR_LDO8_VSEL 0x3200 /*VOUT = 400 mV +   5 mV * VSEL */

#define SLG5BV4987X_LDO1_EN_BIT (1 << 0)
#define SLG5BV4987X_LDO2_EN_BIT (1 << 1)
#define SLG5BV4987X_LDO3_EN_BIT (1 << 2)
#define SLG5BV4987X_LDO4_EN_BIT (1 << 3)
#define SLG5BV4987X_LDO5_EN_BIT (1 << 4)
#define SLG5BV4987X_LDO6_EN_BIT (1 << 5)
#define SLG5BV4987X_LDO7_EN_BIT (1 << 6)
#define SLG5BV4987X_LDO8_EN_BIT (1 << 7)

#define SLG5BV4987X_GPIO1_MUX_ARRAY_INPUT_SEL_16_REG      0x1710
#define SLG5BV4987X_GPIO2_MUX_ARRAY_INPUT_SEL_17_REG      0x1711
#define SLG5BV4987X_GPIO3_MUX_ARRAY_INPUT_SEL_18_REG      0x1712
#define SLG5BV4987X_GPIO4_MUX_ARRAY_INPUT_SEL_19_REG      0x1713

/* renslgr data */
struct renslgr_data {
	struct regmap *regmap;
	struct device *dev;
	struct i2c_client *client;
	struct regulator_init_data *reg_init_data;
	struct regulator_desc regulator_desc;
	struct regulator_dev *regulator;
	int ldo1_mv;
	int ldo2_mv;
	int ldo3_mv;
	int ldo4_mv;
	int ldo5_mv;
	int ldo6_mv;
	int ldo7_mv;
	int ldo8_mv;
};


static const struct regmap_config renslgr_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.max_register   = 0x3200,
	.cache_type     = REGCACHE_RBTREE,
	.use_single_read = true,
	.use_single_write = true,
};


static void set_ldo_control(struct device *dev, bool enable)
{
	int ret;
	int ctrl;
	int mask;
	struct i2c_client *client = to_i2c_client(dev);
	struct renslgr_data *pdata = (struct renslgr_data *)
						i2c_get_clientdata(client);

	ret = regmap_read(pdata->regmap, REN_SLG5BR_RSC_CTRL, &ctrl);
	if (ret < 0)
		dev_err(dev, "failed to read REN_SLG5BR_RSC_CTRL ret:%x\n", ret);

	mask = (SLG5BV4987X_LDO1_EN_BIT | SLG5BV4987X_LDO2_EN_BIT |
		SLG5BV4987X_LDO3_EN_BIT | SLG5BV4987X_LDO4_EN_BIT |
		SLG5BV4987X_LDO5_EN_BIT | SLG5BV4987X_LDO6_EN_BIT |
		SLG5BV4987X_LDO7_EN_BIT | SLG5BV4987X_LDO8_EN_BIT);
	if (enable)
		ctrl |= mask;
	else
		ctrl &= ~mask;

	ret = regmap_write(pdata->regmap, REN_SLG5BR_RSC_CTRL, ctrl);
	if (ret < 0)
		dev_err(dev, "failed to write REN_SLG5BR_RSC_CTRL, ctrl:%x ret:%x\n", ctrl, ret);
	/* Enable GPIO3 to enable LDO Switch for 6DOF NPU and Mono camera */
	ret = regmap_write(pdata->regmap, SLG5BV4987X_GPIO3_MUX_ARRAY_INPUT_SEL_18_REG, 0x1);
	if (ret < 0)
		dev_err(dev,
		"failed to WR SLG5BV4987X_GPIO3_INPUT_SEL_18_REG SLG5BR to 0x1 ret:%x\n", ret);
}

static void set_ldo1(struct device *dev)
{
	int ret;
	int vsel;
	struct i2c_client *client = to_i2c_client(dev);
	struct renslgr_data *pdata = (struct renslgr_data *)
						i2c_get_clientdata(client);

	vsel = ((pdata->ldo1_mv - 1200) / 10);
	ret = regmap_write(pdata->regmap, REN_SLG5BR_LDO1_VSEL, vsel);
	if (ret < 0)
		dev_err(dev, "failed to write REN_SLG5BR_LDO1_VSEL, vsel:%x ret:%x\n", vsel, ret);
}

static void set_ldo2(struct device *dev)
{
	int ret;
	int vsel;
	struct i2c_client *client = to_i2c_client(dev);
	struct renslgr_data *pdata = (struct renslgr_data *)
						i2c_get_clientdata(client);

	vsel = ((pdata->ldo2_mv - 1200) / 10);
	ret = regmap_write(pdata->regmap, REN_SLG5BR_LDO2_VSEL, vsel);
	if (ret < 0)
		dev_err(dev, "failed to write REN_SLG5BR_LDO2_VSEL, vsel:%x ret:%x\n", vsel, ret);
}

static void set_ldo3(struct device *dev)
{
	int ret;
	int vsel;
	struct i2c_client *client = to_i2c_client(dev);
	struct renslgr_data *pdata = (struct renslgr_data *)
						i2c_get_clientdata(client);

	vsel = ((pdata->ldo3_mv - 1200) / 10);
	ret = regmap_write(pdata->regmap, REN_SLG5BR_LDO3_VSEL, vsel);
	if (ret < 0)
		dev_err(dev, "failed to write REN_SLG5BR_LDO3_VSEL, vsel:%x ret:%x\n", vsel, ret);
}

static void set_ldo4(struct device *dev)
{
	int ret;
	int vsel;
	struct i2c_client *client = to_i2c_client(dev);
	struct renslgr_data *pdata = (struct renslgr_data *)
						i2c_get_clientdata(client);

	vsel = ((pdata->ldo4_mv - 1200) / 10);
	ret = regmap_write(pdata->regmap, REN_SLG5BR_LDO4_VSEL, vsel);
	if (ret < 0)
		dev_err(dev, "failed to write REN_SLG5BR_LDO4_VSEL, vsel:%x ret:%x\n", vsel, ret);
}

static void set_ldo5(struct device *dev)
{
	int ret;
	int vsel;
	struct i2c_client *client = to_i2c_client(dev);
	struct renslgr_data *pdata = (struct renslgr_data *)
						i2c_get_clientdata(client);

	vsel = ((pdata->ldo5_mv - 1200) / 10);
	ret = regmap_write(pdata->regmap, REN_SLG5BR_LDO5_VSEL, vsel);
	if (ret < 0)
		dev_err(dev, "failed to write REN_SLG5BR_LDO5_VSEL, vsel:%x ret:%x\n", vsel, ret);
}

static void set_ldo6(struct device *dev)
{
	int ret;
	int vsel;
	struct i2c_client *client = to_i2c_client(dev);
	struct renslgr_data *pdata = (struct renslgr_data *)
						i2c_get_clientdata(client);

	vsel = ((pdata->ldo6_mv - 400) / 5);
	ret = regmap_write(pdata->regmap, REN_SLG5BR_LDO6_VSEL, vsel);
	if (ret < 0)
		dev_err(dev, "failed to write REN_SLG5BR_LDO6_VSEL, vsel:%x ret:%x\n", vsel, ret);
}

static void set_ldo7(struct device *dev)
{
	int ret;
	int vsel;
	struct i2c_client *client = to_i2c_client(dev);
	struct renslgr_data *pdata = (struct renslgr_data *)
						i2c_get_clientdata(client);

	vsel = ((pdata->ldo7_mv - 400) / 5);
	ret = regmap_write(pdata->regmap, REN_SLG5BR_LDO7_VSEL, vsel);
	if (ret < 0)
		dev_err(dev, "failed to write REN_SLG5BR_LDO7_VSEL, vsel:%x ret:%x\n", vsel, ret);
}

static void set_ldo8(struct device *dev)
{
	int ret;
	int vsel;
	struct i2c_client *client = to_i2c_client(dev);
	struct renslgr_data *pdata = (struct renslgr_data *)
						i2c_get_clientdata(client);

	vsel = ((pdata->ldo8_mv - 400) / 5);
	ret = regmap_write(pdata->regmap, REN_SLG5BR_LDO8_VSEL, vsel);
	if (ret < 0)
		dev_err(dev, "failed to write REN_SLG5BR_LDO8_VSEL, vsel:%x ret:%x\n", vsel, ret);
}

static int renslgr_parse_dt(struct renslgr_data *data)
{
	int ret = 0;
	struct device *dev = data->dev;
	struct device_node *np = dev->of_node;

	ret = of_property_read_u32(np, "ldo1-mv", &data->ldo1_mv);
	if (ret) {
		dev_err(dev, "ldo1-mv missing\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ldo2-mv", &data->ldo2_mv);
	if (ret) {
		dev_err(dev, "ldo2-mv missing\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ldo3-mv", &data->ldo3_mv);
	if (ret) {
		dev_err(dev, "ldo3-mv missing\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ldo4-mv", &data->ldo4_mv);
	if (ret) {
		dev_err(dev, "ldo4-mv missing\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ldo5-mv", &data->ldo5_mv);
	if (ret) {
		dev_err(dev, "ldo5-mv missing\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ldo6-mv", &data->ldo6_mv);
	if (ret) {
		dev_err(dev, "ldo6-mv missing\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ldo7-mv", &data->ldo7_mv);
	if (ret) {
		dev_err(dev, "ldo7-mv missing\n");
		return ret;
	}

	ret = of_property_read_u32(np, "ldo8-mv", &data->ldo8_mv);
	if (ret) {
		dev_err(dev, "ldo8-mv missing\n");
		return ret;
	}

	return ret;
}

int renslgr_regulator_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct renslgr_data *pdata;
	int ret = 0;

	pdata = devm_kzalloc(dev, sizeof(struct renslgr_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->dev = dev;
	pdata->client = client;
	i2c_set_clientdata(client, pdata);

	pinctrl_pm_select_default_state(dev);

	ret = renslgr_parse_dt(pdata);
	if (ret < 0) {
		dev_err(pdata->dev, "failed to parse DT Configs: %d\n", ret);
		return ret;
	}

	pdata->regmap = devm_regmap_init_i2c(client, &renslgr_regmap_config);
	if (IS_ERR(pdata->regmap)) {
		ret = PTR_ERR(pdata->regmap);
		dev_err(pdata->dev, "failed to initialize regmap: %d\n", ret);
		return ret;
	}

	set_ldo_control(dev, true);
	set_ldo1(dev);
	set_ldo2(dev);
	set_ldo3(dev);
	set_ldo4(dev);
	set_ldo5(dev);
	set_ldo6(dev);
	set_ldo7(dev);
	set_ldo8(dev);
	set_ldo_control(dev, true);

	return ret;
};

static const struct of_device_id renslgr_of_match_table[] = {
	{ .compatible = "renesas,slg5b-right", },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, renslgr_of_match_table);

static struct i2c_driver renslgr_regulator_driver = {
	.driver = {
		.name = "slg5b-right",
		.of_match_table = renslgr_of_match_table,
	},
	.probe = renslgr_regulator_probe,
	/*.id_table = renslgr_regulator_id,*/
};

module_i2c_driver(renslgr_regulator_driver);

MODULE_DESCRIPTION("Renesas SLG5BV49293 regulator driver");
MODULE_LICENSE("GPL");
