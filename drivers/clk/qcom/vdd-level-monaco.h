/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __DRIVERS_CLK_QCOM_VDD_LEVEL_H
#define __DRIVERS_CLK_QCOM_VDD_LEVEL_H

#include <linux/regulator/consumer.h>
#include <dt-bindings/regulator/qcom,rpm-smd-regulator.h>

enum vdd_levels {
	VDD_NONE,
	VDD_LOWER_D1,		/* MIN SVS */
	VDD_MIN = VDD_LOWER_D1,
	VDD_LOWER,		/* SVS2 */
	VDD_LOWER_L1,		/* LSVS L1 */
	VDD_LOW,		/* SVS */
	VDD_LOW_L0,		/* SVSL0 */
	VDD_LOW_L1,		/* SVSL1 */
	VDD_LOW_L2,		/* SVSL2 */
	VDD_NOMINAL,		/* NOM */
	VDD_NOMINAL_L1,		/* NOM L1 */
	VDD_HIGH,
	VDD_HIGH_L1,		/* TURBO L1*/
	VDD_HIGH_L2,		/* TURBO L2*/
	VDD_NUM,
};

static int vdd_corner[] = {
	[VDD_NONE]    = 0,
	[VDD_LOWER_D1]     = RPM_SMD_REGULATOR_LEVEL_MIN_SVS,
	[VDD_LOWER]   = RPM_SMD_REGULATOR_LEVEL_LOW_SVS,
	[VDD_LOWER_L1] = RPM_SMD_REGULATOR_LEVEL_LOW_SVS_L1,
	[VDD_LOW]     = RPM_SMD_REGULATOR_LEVEL_SVS,
	[VDD_LOW_L0]  = RPM_SMD_REGULATOR_LEVEL_SVS_L0,
	[VDD_LOW_L1]  = RPM_SMD_REGULATOR_LEVEL_SVS_PLUS,
	[VDD_LOW_L2]  = RPM_SMD_REGULATOR_LEVEL_SVS_L2,
	[VDD_NOMINAL] = RPM_SMD_REGULATOR_LEVEL_NOM,
	[VDD_NOMINAL_L1] = RPM_SMD_REGULATOR_LEVEL_NOM_PLUS,
	[VDD_HIGH]    = RPM_SMD_REGULATOR_LEVEL_TURBO,
	[VDD_HIGH_L1]    = RPM_SMD_REGULATOR_LEVEL_TURBO_NO_CPR,
	[VDD_HIGH_L2]    = RPM_SMD_REGULATOR_LEVEL_TURBO_L2,
};

#endif
