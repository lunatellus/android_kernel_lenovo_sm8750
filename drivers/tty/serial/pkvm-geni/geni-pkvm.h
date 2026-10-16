/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#ifndef __GENI_PKVM_H__
#define __GENI_PKVM_H__

#include <asm/kvm_pkvm_module.h>
#include <linux/types.h>

/* Shared declarations between host and hypervisor components */
extern resource_size_t __kvm_nvhe_uart_start;
int __kvm_nvhe_geni_hyp_init(const struct pkvm_module_ops *ops);

#endif /* __GENI_PKVM_H__ */
