/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_GPU_CC_MALABAR_H
#define _DT_BINDINGS_CLK_QCOM_GPU_CC_MALABAR_H

/* GPU_CC clocks */
#define GPU_CC_PLL0						0
#define GPU_CC_PLL0_OUT_EVEN					1
#define GPU_CC_PLL1						2
#define GPU_CC_AHB_CLK						3
#define GPU_CC_CRC_AHB_CLK					4
#define GPU_CC_CX_ACCU_SHIFT_CLK				5
#define GPU_CC_CX_GFX3D_CLK					6
#define GPU_CC_CX_GMU_CLK					7
#define GPU_CC_CXO_AON_CLK					8
#define GPU_CC_CXO_CLK						9
#define GPU_CC_DEMET_CLK					10
#define GPU_CC_DEMET_DIV_CLK_SRC				11
#define GPU_CC_GMU_CLK_SRC					12
#define GPU_CC_GPU_SMMU_VOTE_CLK				13
#define GPU_CC_GX_ACCU_SHIFT_CLK				14
#define GPU_CC_GX_CXO_CLK					15
#define GPU_CC_GX_GFX3D_CLK					16
#define GPU_CC_GX_GFX3D_CLK_SRC					17
#define GPU_CC_GX_GMU_CLK					18
#define GPU_CC_HLOS1_VOTE_GPU_SMMU_CLK				19
#define GPU_CC_MEMNOC_GFX_CLK					20
#define GPU_CC_RBCPR_AHB_CLK					21
#define GPU_CC_RBCPR_CLK					22
#define GPU_CC_RBCPR_CLK_SRC					23
#define GPU_CC_SLEEP_CLK					24
#define GPU_CC_XO_CLK_SRC					25

/* GPU_CC power domains */
#define GPU_CC_CX_GDSC						0
#define GPU_CC_CX_SMMU_GDSC					1
#define GPU_CC_CX_GMU_GDSC					2
#define GPU_CC_GX_GDSC						3

/* GPU_CC resets */
#define GPU_CC_CX_BCR						0
#define GPU_CC_GFX3D_AON_BCR					1
#define GPU_CC_GMU_BCR						2
#define GPU_CC_GX_BCR						3
#define GPU_CC_RBCPR_BCR					4
#define GPU_CC_XO_BCR						5
#define GPU_CC_FREQUENCY_LIMITER_IRQ_CLEAR			6

#endif
