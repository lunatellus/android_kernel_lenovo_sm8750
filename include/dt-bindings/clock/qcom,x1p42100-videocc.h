/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "qcom,sm8650-videocc.h"
#ifndef _DT_BINDINGS_CLK_QCOM_VIDEO_CC_X1P42100_H
#define _DT_BINDINGS_CLK_QCOM_VIDEO_CC_X1P42100_H

/* VIDEO_CC clocks */
#define VIDEO_CC_AHB_CLK					17
#define VIDEO_CC_AHB_CLK_SRC					18
#define VIDEO_CC_MVS0_BSE_CLK					19
#define VIDEO_CC_MVS0_BSE_CLK_SRC				20
#define VIDEO_CC_MVS0_BSE_DIV4_DIV_CLK_SRC			21
#define VIDEO_CC_SLEEP_CLK					22
#define VIDEO_CC_SLEEP_CLK_SRC					23
#define VIDEO_CC_XO_CLK						24

/* VIDEO_CC resets */
#define VIDEO_CC_INTERFACE_AHB_BCR				8
#define VIDEO_CC_MVS0_BSE_BCR					9
#define VIDEO_CC_MVS0_BCR					10
#define VIDEO_CC_MVS0C_BCR					11
#define VIDEO_CC_MVS1_BCR					12
#define VIDEO_CC_MVS1C_BCR					13

#endif
