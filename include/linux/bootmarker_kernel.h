/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef __BOOTMARKER_KERNEL_H_
#define __BOOTMARKER_KERNEL_H_

#include <linux/types.h>

struct bootmarker_drv_ops {
	int (*bootmarker_place_marker)(const char *name);
};

#if IS_ENABLED(CONFIG_BOOTMARKER_PROXY)
int provide_bootmarker_kernel_fun_ops(const struct bootmarker_drv_ops *ops);
int bootmarker_place_marker(const char *name);
#else
static inline int provide_bootmarker_kernel_fun_ops(const struct bootmarker_drv_ops *ops)
{
	return 0;
}
static inline int bootmarker_place_marker(const char *name)
{
	return 0;
}
#endif /*CONFIG_BOOTMARKER_PROXY*/
#endif /* __BOOTMARKER_KERNEL_H_ */
