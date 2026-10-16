/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>

#ifndef _CPU_PHYS_LOG_MAP_H
#define _CPU_PHYS_LOG_MAP_H

#if IS_ENABLED(CONFIG_QCOM_CPU_PHYS_LOG_MAP)
extern int cpu_logical_to_phys(int cpu);
extern int cpu_phys_to_logical(int cpu);
#else
static inline int cpu_logical_to_phys(int cpu)
{
	return cpu;
}
static inline int cpu_phys_to_logical(int cpu)
{
	return cpu;
}
#endif

#endif /* _CPU_PHYS_LOG_MAP_H */
