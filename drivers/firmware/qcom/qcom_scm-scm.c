// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/arm-smccc.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/firmware/qcom/qcom_scm.h>

#include "qcom_scm.h"

/**
 * qcom_scm_pas_supported() - Check if the peripheral authentication service is
 *			      available for the given peripheral
 * @peripheral:	peripheral id
 *
 * Returns true if PAS is supported for this peripheral, otherwise false.
 */
bool qcom_scm_pas_supported(u32 peripheral)
{
	int ret;
	struct device *scm_dev = NULL;

	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_IS_SUPPORTED,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = peripheral,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return false;

	if (!__qcom_scm_is_call_available(scm_dev, QCOM_SCM_SVC_PIL,
					  QCOM_SCM_PIL_PAS_IS_SUPPORTED))
		return false;

	ret = qcom_scm_call(scm_dev, &desc, &res);

	return ret ? false : !!res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_pas_supported);

/**
 * qcom_scm_pas_init_image() - Initialize peripheral authentication service
 *			       state machine for a given peripheral, using the
 *			       metadata
 * @peripheral: peripheral id
 * @metadata:	pointer to memory containing ELF header, program header table
 *		and optional blob of data used for authenticating the metadata
 *		and the rest of the firmware
 * @size:	size of the metadata
 * @ctx:	optional metadata context
 * @dev_32bit: if not NULL, memory need to be allocated from lower 4G
 *
 * Return: 0 on success.
 *
 * Upon successful return, the PAS metadata context (@ctx) will be used to
 * track the metadata allocation, this needs to be released by invoking
 * qcom_scm_pas_metadata_release() by the caller.
 */
int qcom_scm_pas_init_image(u32 peripheral, const void *metadata, size_t size,
			    struct qcom_scm_pas_metadata *ctx, struct device *dev_32bit)
{
	struct device *dma_dev = NULL, *scm_dev = NULL;
	dma_addr_t mdata_phys;
	void *mdata_buf;
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_INIT_IMAGE,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_VAL, QCOM_SCM_RW),
		.args[0] = peripheral,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;
	dma_dev = scm_dev;

	/*
	 * Only use 32bit dma device for dma memory allocation but use
	 * Scm device for any scm calls.
	 */
	if (dev_32bit)
		dma_dev = dev_32bit;

	/*
	 * During the scm call memory protection will be enabled for the
	 * metadata blob, so make sure it's physically contiguous, 4K aligned
	 * and non-cachable to avoid XPU violations.
	 *
	 * For PIL calls the hypervisor creates SHM Bridges for the blob
	 * buffers on behalf of Linux so we must not do it ourselves hence
	 * not using the TZMem allocator here.
	 *
	 * If we pass a buffer that is already part of an SHM Bridge to this
	 * call, it will fail.
	 */
	mdata_buf = dma_alloc_coherent(dma_dev, size, &mdata_phys,
				       GFP_KERNEL);
	if (!mdata_buf)
		return -ENOMEM;
	memcpy(mdata_buf, metadata, size);

	ret = qcom_scm_clk_enable();
	if (ret)
		goto out;

	ret = qcom_scm_bw_enable();
	if (ret)
		goto disable_clk;

	desc.args[1] = mdata_phys;

	ret = qcom_scm_call(scm_dev, &desc, &res);
	qcom_scm_bw_disable();

disable_clk:
	qcom_scm_clk_disable();

out:
	if (ret < 0 || !ctx) {
		dma_free_coherent(dma_dev, size, mdata_buf, mdata_phys);
	} else if (ctx) {
		ctx->ptr = mdata_buf;
		ctx->phys = mdata_phys;
		ctx->size = size;
	}

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_pas_init_image);

/**
 * qcom_scm_pas_shutdown() - Shut down the remote processor
 * @peripheral: peripheral id
 *
 * Returns 0 on success.
 */
int qcom_scm_pas_shutdown(u32 peripheral)
{
	int ret;
	struct device *scm_dev = NULL;

	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_SHUTDOWN,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = peripheral,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = qcom_scm_bw_enable();
	if (ret)
		goto disable_clk;

	ret = qcom_scm_call(scm_dev, &desc, &res);
	qcom_scm_bw_disable();

disable_clk:
	qcom_scm_clk_disable();

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_pas_shutdown);

/**
 * qcom_scm_pas_shutdown_retry() - Shut down the remote processor by retrying
 * @peripheral: peripheral id
 *
 * Returns 0 on success.
 */
int qcom_scm_pas_shutdown_retry(u32 peripheral)
{
	unsigned int pas_shutdown_retry_delay_ms = 0;
	int ret;

	ret = qcom_scm_pas_shutdown(peripheral);
	/* No need to retry if the first try worked */
	if (!ret)
		return ret;

	pas_shutdown_retry_delay_ms = qcom_scm_pas_get_shutdown_retry_delay_ms();
	if (pas_shutdown_retry_delay_ms > 0) {
		pr_err("PAS Shutdown: First call to shutdown failed with error: %d\n", ret);
		pr_err("PAS Shutdown: Sleeping for: %u\n", pas_shutdown_retry_delay_ms);
		msleep(pas_shutdown_retry_delay_ms);

		pr_err("PAS Shutdown: Attempting to shutdown peripheral again\n");
		return qcom_scm_pas_shutdown(peripheral);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_scm_pas_shutdown_retry);

/**
 * qcom_scm_pas_mem_setup() - Prepare the memory related to a given peripheral
 *			      for firmware loading
 * @peripheral:	peripheral id
 * @addr:	start address of memory area to prepare
 * @size:	size of the memory area to prepare
 *
 * Returns 0 on success.
 */
int qcom_scm_pas_mem_setup(u32 peripheral, phys_addr_t addr, phys_addr_t size)
{
	int ret;
	struct device *scm_dev = NULL;

	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_MEM_SETUP,
		.arginfo = QCOM_SCM_ARGS(3),
		.args[0] = peripheral,
		.args[1] = addr,
		.args[2] = size,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = qcom_scm_bw_enable();
	if (ret)
		goto disable_clk;

	ret = qcom_scm_call(scm_dev, &desc, &res);
	qcom_scm_bw_disable();

disable_clk:
	qcom_scm_clk_disable();

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_pas_mem_setup);

/**
 * qcom_scm_pas_auth_and_reset() - Authenticate the given peripheral firmware
 *				   and reset the remote processor
 * @peripheral:	peripheral id
 *
 * Return 0 on success.
 */
int qcom_scm_pas_auth_and_reset(u32 peripheral)
{
	int ret;
	struct device *scm_dev = NULL;

	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_AUTH_AND_RESET,
		.arginfo = QCOM_SCM_ARGS(1),
		.args[0] = peripheral,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	struct qcom_scm_res res;

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	ret = qcom_scm_clk_enable();
	if (ret)
		return ret;

	ret = qcom_scm_bw_enable();
	if (ret)
		goto disable_clk;

	ret = qcom_scm_call(scm_dev, &desc, &res);
	qcom_scm_bw_disable();

disable_clk:
	qcom_scm_clk_disable();

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_pas_auth_and_reset);

/**
 * Placeholder API
 * Currently no-op, safe to call
 */
void qcom_scm_pas_store_memoryinfo(u32 peripheral, phys_addr_t addr,
		phys_addr_t size)
{
	/* Nothing to do for legacy SCM */
}
EXPORT_SYMBOL_GPL(qcom_scm_pas_store_memoryinfo);

int qcom_scm_mem_protect_video_var(u32 cp_start, u32 cp_size,
				   u32 cp_nonpixel_start,
				   u32 cp_nonpixel_size)
{
	int ret;
	struct device *scm_dev = NULL;

	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_VIDEO_VAR,
		.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_VAL, QCOM_SCM_VAL,
					 QCOM_SCM_VAL, QCOM_SCM_VAL),
		.args[0] = cp_start,
		.args[1] = cp_size,
		.args[2] = cp_nonpixel_start,
		.args[3] = cp_nonpixel_size,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	struct qcom_scm_res res;

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	ret = qcom_scm_call(scm_dev, &desc, &res);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_mem_protect_video_var);

/**
 * qcom_scm_assign_dump_table_region() - Assign a memory region to the
 *                                       dump table
 * @is_assign:  1 = assign, 0 = unassign
 * @addr:       start address of memory region
 * @size:       size of the memory region
 *
 * Returns 0 on success.
 */
int qcom_scm_assign_dump_table_region(bool is_assign, phys_addr_t addr, size_t size)
{
	struct device *scm_dev = NULL;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_UTIL,
		.cmd = QCOM_SCM_UTIL_DUMP_TABLE_ASSIGN,
		.arginfo = QCOM_SCM_ARGS(3),
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = is_assign,
		.args[1] = addr,
		.args[2] = size,
	};

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	return qcom_scm_call(scm_dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_assign_dump_table_region);

/**
 * qcom_scm_kgsl_dcvs_tuning() - Tune KGSL DCVS parameters.
 * @mingap:     Minimum time gap between two consecutive frequency requests.
 * @penalty:    A penalty value applied to each frequency request to discourage
 *              frequent changes.
 * @numbusy:    The number of busy cycles to consider for performance evaluation.
 *
 * This function makes a secure call to adjust the tuning parameters for the
 * KGSL DCVS algorithm. These parameters influence how the GPU frequency scales in
 * response to load.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
int qcom_scm_kgsl_dcvs_tuning(u32 mingap, u32 penalty, u32 numbusy)
{
	struct device *scm_dev = NULL;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_DCVS,
		.cmd = QCOM_SCM_DCVS_TUNING,
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = mingap,
		.args[1] = penalty,
		.args[2] = numbusy,
		.arginfo = QCOM_SCM_ARGS(3),
	};

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	return qcom_scm_call(scm_dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_kgsl_dcvs_tuning);

/**
 * qcom_scm_dcvs_update_v2() - Update DCVS with new performance data.
 * @level:      The DCVS level to update.
 * @total_time: Total time elapsed for the measurement period in nanoseconds.
 * @busy_time:  Time the resource was busy during the measurement period in ns.
 *
 * This function makes a secure call to update the DCVS service
 * with new performance metrics.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
int qcom_scm_dcvs_update_v2(int level, s64 total_time, s64 busy_time)
{
	struct device *scm_dev = NULL;
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_DCVS,
		.cmd = QCOM_SCM_DCVS_UPDATE_V2,
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = level,
		.args[1] = total_time,
		.args[2] = busy_time,
		.arginfo = QCOM_SCM_ARGS(3),
	};
	struct qcom_scm_res res;

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	ret = qcom_scm_call(scm_dev, &desc, &res);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_dcvs_update_v2);

/**
 * qcom_scm_dcvs_update_ca_v2() - Update Context-Aware DCVS with new values.
 * @level:          The DCVS level to update.
 * @total_time:     Total time elapsed for the measurement period.
 * @busy_time:      Time the resource was busy during the measurement period.
 * @context_count:  The number of active contexts during the period.
 *
 * This function makes a secure call to update the Context-Aware DCVS
 * service with new performance data, including the number of active
 * contexts.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
int qcom_scm_dcvs_update_ca_v2(int level, s64 total_time, s64 busy_time,
			       int context_count)
{
	struct device *scm_dev = NULL;
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_DCVS,
		.cmd = QCOM_SCM_DCVS_UPDATE_CA_V2,
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = level,
		.args[1] = total_time,
		.args[2] = busy_time,
		.args[3] = context_count,
		.arginfo = QCOM_SCM_ARGS(4),
	};
	struct qcom_scm_res res;

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	ret = qcom_scm_call(scm_dev, &desc, &res);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_dcvs_update_ca_v2);

/**
 * qcom_scm_dcvs_reset()
 */
int qcom_scm_dcvs_reset(void)
{
	struct device *scm_dev = NULL;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_DCVS,
		.cmd = QCOM_SCM_DCVS_RESET,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	scm_dev = qcom_scm_get_dev();

	return qcom_scm_call(scm_dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_dcvs_reset);

/**
 * qcom_scm_dcvs_init_ca_v2() - Initialize Context-Aware DCVS.
 * @addr: Physical address of the buffer with context-aware tuning data.
 * @size: Size of the context-aware data buffer.
 *
 * This function makes a secure call to initialize the context-aware
 * feature of the DCVS service. It passes a buffer containing parameters
 * like the target power level and busy penalty to the secure environment.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
int qcom_scm_dcvs_init_ca_v2(phys_addr_t addr, size_t size)
{
	struct device *scm_dev = NULL;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_DCVS,
		.cmd = QCOM_SCM_DCVS_INIT_CA_V2,
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = addr,
		.args[1] = size,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL),
	};

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	return qcom_scm_call(scm_dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_dcvs_init_ca_v2);

/**
 * qcom_scm_dcvs_init_v2() - Initialize DCVS service.
 * @addr:       Physical address of the buffer containing power level data.
 * @size:       Size of the power level data buffer.
 * @version:    Output pointer to store the DCVS version.
 *
 * This function makes a secure call to initialize the DCVS v2 service.
 * It passes a table of power levels to the secure environment and gets
 * back the supported DCVS interface version.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
int qcom_scm_dcvs_init_v2(phys_addr_t addr, size_t size, int *version)
{
	struct device *scm_dev = NULL;
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_DCVS,
		.cmd = QCOM_SCM_DCVS_INIT_V2,
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = addr,
		.args[1] = size,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL),
	};
	struct qcom_scm_res res;

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	ret = qcom_scm_call(scm_dev, &desc, &res);

	if (ret >= 0)
		*version = res.result[0];
	return ret;
}
EXPORT_SYMBOL_GPL(qcom_scm_dcvs_init_v2);

/**
 * qcom_scm_dcvs_update() - Update DCVS with new values
 * @level:      The DCVS level to update.
 * @total_time: Total time elapsed for the measurement period.
 * @busy_time:  Time the resource was busy during the measurement period.
 *
 * This function makes an atomic secure call to update the DCVS parameters.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
int qcom_scm_dcvs_update(int level, s64 total_time, s64 busy_time)
{
	struct device *scm_dev = NULL;
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_DCVS,
		.cmd = QCOM_SCM_DCVS_UPDATE,
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = level,
		.args[1] = total_time,
		.args[2] = busy_time,
		.arginfo = QCOM_SCM_ARGS(3),
	};
	struct qcom_scm_res res;

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	ret = qcom_scm_call_atomic(scm_dev, &desc, &res);

	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_dcvs_update);

/**
 * qcom_scm_kgsl_init_regs() - Initialize KGSL registers
 * @gpu_req: The GPU request identifier.
 *
 * This function makes a secure call to initialize
 * the KGSL registers for a specific GPU context.
 *
 * Return: 0 on success, or a negative error code on failure.
 */
int qcom_scm_kgsl_init_regs(u32 gpu_req)
{
	struct device *scm_dev = NULL;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_GPU,
		.cmd = QCOM_SCM_SVC_GPU_INIT_REGS,
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = gpu_req,
		.arginfo = QCOM_SCM_ARGS(1),
	};

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	return qcom_scm_call(scm_dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_kgsl_init_regs);

int qcom_scm_kgsl_set_smmu_aperture(unsigned int num_context_bank)
{
	struct device *scm_dev = NULL;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_CP_SMMU_APERTURE_ID,
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = 0xffff0000
			   | ((QCOM_SCM_CP_APERTURE_REG & 0xff) << 8)
			   | (num_context_bank & 0xff),
		.args[1] = 0xffffffff,
		.args[2] = 0xffffffff,
		.args[3] = 0xffffffff,
		.arginfo = QCOM_SCM_ARGS(4),
	};

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	return qcom_scm_call(scm_dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_kgsl_set_smmu_aperture);

int qcom_scm_kgsl_set_smmu_lpac_aperture(unsigned int num_context_bank)
{
	struct device *scm_dev = NULL;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_CP_SMMU_APERTURE_ID,
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = 0xffff0000
			   | ((QCOM_SCM_CP_LPAC_APERTURE_REG & 0xff) << 8)
			   | (num_context_bank & 0xff),
		.args[1] = 0xffffffff,
		.args[2] = 0xffffffff,
		.args[3] = 0xffffffff,
		.arginfo = QCOM_SCM_ARGS(4),
	};

	scm_dev = qcom_scm_get_dev();
	if (!scm_dev)
		return -ENODEV;

	return qcom_scm_call(scm_dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_kgsl_set_smmu_lpac_aperture);

/**
 * qcom_scm_dcvs_core_available() - check if core DCVS operations are available
 */
bool qcom_scm_dcvs_core_available(void)
{
	struct device *dev = NULL;

	dev = qcom_scm_get_dev();

	return __qcom_scm_is_call_available(dev, QCOM_SCM_SVC_DCVS,
					    QCOM_SCM_DCVS_INIT) &&
	       __qcom_scm_is_call_available(dev, QCOM_SCM_SVC_DCVS,
					    QCOM_SCM_DCVS_UPDATE) &&
	       __qcom_scm_is_call_available(dev, QCOM_SCM_SVC_DCVS,
					    QCOM_SCM_DCVS_RESET);
}
EXPORT_SYMBOL_GPL(qcom_scm_dcvs_core_available);

/**
 * qcom_scm_dcvs_ca_available() - check if context aware DCVS operations are
 * available
 */
bool qcom_scm_dcvs_ca_available(void)
{
	struct device *dev = NULL;

	dev = qcom_scm_get_dev();

	return __qcom_scm_is_call_available(dev, QCOM_SCM_SVC_DCVS,
					    QCOM_SCM_DCVS_INIT_CA_V2) &&
	       __qcom_scm_is_call_available(dev, QCOM_SCM_SVC_DCVS,
					    QCOM_SCM_DCVS_UPDATE_CA_V2);
}
EXPORT_SYMBOL_GPL(qcom_scm_dcvs_ca_available);
