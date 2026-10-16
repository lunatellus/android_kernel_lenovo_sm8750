/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#ifndef USB_EOM_REG_H
#define USB_EOM_REG_H

#define EYE_HEIGHT_STEP         3
#define MAX_VERTICAL_THRESHOLD  8
#define MAX_EYE_HEIGHT          8
#define MAX_EYE_HEIGHT_MV       (MAX_EYE_HEIGHT * EYE_HEIGHT_STEP)
#define EYE_WIDTH_STEP          (1 / 32)
#define MAX_EYE_WIDTH           64
#define MAX_EYE_WIDTH_UI        (MAX_EYE_WIDTH * EYE_WIDTH_STEP)
#define MAX_NUM_LANES           8
#define EOM_TEST_TIME_DEFAULT   1
#define EOM_TEST_TIME_MAX       1000
#define MAX_T_COARSE            8

#define USB_PHY_LANE_TYPE_CONFIG_OFFSET 0x24
#define USB3_QSERDES_LANE_A_BASE_OFFSET 0x1400
#define LANE_B_OFFSET 0x400

#define USB3_QSERDES_TX_LANE_MODE_1_OFFSET              0x84
#define USB3_QSERDES_TX_LANE_MODE_5_OFFSET              0x94
#define USB3_QSERDES_RX_AUX_CONTROL_OFFSET              0x25C
#define USB3_QSERDES_RX_DFE_4_OFFSET                    0x2C0
#define USB3_QSERDES_RX_AUX_DATA_TCOARSE_TFINE_OFFSET   0x260
#define USB3_QSERDES_RX_RCLK_AUXDATA_SEL_OFFSET         0x264
#define USB3_QSERDES_RX_CDR_RESET_OVERRIDE_OFFSET       0x330
#define USB3_QSERDES_TX_RESET_GEN_OFFSET                0xB8
#define USB3_QSERDES_RX_VTH_CODE_OFFSET                 0x3B0
#define USB3_QSERDES_TX_IA_ERROR_COUNTER_LOW_OFFSET     0x11C
#define USB3_QSERDES_TX_IA_ERROR_COUNTER_HIGH_OFFSET    0x120

#endif
