/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_CAM_CC_PIKACHU_H
#define _DT_BINDINGS_CLK_QCOM_CAM_CC_PIKACHU_H

/* CAM_CC clocks */
#define CAM_CC_BPS_AHB_CLK					0
#define CAM_CC_BPS_CLK						1
#define CAM_CC_BPS_CLK_SRC					2
#define CAM_CC_BPS_FAST_AHB_CLK					3
#define CAM_CC_CAMNOC_AHB_CLK					4
#define CAM_CC_CAMNOC_AXI_NRT_CLK				5
#define CAM_CC_CAMNOC_AXI_RT_CLK				6
#define CAM_CC_CAMNOC_AXI_RT_CLK_SRC				7
#define CAM_CC_CAMNOC_DCD_XO_CLK				8
#define CAM_CC_CAMNOC_XO_CLK					9
#define CAM_CC_CCI_0_CLK					10
#define CAM_CC_CCI_0_CLK_SRC					11
#define CAM_CC_CCI_1_CLK					12
#define CAM_CC_CCI_1_CLK_SRC					13
#define CAM_CC_CORE_AHB_CLK					14
#define CAM_CC_CPAS_AHB_CLK					15
#define CAM_CC_CPAS_BPS_CLK					16
#define CAM_CC_CPAS_FAST_AHB_CLK				17
#define CAM_CC_CPAS_IFE_0_CLK					18
#define CAM_CC_CPAS_IFE_LITE_0_CLK				19
#define CAM_CC_CPAS_IPE_NPS_CLK					20
#define CAM_CC_CPHY_RX_CLK_SRC					21
#define CAM_CC_CSI2PHYTIMER_CLK					22
#define CAM_CC_CSI2PHYTIMER_CLK_SRC				23
#define CAM_CC_CSI4PHYTIMER_CLK					24
#define CAM_CC_CSI4PHYTIMER_CLK_SRC				25
#define CAM_CC_CSID_CLK						26
#define CAM_CC_CSID_CLK_SRC					27
#define CAM_CC_CSID_CSIPHY_RX_CLK				28
#define CAM_CC_CSIPHY2_CLK					29
#define CAM_CC_CSIPHY4_CLK					30
#define CAM_CC_DRV_AHB_CLK					31
#define CAM_CC_DRV_XO_CLK					32
#define CAM_CC_FAST_AHB_CLK_SRC					33
#define CAM_CC_GDSC_CLK						34
#define CAM_CC_ICP_AHB_CLK					35
#define CAM_CC_ICP_CLK						36
#define CAM_CC_ICP_CLK_SRC					37
#define CAM_CC_IFE_0_CLK					38
#define CAM_CC_IFE_0_CLK_SRC					39
#define CAM_CC_IFE_0_FAST_AHB_CLK				40
#define CAM_CC_IFE_LITE_0_AHB_CLK				41
#define CAM_CC_IFE_LITE_0_CLK					42
#define CAM_CC_IFE_LITE_0_CLK_SRC				43
#define CAM_CC_IFE_LITE_0_CPHY_RX_CLK				44
#define CAM_CC_IFE_LITE_0_CSID_CLK				45
#define CAM_CC_IFE_LITE_0_CSID_CLK_SRC				46
#define CAM_CC_IPE_NPS_AHB_CLK					47
#define CAM_CC_IPE_NPS_CLK					48
#define CAM_CC_IPE_NPS_CLK_SRC					49
#define CAM_CC_IPE_NPS_FAST_AHB_CLK				50
#define CAM_CC_IPE_PPS_CLK					51
#define CAM_CC_IPE_PPS_FAST_AHB_CLK				52
#define CAM_CC_JPEG_CLK						53
#define CAM_CC_JPEG_CLK_SRC					54
#define CAM_CC_MCLK0_CLK					55
#define CAM_CC_MCLK0_CLK_SRC					56
#define CAM_CC_MCLK1_CLK					57
#define CAM_CC_MCLK1_CLK_SRC					58
#define CAM_CC_MCLK2_CLK					59
#define CAM_CC_MCLK2_CLK_SRC					60
#define CAM_CC_MCLK4_CLK					61
#define CAM_CC_MCLK4_CLK_SRC					62
#define CAM_CC_PLL0						63
#define CAM_CC_PLL0_OUT_EVEN					64
#define CAM_CC_PLL0_OUT_ODD					65
#define CAM_CC_PLL1						66
#define CAM_CC_PLL1_OUT_EVEN					67
#define CAM_CC_PLL2						68
#define CAM_CC_PLL3						69
#define CAM_CC_PLL3_OUT_EVEN					70
#define CAM_CC_PLL5						71
#define CAM_CC_PLL5_OUT_EVEN					72
#define CAM_CC_PLL6						73
#define CAM_CC_PLL6_OUT_EVEN					74
#define CAM_CC_PLL6_OUT_ODD					75
#define CAM_CC_QDSS_DEBUG_CLK					76
#define CAM_CC_QDSS_DEBUG_CLK_SRC				77
#define CAM_CC_QDSS_DEBUG_XO_CLK				78
#define CAM_CC_SLEEP_CLK					79
#define CAM_CC_SLEEP_CLK_SRC					80
#define CAM_CC_SLOW_AHB_CLK_SRC					81
#define CAM_CC_XO_CLK_SRC					82

/* CAM_CC power domains */
#define CAM_CC_BPS_GDSC						0
#define CAM_CC_IFE_0_GDSC					1
#define CAM_CC_IFE_LITE_0_GDSC					2
#define CAM_CC_IPE_0_GDSC					3
#define CAM_CC_TITAN_TOP_GDSC					4

/* CAM_CC resets */
#define CAM_CC_BPS_BCR						0
#define CAM_CC_CAMNOC_BCR					1
#define CAM_CC_DRV_BCR						2
#define CAM_CC_ICP_BCR						3
#define CAM_CC_IFE_0_BCR					4
#define CAM_CC_IFE_LITE_0_BCR					5
#define CAM_CC_IPE_0_BCR					6
#define CAM_CC_QDSS_DEBUG_BCR					7
#define CAM_CC_TITAN_TOP_BCR					8

#endif
