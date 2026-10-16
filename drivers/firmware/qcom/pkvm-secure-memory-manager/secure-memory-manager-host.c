// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include "hyp/smm-pkvm.h"
#include <asm/kvm_pkvm_module.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kvm_host.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/firmware/qcom/si_core_xts.h>
#include <linux/firmware/qcom/si_object.h>
#include <linux/scatterlist.h>
#include <linux/delay.h>

#ifndef MODULE
BUILD_BUG("pKVM Secure Memory Manager must be compiled as a module");
#endif

/*
 * ============================================================================
 * Configuration Macros
 * ============================================================================
 */

/*
 * SMM_BUFFER_SIZE - Size of the shared memory buffer used for data exchange
 * between EL1 (host) and QTEE.
 *
 * The buffer is allocated by EL1, filled by QTEE, and then donated to
 * EL2 via host_donate_hyp for permission processing.
 */
#define SMM_BUFFER_SIZE		0x5000	/* 20KB */

/* Service ID for SMM service in QTEE */
#define SMM_SERVICE_UID 448

/* Service method ID for SMM operations */
#define SMM_SERVICE_METHOD_ID 3

/* Error code indicating buffer is too small */
#define SMM_BUFFER_TOO_SMALL_ERROR 25

/* Magic cookie value; fill with the agreed-upon value */
#define SMM_MAGIC_COOKIE_VALUE 0xDEADBEEF

/*
 * Magic cookie value returned by QTEE when the shared buffer is too
 * small.  When si_object_do_invoke returns result code 25 and the response
 * buffer contains this cookie, num_elements holds the required buffer size
 * in bytes and the call must be retried with a newly allocated buffer of
 * that size.
 */
#define SMM_SIZE_MAGIC_COOKIE 0x53495A45

/* Magic value for num_elements indicating NOAC config is loaded */
#define SMM_NOAC_CONFIG_MAGIC 0xCAFEBABE

/*
 * ============================================================================
 * Data Structures
 * ============================================================================
 */

/**
 * struct smm_mo_info - Shared memory object information for SMM.
 * @object: si_object representing the memory object as seen by QTEE.
 * @vaddr: Virtual address for the shared buffer.
 * @size: Size in bytes of the shared buffer.
 * @paddr: Physical address for the shared buffer.
 *
 * This structure encapsulates all information needed to manage the shared
 * memory buffer used for communication with QTEE.
 */
struct smm_mo_info {
	struct si_object *object;
	void *vaddr;
	size_t size;
	phys_addr_t paddr;
};

/*
 * ============================================================================
 * Helper Functions
 * ============================================================================
 */

/**
 * smm_service_open() - Open a QTEE service by UID.
 * @uid: Service unique identifier.
 *
 * Opens a connection to a QTEE service identified by the given UID.
 * The service must be closed using put_si_object() when done.
 *
 * Return: si_object pointer on success, NULL_SI_OBJECT on failure.
 */
static struct si_object *smm_service_open(u32 uid)
{
	struct si_object *client_env = NULL_SI_OBJECT;
	struct si_object *object = NULL_SI_OBJECT;
	struct si_object_invoke_ctx *oic;

	/* Allocate invocation context for service operations */
	oic = kzalloc(sizeof(*oic), GFP_KERNEL);
	if (!oic)
		return NULL_SI_OBJECT;

	/* Get client environment and open the service */
	if (!si_core_get_client_env(oic, &client_env)) {
		/* Open the service defined using @uid */
		si_core_client_env_open(oic, client_env, uid, &object);
	}

	/* Cleanup client environment and invocation context */
	put_si_object(client_env);
	kfree(oic);

	return object;
}

/**
 * smm_mo_release() - Memory object release callback.
 * @private: Private data (smm_mo_info structure).
 *
 * Called when the memory object reference count reaches zero.
 * Frees the scatter-gather table associated with the memory object.
 */
static void smm_mo_release(void *private)
{
	struct smm_mo_info *mo_info = private;
	struct sg_table *sgt;

	/* Get the scatter-gather table back from the memory object */
	sgt = mem_object_to_sgt(mo_info->object);

	/* Free scatter-gather table resources */
	sg_free_table(sgt);
	kfree(sgt);
}

/**
 * smm_mo_info_init() - Initialize memory object information.
 * @mo_info: Pointer to smm_mo_info structure to initialize.
 *
 * Allocates a 16KB buffer, creates a scatter-gather table, and initializes
 * a secure interface memory object for communication with QTEE.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int smm_mo_info_init(struct smm_mo_info *mo_info, size_t size)
{
	struct sg_table *sgt;
	int ret;

	/* Allocate physically contiguous buffer of the requested size */
	mo_info->size = size;
	mo_info->vaddr = alloc_pages_exact(mo_info->size, GFP_KERNEL);
	if (!mo_info->vaddr) {
		kvm_err("pKVM SMM: Failed to allocate buffer (ret=-ENOMEM)\n");
		return -ENOMEM;
	}

	/* Zero-initialize buffer for QTEE */
	memset(mo_info->vaddr, 0, mo_info->size);

	/* Allocate scatter-gather table structure */
	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		ret = -ENOMEM;
		goto err_free_buf;
	}

	/* Create a single-entry scatter-gather table for the buffer */
	ret = sg_alloc_table(sgt, 1, GFP_KERNEL);
	if (ret)
		goto err_free_sgt;

	/* Set up the scatter-gather entry with buffer information */
	sg_set_page(sgt->sgl, virt_to_page(mo_info->vaddr), mo_info->size, 0);

	/* Initialize secure interface memory object with the scatter-gather table */
	mo_info->object = init_si_mem_object_sg(sgt, 0, 0, smm_mo_release, mo_info);
	if (mo_info->object == NULL_SI_OBJECT) {
		kvm_err("pKVM SMM: Failed to initialize memory object (ret=-EINVAL)\n");
		ret = -EINVAL;
		goto err_free_sgt_table;
	}

	return 0;

err_free_sgt_table:
	sg_free_table(sgt);
err_free_sgt:
	kfree(sgt);
err_free_buf:
	free_pages_exact(mo_info->vaddr, mo_info->size);

	return ret;
}

/**
 * smm_mo_info_cleanup() - Release resources held by a memory object info.
 * @mo_info: Pointer to smm_mo_info structure to clean up.
 *
 * Frees the physically contiguous buffer and releases the si_object
 * reference, which triggers smm_mo_release() to free the scatter-gather
 * table.  The structure is zeroed on return so it can be safely
 * re-initialised with smm_mo_info_init().
 */
static void smm_mo_info_cleanup(struct smm_mo_info *mo_info)
{
	/*
	 * Release the si_object first.  smm_mo_release() is invoked when the
	 * reference count reaches zero and it frees the scatter-gather table
	 * using mo_info->object, which is still valid at this point.
	 */
	put_si_object(mo_info->object);

	/* Free the physically contiguous buffer */
	free_pages_exact(mo_info->vaddr, mo_info->size);

	memset(mo_info, 0, sizeof(*mo_info));
}

/**
 * smm_is_noac_config_loaded() - Check if QTEE returns noac .
 * @response: Pointer to response buffer filled by QTEE.
 *
 * After QTEE fills the buffer, check if num_elements and magic_cookie
 * indicate that EL2 processing should be skipped.
 *
 * Return: true if EL2 should be skipped, false otherwise.
 */
static bool smm_is_noac_config_loaded(struct smm_response_buffer *response)
{
	if (response->num_elements == SMM_NOAC_CONFIG_MAGIC &&
	    response->magic_cookie == SMM_MAGIC_COOKIE_VALUE) {
		kvm_info("NOAC Config loaded");
		return true;
	}

	return false;
}

/**
 * smm_service_invoke() - Invoke QTEE service with memory object.
 * @service: Pointer to service object.
 * @mo_info: Pointer to memory object information.
 *
 * Invokes method 3 on the QTEE service, passing the memory object
 * as an input/output parameter. The service fills the buffer with memory
 * region information.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int smm_service_invoke(struct si_object *service, struct smm_mo_info *mo_info)
{
	struct si_object_invoke_ctx *oic;
	struct si_arg args[2] = { 0 };
	int ret, result;

	/* Allocate invocation context */
	oic = kzalloc(sizeof(*oic), GFP_KERNEL);
	if (!oic)
		return -ENOMEM;

	/* Increment reference count for memory object (ownership retained) */
	get_si_object(mo_info->object);

	/*
	 * Initialize arguments for method 3:
	 * args[0]: Input/Output memory object
	 * args[1]: End marker
	 */
	args[0].type = SI_AT_IO;
	args[0].o = mo_info->object;
	args[1].type = SI_AT_END;

	/* Invoke method 3 on the QTEE service */
	ret = si_object_do_invoke(oic, service, SMM_SERVICE_METHOD_ID, args, &result);

	/*
	 * QTEE signals that the shared buffer is too small by returning
	 * result code 25 and placing SMM_SIZE_MAGIC_COOKIE in the response
	 * buffer's magic_cookie field.  In that case num_elements carries the
	 * required buffer size in bytes.  Free the current buffer, allocate a
	 * new one of the requested size, and retry the service call once.
	 */
	if (!ret && result) {
		struct smm_response_buffer *response =
			(struct smm_response_buffer *)mo_info->vaddr;

		if (response->magic_cookie == SMM_SIZE_MAGIC_COOKIE) {
			size_t new_size = response->num_elements;

			kvm_info("pKVM SMM: Buffer too small, reallocating (new size=%zu bytes)\n",
				 new_size);

			/* Release old buffer and memory object */
			smm_mo_info_cleanup(mo_info);

			/* Allocate new buffer of the size requested by QTEE */
			ret = smm_mo_info_init(mo_info, new_size);
			if (ret) {
				kvm_err("pKVM SMM: Failed to reallocate memory object (ret=%d)\n",
					ret);
				kfree(oic);
				return ret;
			}

			/* Re-arm the invocation arguments with the new object */
			get_si_object(mo_info->object);
			args[0].type = SI_AT_IO;
			args[0].o = mo_info->object;
			args[1].type = SI_AT_END;

			/* Retry the service call with the larger buffer */
			ret = si_object_do_invoke(oic, service,
						  SMM_SERVICE_METHOD_ID,
						  args, &result);
		}
	}

	/* Check for any errors from initial or retry invocation */
	if (ret || result) {
		kvm_err("pKVM SMM: Service invocation failed (ret=%d, result=%d)\n", ret, result);
		kfree(oic);
		return ret ? ret : -EIO;
	}

	kfree(oic);

	return 0;
}

/**
 * smm_pass_buffer_to_hypervisor() - Pass buffer to EL2 hypervisor.
 * @vaddr: Virtual address of the buffer to pass to EL2.
 * @size: Size in bytes of the buffer.
 *
 * Passes the physical address and size of the buffer to the EL2 hypervisor
 * for further processing. The hypervisor will parse the buffer and modify
 * memory region permissions.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int smm_pass_buffer_to_hypervisor(void *vaddr, size_t size)
{
	struct smm_response_buffer *response;
	phys_addr_t buffer_phys;
	unsigned long token;
	int ret;

	response = (struct smm_response_buffer *)vaddr;

	/*
	 * Check if QTEE signalled to skip EL2 processing.
	 * QTEE sets magic_cookie in the buffer to indicate the outcome.
	 */
	if (smm_is_noac_config_loaded(response)) {
		kvm_info("pKVM Secure Memory Manager: Initialized successfully (EL2 not required)\n");
		return 0;
	}

	buffer_phys = virt_to_phys(vaddr);

	/* Set buffer pointer and size for EL2 hypervisor */
	kvm_nvhe_sym(smm_buffer_ptr) = (void *)buffer_phys;
	kvm_nvhe_sym(smm_buffer_size) = size;

	/* Load EL2 module to process the buffer */
	ret = pkvm_load_el2_module(kvm_nvhe_sym(smm_hyp_init), &token);
	if (ret) {
		kvm_err("pKVM SMM: Failed to load EL2 module (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}

/*
 * ============================================================================
 * Module Initialization and Exit
 * ============================================================================
 */

/**
 * smm_nvhe_init() - Initialize pKVM Secure Memory Manager.
 *
 * Main initialization function that:
 * 1. Allocates 16KB buffer for memory region data
 * 2. Opens QTEE service (ID 448)
 * 3. Invokes service to fill buffer with memory region information
 * 4. Passes buffer to EL2 hypervisor for permission modifications
 *
 * Return: 0 on success, negative error code on failure.
 */
static int __init smm_nvhe_init(void)
{
	struct smm_mo_info mo_info = { 0 };
	struct si_object *service = NULL_SI_OBJECT;
	int ret;

	pr_info("pKVM Secure Memory Manager: Initializing...\n");

	/* Allocate 16KB buffer and initialize memory object */
	ret = smm_mo_info_init(&mo_info, SMM_BUFFER_SIZE);
	if (ret) {
		kvm_err("pKVM SMM: Failed to initialize memory object (ret=%d)\n", ret);
		return ret;
	}

	/* Open QTEE service */
	service = smm_service_open(SMM_SERVICE_UID);
	if (service == NULL_SI_OBJECT) {
		kvm_err("pKVM SMM: Failed to open service ID %d (ret=-ENODEV)\n", SMM_SERVICE_UID);
		ret = -ENODEV;
		goto err_release_mo;
	}

	/* Invoke QTEE service to fill buffer */
	ret = smm_service_invoke(service, &mo_info);
	if (ret) {
		kvm_err("pKVM SMM: Failed to invoke service (ret=%d)\n", ret);
		goto err_release_service;
	}

	/* Store buffer info locally before releasing objects */
	void *buffer_vaddr = mo_info.vaddr;
	size_t buffer_size = mo_info.size;

	/* Cleanup service and memory object references before passing to hypervisor */
	put_si_object(service);
	put_si_object(mo_info.object);

	/* Pass buffer to EL2 hypervisor for processing */
	ret = smm_pass_buffer_to_hypervisor(buffer_vaddr, buffer_size);
	if (ret)
		return ret;

	kvm_info("pKVM Secure Memory Manager: Initialized successfully\n");
	return 0;

err_release_service:
	put_si_object(service);
err_release_mo:
	/*
	 * mo_info.object may be NULL if smm_service_invoke cleaned up the
	 * original object during a resize-and-retry that subsequently failed
	 * before a new object could be created.
	 */
	if (mo_info.object != NULL_SI_OBJECT)
		put_si_object(mo_info.object);
	return ret;
}
module_init(smm_nvhe_init);

/**
 * smm_nvhe_exit() - Cleanup pKVM Secure Memory Manager.
 *
 * Module exit function for cleanup operations.
 */
static void __exit smm_nvhe_exit(void)
{
	pr_info("pKVM Secure Memory Manager: Exiting...\n");
}
module_exit(smm_nvhe_exit);

MODULE_DESCRIPTION("pKVM Secure Memory Manager");
MODULE_LICENSE("GPL");
