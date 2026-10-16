// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include <asm/kvm_pkvm_module.h>
#include <asm/kvm_mmu.h>
#include <linux/init.h>
#include <linux/kvm_host.h>
#include <linux/module.h>

/****************************************
 * QCOM pKVM SMC filter module - EL1
 */

#ifndef MODULE
BUILD_BUG("pKVM SMC filter must be compiled as a module");
#endif

int kvm_nvhe_sym(smc_filter_hyp_init)(const struct pkvm_module_ops *ops);

static int __init smc_filter_nvhe_init(void)
{
	unsigned long token;
	int ret;

	ret = pkvm_load_el2_module(kvm_nvhe_sym(smc_filter_hyp_init), &token);
	if (ret) {
		kvm_err("qcom SMC filter module init failed\n");
		return ret;
	}

	kvm_info("qcom SMC filter module initialized successfully\n");
	return 0;
}
module_init(smc_filter_nvhe_init);

MODULE_DESCRIPTION("qcom pKVM SMC filter module");
MODULE_LICENSE("GPL");
