// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include "geni-pkvm.h"
#include <asm/kvm_pkvm_module.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kvm_host.h>
#include <linux/module.h>
#include <linux/of_address.h>

#ifndef MODULE
BUILD_BUG("pKVM GENI UART must be compiled as a module");
#endif

static int __init geni_nvhe_init(void)
{
	struct resource res;
	unsigned long token;
	int ret = -EINVAL;

	struct device_node *np = of_find_compatible_node(NULL, NULL,
						"qcom,geni-debug-uart");
	if (np)
		ret = of_address_to_resource(np, 0, &res);

	of_node_put(np);

	if (ret) {
		kvm_err("qcom GENI UART module init - compatible error\n");
		return ret;
	}

	/*
	 * Propagate the UART start address to the EL2 part, prior to loading.
	 * This initialization is only executed once.
	 */
	kvm_nvhe_sym(uart_start) = res.start;

	ret = pkvm_load_el2_module(kvm_nvhe_sym(geni_hyp_init), &token);
	if (ret) {
		kvm_err("qcom GENI UART module - failed EL2 module load\n");
		return ret;
	}

	kvm_info("qcom GENI UART module initialized successfully\n");
	return 0;
}
module_init(geni_nvhe_init);

MODULE_DESCRIPTION("qcom GENI UART pKVM module");
MODULE_LICENSE("GPL");
