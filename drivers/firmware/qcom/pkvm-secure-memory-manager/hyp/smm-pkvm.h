/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#ifndef __SMM_PKVM_H__
#define __SMM_PKVM_H__

#include <asm/kvm_pkvm_module.h>
#include <asm/kvm_pgtable.h>
#include <linux/types.h>

/* Memory region structure with permissions */
struct smm_memory_region {
	phys_addr_t base;
	size_t size;
	enum kvm_pgtable_prot prot;
};

/**
 * struct smm_response_buffer - Response buffer structure from service.
 * @num_elements: Number of memory region elements in the array.
 * @regions: Array of memory region structures.
 */
struct smm_response_buffer {
	u32 magic_cookie;
	u32 num_elements;
	struct smm_memory_region regions[];
} __packed;

/* Shared declarations between host and hypervisor components */
extern void *__kvm_nvhe_smm_buffer_ptr;
extern size_t __kvm_nvhe_smm_buffer_size;

int __kvm_nvhe_smm_hyp_init(const struct pkvm_module_ops *ops);

#endif /* __SMM_PKVM_H__ */
