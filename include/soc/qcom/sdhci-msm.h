/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _DRIVERS_MMC_SDHCI_MSM_H
#define _DRIVERS_MMC_SDHCI_MSM_H

#include <linux/mmc/sdio_func.h>

void sdhci_msm_toggle_dat1_gpio(struct sdio_func *func);

#endif
