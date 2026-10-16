/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __UAPI_LINUX_MSM_GENI_SERIAL_H
#define __UAPI_LINUX_MSM_GENI_SERIAL_H

/* IOCTLS used by BT clients to control UART power state */

#define MSM_GENI_SERIAL_TIOCFAULT	0x54EC  /* Uart fault */
#define MSM_GENI_SERIAL_TIOCPMGET	0x54ED	/* PM get */
#define MSM_GENI_SERIAL_TIOCPMPUT	0x54EE	/* PM put */
#define MSM_GENI_SERIAL_TIOCPMACT	0x54EF	/* PM is active */

/* IOCTLS used for enabling or disabling timestamp on UART */

/**
 * MSM_GENI_SERIAL_TIOCTSSET - Enable timestamp functionality for UART transfers
 * This IOCTL enables the collection of timestamp data during UART transfers,
 * which can be used for performance analysis and debugging.
 */
#define MSM_GENI_SERIAL_TIOCTSSET	0x201	/* Timestamp enabled */

/**
 * MSM_GENI_SERIAL_TIOCTSCLR - Disable timestamp functionality for UART transfers
 * This IOCTL disables the collection of timestamp data during UART transfers.
 */
#define MSM_GENI_SERIAL_TIOCTSCLR	0x200	/* Timestamp disabled */

#endif /* __UAPI_LINUX_MSM_GENI_SERIAL_H */
