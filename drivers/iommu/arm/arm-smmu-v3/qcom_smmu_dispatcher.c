// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include <asm/arm-smmu-v3-common.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_pkvm.h>

#include <linux/atomic.h>
#include <linux/moduleparam.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/kvm_host.h>
#include "smmuv2_nesting.h"
#include "arm_smmu_v3_nested.h"

#ifdef MODULE
static unsigned long                   pkvm_module_token;

#define ksym_ref_addr_nvhe(x) \
	((typeof(kvm_nvhe_sym(x))*)(pkvm_el2_mod_va(&kvm_nvhe_sym(x), pkvm_module_token)))
#else
#define ksym_ref_addr_nvhe(x) \
	((typeof(kvm_nvhe_sym(x))*)(kern_hyp_va(lm_alias(&kvm_nvhe_sym(x)))))
#endif

int kvm_nvhe_sym(qcom_smmu_nesting_init_module)(const struct pkvm_module_ops *ops);
extern struct kvm_iommu_ops kvm_nvhe_sym(qcom_smmu_hyp_nesting_ops);

static int smmu_alloc_atomic_mc(struct kvm_hyp_memcache *atomic_mc)
{
	int ret;
	int atomic_pages = 6000; /* arbitrary for now. */
	int nr_smmus = 2; /* assumption for now */
#ifndef MODULE
	u64 i;
	phys_addr_t start, end;

	/*
	 * Allocate pages to cover mapping with PAGE_SIZE for all memory
	 * Then allocate extra for 1GB of MMIO.
	 * Add 10 extra pages as we map the rest with first level blocks
	 * for PAGE_SIZE = 4KB, that should cover 5TB of address space.
	 */
	for_each_mem_range(i, &start, &end) {
		atomic_pages += __hyp_pgtable_max_pages((end - start) >> PAGE_SHIFT);
	}

	atomic_pages += __hyp_pgtable_max_pages(SZ_1G >> PAGE_SHIFT) + 10;
#endif

	/* Module didn't set that parameter. */
	if (!atomic_pages)
		return 0;

	/* For PGD*/
	ret = topup_hyp_memcache(atomic_mc, 1, 3);
	if (ret)
		return ret;

	/* For STEs. */
	ret = topup_hyp_memcache(atomic_mc, nr_smmus, 10);
		if (ret)
			return ret;

	/* For command queue. */
	ret = topup_hyp_memcache(atomic_mc, nr_smmus, 8);
	if (ret)
		return ret;

	/* For PGD. */
	ret = topup_hyp_memcache(atomic_mc, nr_smmus, 3);
	if (ret)
		return ret;

	/*For L2 ptrs */;
	ret = topup_hyp_memcache(atomic_mc, 50, 2);
	if (ret)
		return ret;

	ret = topup_hyp_memcache(atomic_mc, atomic_pages, 0);
	if (ret)
		return ret;

	pr_info("smmuv3: Allocated %d MiB for atomic usage\n",
		(atomic_pages << PAGE_SHIFT) / SZ_1M);

	return 0;
}

static int qcom_smmu_nesting_init(void)
{
	int ret = 0;
	struct kvm_hyp_memcache atomic_mc;

	init_hyp_memcache(&atomic_mc);

	ret = smmu_alloc_atomic_mc(&atomic_mc);
	if (ret)
		return ret;

	pr_info("alloc atomic suceeded\n");

	ret = kvm_arm_smmu_v3_init_drv();
	if (ret) {
		pr_err("Failed to load SMMUv3 IOMMU EL1 module %d\n", ret);
		return ret;
	}

	smmuv2_nesting_init();

#ifdef MODULE
	ret = pkvm_load_el2_module(kvm_nvhe_sym(qcom_smmu_nesting_init_module),
				   &pkvm_module_token);

	if (ret) {
		pr_err("Failed to load SMMUv3 IOMMU EL2 module: %d\n", ret);
		return ret;
	}
#endif

	ret = kvm_iommu_init_hyp(ksym_ref_addr_nvhe(qcom_smmu_hyp_nesting_ops),
				 &atomic_mc);

	if (ret) {
		pr_err("Failed to init hyp iommu ops: %d\n", ret);
		return ret;
	}

	ret = smmuv2_post_boot_init();
	if (ret) {
		pr_err("Failed to initialize SMMUv2 post boot: %d\n", ret);
		return ret;
	}

	ret = kvm_arm_smmu_v3_post_init();
	if (ret) {
		pr_err("Failed to initialize SMMUv3 post boot: %d\n", ret);
		return ret;
	}

	return ret;
}

static pkvm_handle_t qcom_smmu_get_iommu_id(struct device_node *np)
{
	return (pkvm_handle_t)0;
}

static void qcom_smmu_nesting_remove(void)
{
}

struct kvm_iommu_driver qcom_smmu_nesting_ops = {
	.init_driver = qcom_smmu_nesting_init,
	.remove_driver = qcom_smmu_nesting_remove,
	.get_iommu_id_by_of = qcom_smmu_get_iommu_id,
};

static int qcom_smmu_nesting_register(void)
{
	return kvm_iommu_register_driver(&qcom_smmu_nesting_ops);
}

/*
 * Register must be run before de-privliage before kvm_iommu_init_driver
 * for module case, it should be loaded using pKVM early loading which
 * loads it before this point.
 * For builtin drivers we use core_initcall
 */
subsys_initcall(qcom_smmu_nesting_register);
MODULE_LICENSE("GPL");
