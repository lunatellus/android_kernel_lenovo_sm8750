// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifdef CONFIG_QTI_CHARGER_NFC_VREG
#include <linux/regulator/driver.h>
#include <linux/regulator/debug-regulator.h>
#include "qti_charger_boost_lib.h"

static int nfc_regulator_enable(struct regulator_dev *rdev)
{
	struct bharger_regulator *nfc_vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = nfc_regulator_en(nfc_vreg, true);
	if (rc < 0)
		pr_err("Failed to enable nfc regulator rc=%d\n", rc);
	else
		nfc_vreg->enable = BOOST_ENABLE;

	return rc;
}

static int nfc_regulator_disable(struct regulator_dev *rdev)
{
	struct bharger_regulator *nfc_vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = nfc_regulator_en(nfc_vreg, false);
	if (rc < 0)
		pr_err("failed to disable nfc regulator rc=%d\n", rc);
	else
		nfc_vreg->enable = BOOST_DISABLE;

	return rc;
}

static int nfc_regulator_is_enable(struct regulator_dev *rdev)
{
	struct bharger_regulator *nfc_vreg = rdev_get_drvdata(rdev);

	return nfc_vreg->enable;
}

static int nfc_regulator_set_voltage(struct regulator_dev *rdev,
				int min_uv, int max_uv, unsigned int *selector)
{
	struct bharger_regulator *nfc_vreg = rdev_get_drvdata(rdev);

	if (max_uv > DEFAULT_NFC_MAX_VOLTAGE)
		max_uv = DEFAULT_NFC_MAX_VOLTAGE;

	if (min_uv < DEFAULT_NFC_MIN_VOLTAGE)
		min_uv = DEFAULT_NFC_MIN_VOLTAGE;

	if (max_uv < min_uv) {
		pr_err("NFC boost set voltage failed! invalid voltage request\n");
		return -EINVAL;
	}

	nfc_vreg->min_uv = min_uv;

	if (nfc_vreg->enable)
		return nfc_regulator_enable(rdev);

	return 0;
}

static int nfc_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct bharger_regulator *nfc_vreg = rdev_get_drvdata(rdev);

	return nfc_vreg->min_uv;
}

static const struct regulator_ops nfc_reg_ops = {
	.enable = nfc_regulator_enable,
	.disable = nfc_regulator_disable,
	.is_enabled = nfc_regulator_is_enable,
	.set_voltage = nfc_regulator_set_voltage,
	.get_voltage = nfc_regulator_get_voltage,
};

int nfc_init_regulator(struct device *dev, struct bharger_regulator *nfc_vreg)
{
	struct regulator_config cfg = {};
	int rc;

	cfg.dev = dev;
	cfg.driver_data = nfc_vreg;

	nfc_vreg->rdesc.owner = THIS_MODULE;
	nfc_vreg->rdesc.type = REGULATOR_VOLTAGE;
	nfc_vreg->rdesc.ops = &nfc_reg_ops;
	nfc_vreg->rdesc.of_match = "qcom,nfc-vreg";
	nfc_vreg->rdesc.name = "qcom,nfc-vreg";
	nfc_vreg->min_uv = -ENOTRECOVERABLE;
	nfc_vreg->enable = BOOST_DISABLE;

	nfc_vreg->rdev = devm_regulator_register(dev, &nfc_vreg->rdesc, &cfg);
	if (IS_ERR(nfc_vreg->rdev)) {
		rc = PTR_ERR(nfc_vreg->rdev);
		nfc_vreg->rdev = NULL;
		if (rc != -EPROBE_DEFER)
			dev_err(dev, "Couldn't register NFC regulator %d\n",
					rc);
		return rc;
	}

	rc = devm_regulator_debug_register(dev, nfc_vreg->rdev);
	if (rc)
		dev_err(dev, "failed to register debug regulator, rc=%d\n",
			rc);

	return 0;
}
#endif
