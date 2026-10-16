/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#ifndef PCIE_EOM_HAMOA_PHY_REG_H
#define PCIE_EOM_HAMOA_PHY_REG_H

#define NEGATIVE_SEQUENCE              0
#define POSITIVE_SEQUENCE              1
#define EOM_TEST_TIME_DEFAULT          1
#define EYE_HEIGHT_STEP                3
#define MAX_NUM_LANES                  8
#define MAX_EYE_WIDTH                  64
#define MAX_VERTICAL_THRESHOLD         128
#define MAX_EYE_HEIGHT                 128
#define EYE_WIDTH_STEP_DIVISOR         32
#define EOM_REG_WRITE_DELAY            100        /* in nano seconds */
#define EOM_TEST_TIME_MAX              1000
#define QSERDES_OFFSET_SIZE            0x800
#define HAMOA_PHY_B_LANE_START         2
#define HAMOA_PHY_MAX_RC_INSTANCES     8

#define HAMOA_PHY_REG_ADDR(base_reg, lanenum)	(base_reg + LANE_SIZE(lanenum))
#define LANE_SIZE(lanenum)			(lanenum * QSERDES_OFFSET_SIZE)
#define MAX_EYE_WIDTH_UI			(MAX_EYE_WIDTH / EYE_WIDTH_STEP_DIVISOR)
#define MAX_EYE_HEIGHT_MV			(MAX_EYE_HEIGHT * EYE_HEIGHT_STEP)

#define PCIE_PHY_QSERDES_TX0_RESET_GEN_MUXES        0x0A8
#define PCIE_PHY_QSERDES_RX0_AUX_CONTROL            0x238
#define PCIE_PHY_QSERDES_RX0_AUXDATA_TB             0x23C
#define PCIE_PHY_QSERDES_RX0_RCLK_AUXDATA_SEL       0x240
#define PCIE_PHY_QSERDES_RX0_EOM_CTRL1              0x244
#define PCIE_PHY_QSERDES_RX0_EOM_CTRL2              0x248
#define PCIE_PHY_QSERDES_RX0_CDR_RESET_OVERRIDE     0x35C
#define PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL2          0x3BC
#define PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3          0x3C0
#define PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL4          0x3C4
#define PCIE_PHY_QSERDES_RX0_RX_MARG_VERTICAL_CTRL  0x3E0
#define PCIE_PHY_QSERDES_RX0_IA_ERROR_COUNTER_LOW   0x470
#define PCIE_PHY_QSERDES_RX0_IA_ERROR_COUNTER_HIGH  0x474

#endif
