/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _LINUX_INPUT_CAP1296_H_
#define _LINUX_INPUT_CAP1296_H_

#define CAP1296_ADDRESS 0x28
#define CAP1296_ID      0x69  // default 0x69 in datasheet
#define CAP_NUM_WRITABLE_REGISTERS          27
#define CAP_MAINCONTROL_REG         0x00
#define CAP_GENERAL_STATUS_REG      0x02
#define CAP_SENSORINPUT_STATUS_REG  0x03
#define CAP_NOISE_FLAG_STATUS_REG   0x0a
#define CAP_KEY_CHANNAL1    1
#define CAP_KEY_CHANNAL2    2
#define CAP_KEY_CHANNAL3    8
#define CAP_KEY_CHANNAL4    16
#define CAP_KEY_CHANNAL5    32
#define CAP_KEY_MUTE_FLAG_STATUS_REG    0x01
#define CAP_KEY_UP_FLAG_STATUS_REG      0x02
#define CAP_KEY_POWER_FLAG_STATUS_REG   0x03
#define CAP_KEY_DOWN_FLAG_STATUS_REG    0x04
#define CAP_KEY_F11_FLAG_STATUS_REG    0x05

const uint8_t CAP_defaultConfiguration[CAP_NUM_WRITABLE_REGISTERS][2] = {
	// Address Value
	{ 0x00,   0x10 },      // Main Control
	{ 0x1F,   0x2F },      // Sensitivity Control
	{ 0x20,   0x20 },      // General Configuration
	{ 0x21,   0x3F },      // Sensor Input Enable
	{ 0x22,   0xA4 },      // Sensor Input Configuration
	{ 0x23,   0x07 },      // Sensor Input Configuration 2
	{ 0x24,   0x39 },      // Averaging and Sampling Config
	{ 0x26,   0x03 },      // Calibration Activate
	{ 0x27,   0x3F },      // Interrupt Enable
	{ 0x28,   0x3F },      // Repeat Rate Enable
	{ 0x29,   0x03 },      // Signal Guard Disable
	{ 0x2A,   0x00 },      // Multiple Touch Configuration
	{ 0x2B,   0x00 },      // Multiple Touch Pattern Config
	{ 0x2D,   0x3F },      // Multiple Touch Pattern
	{ 0x2F,   0x8A },      // Recalibration Configuration
	{ 0x30,   0x40 },      // Sensor Input 1 Threshold
	{ 0x31,   0x40 },      // Sensor Input 2 Threshold
	{ 0x32,   0x30 },      // Sensor Input 3 Threshold
	{ 0x33,   0x40 },      // Sensor Input 4 Threshold
	{ 0x34,   0x40 },      // Sensor Input 5 Threshold
	{ 0x35,   0x40 },      // Sensor Input 6 Threshold
	{ 0x38,   0x01 },      // Sensor Input Noise Threshold
	{ 0x40,   0x02 },      // Standby Channel
	{ 0x41,   0x39 },      // Standby Configuration
	{ 0x42,   0x02 },      // Standby Sensitivity
	{ 0x43,   0x40 },      // Standby Threshold
	{ 0x44,   0x40 },      // Configuration 2
};

#endif  // _LINUX_INPUT_CAP1296_H_
