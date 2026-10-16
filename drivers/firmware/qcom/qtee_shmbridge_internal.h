/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __QCOM_QTEE_SHM_BRIDGE_INT_H_
#define __QCOM_QTEE_SHM_BRIDGE_INT_H_

#ifdef CONFIG_QTEE_SHM_BRIDGE
int qtee_shmbridge_driver_init(void);
void qtee_shmbridge_driver_exit(void);

int qtee_shmbridge_pm_freeze(void);
int qtee_shmbridge_pm_restore(void);
int qtee_shmbridge_pm_thaw(void);

#else
static inline int qtee_shmbridge_driver_init(void)
{
	return 0;
}

static inline void qtee_shmbridge_driver_exit(void) {}

static inline int qtee_shmbridge_pm_freeze(void)
{
	return 0;
}

static inline int qtee_shmbridge_pm_restore(void)
{
	return 0;
}

static inline int qtee_shmbridge_pm_thaw(void)
{
	return 0;
}
#endif

#define SCM_SVC_RTIC                                0x19
#define TZ_HLOS_NOTIFY_CORE_KERNEL_BOOTUP           0x7
int scm_mem_protection_init_do(void);
#endif
