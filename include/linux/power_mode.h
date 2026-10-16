/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _LINUX_POWER_MODE_H
#define _LINUX_POWER_MODE_H

#include <linux/string.h>

enum qcom_power_state {
	PM_S2IDLE = 0,
	PM_S2R,
	PM_DEEPSLEEP,
};

extern char power_mode_buf[64];

static inline int qcom_get_power_mode(void)
{
	if (strcmp(power_mode_buf, "S2R") == 0)
		return PM_S2R;
	else if (strcmp(power_mode_buf, "DEEPSLEEP") == 0)
		return PM_DEEPSLEEP;
	else
		return PM_S2IDLE;
}
#endif
