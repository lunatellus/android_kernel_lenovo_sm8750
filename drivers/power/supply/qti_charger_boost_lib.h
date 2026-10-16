/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifdef CONFIG_QTI_CHARGER_NFC_VREG
#ifndef __QTI_CHARGER_BOOST_LIB_H
#define __QTI_CHARGER_BOOST_LIB_H

#include <linux/regulator/driver.h>
#include <linux/regulator/debug-regulator.h>
#include <linux/soc/qcom/qti_pmic_glink.h>

#define BC_CHG_CTRL_EN_W_BOOST		0x4E

#define DEFAULT_NFC_MIN_VOLTAGE		4100000
#define DEFAULT_NFC_MAX_VOLTAGE		5600000

enum boost_type {
	BOOST_NFC = 1,
};

enum boost_vote {
	BOOST_DISABLE,
	BOOST_ENABLE,
};

enum boost_mode {
	OFF_MODE,
	BURST_MODE,
	PWM_FF_MODE,
	HRM_MODE,
};

struct bharger_regulator {
	struct regulator_dev	*rdev;
	struct regulator_desc	rdesc;
	u32 min_uv;
	u32 enable;
};

struct battery_enable_w_boost_req_msg {
	struct pmic_glink_hdr	hdr;
	u32 client;
	u32 enable;
	u32 gpio_override;
	u32 mode;
	u32 boost_microvolt;
};

int nfc_regulator_en(struct bharger_regulator *nfc_vreg, bool enable);
int nfc_init_regulator(struct device *dev, struct bharger_regulator *nfc_vreg);
#endif
#endif
