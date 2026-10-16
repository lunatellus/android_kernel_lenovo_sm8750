// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include <asm/kvm_pkvm_module.h>
#include <asm/kvm_pgtable.h>
#include <linux/types.h>

#include "smm-pkvm.h"

/*
 * ============================================================================
 * pKVM Secure Memory Manager - EL2 Hypervisor Component
 * ============================================================================
 *
 * This module runs at EL2 (hypervisor level) and is responsible for:
 * 1. Receiving the 16KB buffer from EL1 (host) containing memory region data
 * 2. Performing host_donate_hyp to transfer buffer ownership to hypervisor
 * 3. Parsing the buffer to extract memory region information
 * 4. Modifying permissions of specified memory regions in host page tables
 * 5. Registering fault handlers for protected memory regions
 */

/*
 * ============================================================================
 * Global Variables
 * ============================================================================
 */

/* Buffer pointer (physical address) and size, set by EL1 init */
void *smm_buffer_ptr;
size_t smm_buffer_size;

/* EL2 virtual address mapping of the buffer */
struct smm_response_buffer *smm_buffer_vaddr;

/* Module operations pointer for hypervisor services */
static const struct pkvm_module_ops *mod_ops;

/*
 * ============================================================================
 * Permission Fault Handler Functions
 * ============================================================================
 */

/**
 * smm_host_perm_fault_handler() - Handle host permission faults.
 * @regs: Pointer to user registers at time of fault.
 * @esr: Exception Syndrome Register value.
 * @addr: Faulting address.
 *
 * Called when the host tries to access a memory region with insufficient
 * permissions. Checks if the fault is in one of our managed regions.
 *
 * Return: -EPERM if in our region (let pKVM handle), 0 otherwise.
 */
static int smm_host_perm_fault_handler(struct user_pt_regs *regs, u64 esr, u64 addr)
{
	struct smm_response_buffer *buffer;
	unsigned int i, num_regions;
	u64 region_start, region_end;

	if (!smm_buffer_vaddr)
		return 0;

	buffer = smm_buffer_vaddr;
	num_regions = buffer->num_elements;

	mod_ops->puts("[pKVM EL2] SMM: Permission fault handler invoked");

	bool is_write = esr & ESR_ELx_WNR;
	/* Check if the fault address is in one of our managed regions */
	for (i = 0; i < num_regions; i++) {
		region_start = buffer->regions[i].base;
		region_end = region_start + buffer->regions[i].size;

		if (addr >= region_start && addr < region_end) {
			/*
			 * This is a fault in one of our protected regions.
			 * Return -EPERM to let pKVM handle the abort.
			 */
			if (is_write)
				mod_ops->puts("[pKVM EL2] SMM: write fault at");
			else
				mod_ops->puts("[pKVM EL2] SMM: read fault at");
			mod_ops->putx64(addr);
			mod_ops->puts("[pKVM EL2] SMM: ESR");
			mod_ops->putx64(esr);
			return -EPERM;
		}
	}

	/*
	 * Not our region, return 0 to let the next handler try,
	 * or let pKVM's default handling take over.
	 */
	return 0;
}

/*
 * ============================================================================
 * Memory Region Permission Modification Functions
 * ============================================================================
 */

/**
 * smm_is_donated_to_hyp() - Check if a region falls within a hyp-donated area.
 * @buffer: Pointer to the response buffer containing all memory regions.
 * @base: Physical base address of the region to check.
 * @size: Size of the region to check.
 * @current_idx: Current index in the buffer (only check regions before this).
 *
 * Loops through regions that have already been processed (indices < current_idx)
 * looking for regions with prot == 0 (donated to hyp via host_donate_hyp) and
 * checks whether the given region is fully contained within any of them. This
 * handles the case where a larger region was donated and only a portion is being
 * returned to the host.
 *
 * Return: true if the region is within a donated area, false otherwise.
 */
static bool smm_is_donated_to_hyp(struct smm_response_buffer *buffer,
				  phys_addr_t base, size_t size,
				  unsigned int current_idx)
{
	unsigned int j;
	u64 donated_start, donated_end;
	u64 region_start = base;
	u64 region_end = base + size;

	for (j = 0; j < current_idx; j++) {
		if (buffer->regions[j].prot != 0)
			continue;

		donated_start = buffer->regions[j].base;
		donated_end = donated_start + buffer->regions[j].size;

		/* Check if the region is fully contained within a donated region */
		if (region_start >= donated_start && region_end <= donated_end)
			return true;
	}

	return false;
}

/**
 * smm_apply_memory_protections() - Apply permissions to all memory regions.
 * @buffer: Pointer to the response buffer containing all memory regions.
 *
 * Iterates over all memory regions in the buffer and applies the specified
 * protection flags to each region in the host stage-2 page tables.
 * If any region fails, the function returns immediately with the error code.
 *
 * Protection values:
 * - 0: No permissions (donated to hyp, host loses all access)
 * - KVM_PGTABLE_PROT_R: Read-only
 * - KVM_PGTABLE_PROT_RW: Return to host (only if previously donated to hyp)
 *
 * Return: 0 on success, negative error code on failure.
 */
static int smm_apply_memory_protections(struct smm_response_buffer *buffer)
{
	unsigned int i;
	u64 pfn, nr_pages;
	int ret;

	for (i = 0; i < buffer->num_elements; i++) {
		/*
		 * Shift pointer and size by PAGE_SHIFT to convert to page
		 * frame number and number of pages, as host_donate_hyp and
		 * host_stage2_mod_prot work on page number and number of pages.
		 */
		pfn = buffer->regions[i].base >> PAGE_SHIFT;
		nr_pages = buffer->regions[i].size >> PAGE_SHIFT;

		if (buffer->regions[i].prot == 0) {
			/*
			 * prot == 0: Donate the region to the hypervisor.
			 * The host loses all access; the region becomes
			 * exclusively owned by EL2.
			 */
			ret = mod_ops->host_donate_hyp(pfn, nr_pages, true);
			if (ret) {
				mod_ops->puts("[pKVM EL2] SMM: host_donate_hyp failed for region");
				mod_ops->putx64(ret);
				mod_ops->puts("[pKVM EL2] SMM: Base Address:");
				mod_ops->putx64((u64)buffer->regions[i].base);
				return ret;
			}
		} else {
			/*
			 * prot != 0: The host retains (or regains) access.
			 *
			 * If the region falls within a previously donated area
			 * (prot==0), it must first be returned to the host via
			 * hyp_donate_host before any stage-2 permission change.
			 *
			 * For KVM_PGTABLE_PROT_RW, skip entirely if the region
			 * was never donated (nothing to do). For all other prot
			 * values, apply host_stage2_mod_prot after the optional
			 * hyp_donate_host.
			 */
			bool donated = smm_is_donated_to_hyp(buffer,
							     buffer->regions[i].base,
							     buffer->regions[i].size, i);

			if (buffer->regions[i].prot == KVM_PGTABLE_PROT_RW && !donated)
				continue;

			if (donated) {
				ret = mod_ops->hyp_donate_host(pfn, nr_pages);
				if (ret) {
					mod_ops->puts("[pKVM EL2] SMM: hyp_donate_host failed");
					mod_ops->putx64(ret);
					mod_ops->puts("[pKVM EL2] SMM: Base Address:");
					mod_ops->putx64((u64)buffer->regions[i].base);
					return ret;
				}
			}

			if (buffer->regions[i].prot != KVM_PGTABLE_PROT_RW) {
				ret = mod_ops->host_stage2_mod_prot(pfn,
								    buffer->regions[i].prot,
								    nr_pages, true);
				if (ret) {
					mod_ops->puts("[pKVM EL2] SMM: host_stage2_mod_prot fail");
					mod_ops->putx64(ret);
					mod_ops->puts("[pKVM EL2] SMM: Base Address:");
					mod_ops->putx64((u64)buffer->regions[i].base);
					return ret;
				}
			}
		}
	}
	return 0;
}

/*
 * ============================================================================
 * Module Initialization
 * ============================================================================
 */

/**
 * smm_hyp_init() - Initialize EL2 Secure Memory Manager module.
 * @ops: Pointer to pKVM module operations.
 *
 * This function is called during pKVM module initialization and performs:
 * 1. Validates the buffer pointer received from EL1
 * 2. Performs host_donate_hyp to transfer buffer ownership to hypervisor
 * 3. Maps the buffer to EL2 virtual address space
 * 4. Registers fault handlers for protected regions
 * 5. Modifies permissions for all memory regions in the buffer
 *
 * Return: 0 on success, negative error code on failure.
 */
int smm_hyp_init(const struct pkvm_module_ops *ops)
{
	int ret;
	u64 pfn, nr_pages;

	if (!ops)
		return -EINVAL;

	mod_ops = ops;

	/* Validate buffer pointer (physical address from EL1) */
	if (!smm_buffer_ptr) {
		mod_ops->puts("[pKVM EL2] SMM: Buffer pointer is NULL (ret=-EINVAL)");
		return -EINVAL;
	}
	/*
	 * Shift pointer and size by PAGE_SHIFT to convert to page frame number
	 * and number of pages, as host_donate_hyp works on page number and
	 * number of pages.
	 */
	pfn = (u64)smm_buffer_ptr >> PAGE_SHIFT;
	nr_pages = smm_buffer_size >> PAGE_SHIFT;

	/* Transfer buffer received from EL1 to hypervisor ownership*/
	ret = mod_ops->host_donate_hyp(pfn, nr_pages, false);
	if (ret) {
		mod_ops->puts("[pKVM EL2] SMM: host_donate_hyp failed on EL1 buffer");
		mod_ops->putx64(ret);
		return ret;
	}

	/* Map physical address to EL2 virtual address */
	smm_buffer_vaddr =
		(struct smm_response_buffer *)mod_ops->hyp_va((phys_addr_t)smm_buffer_ptr);

	if (!smm_buffer_vaddr) {
		mod_ops->puts("[pKVM EL2] SMM: Failed to map buffer to EL2 VA (ret=-EINVAL)");
		return -EINVAL;
	}

	/* Register permission fault handler */
	ret = mod_ops->register_host_perm_fault_handler(smm_host_perm_fault_handler);
	if (ret) {
		mod_ops->puts("[pKVM EL2] SMM: register_host_perm_fault_handler failed");
		mod_ops->putx64(ret);
		return ret;
	}

	/* Apply permissions to all memory regions in the buffer */
	ret = smm_apply_memory_protections(smm_buffer_vaddr);
	if (ret) {
		mod_ops->puts("[pKVM EL2] SMM: smm_apply_memory_protections failed");
		mod_ops->putx64(ret);
		return ret;
	}

	mod_ops->puts("[pKVM EL2] SMM: Secure memory manager loaded successfully");

	return 0;
}
