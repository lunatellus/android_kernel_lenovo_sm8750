// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) __FILE__ ": " fmt

#include <linux/module.h>
#include <linux/habmm.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/spinlock.h>
#include <soc/qcom/qseecom_scm.h>
#include <linux/arm-smccc.h>
#include <linux/list.h>

#include "qcom_scm.h"

/**
 * struct smc_params_s
 * @fn_id: Function id used for hab channel communication
 * @arginfo: Argument information used for hab channel communication
 * @args: The array of values used for hab cannel communication
 */
struct smc_params_s {
	uint64_t fn_id;
	uint64_t arginfo;
	uint64_t args[MAX_SCM_ARGS];
} __packed;

struct hab_channel {
	struct list_head node;
	uint32_t handle;
	bool occupied;
};

static uint32_t atomic_handle;
static LIST_HEAD(nonatomic_handle_pool);
static DEFINE_SPINLOCK(nonatomic_handle_lock);

int scm_qcpe_hab_open_atomic(void)
{
	int ret;

	if (atomic_handle != 0)
		return 0;

	ret = habmm_socket_open(&atomic_handle, MM_QCPE_VM1, 0, 0);

	if (ret) {
		pr_err("Failed to open atomic HAB channel, ret = %d\n", ret);
		atomic_handle = 0;
		return ret;
	}

	pr_info("Atomic HAB channel established\n");
	return 0;
}
EXPORT_SYMBOL_GPL(scm_qcpe_hab_open_atomic);

int scm_qcpe_hab_open_nonatomic(uint32_t nchan)
{
	int ret;
	int handle;
	struct hab_channel *channel = NULL;

	/* Compatibility: fall back to single channel in case BE does not support WQ */
	if (nchan == 0)
		nchan = 1;

	for (unsigned int i = 0; i < nchan; ++i) {
		ret = habmm_socket_open(&handle, MM_QCPE_VM1, 0, 0);
		if (ret) {
			pr_err("Failed to open non-atomic HAB channel, ret = %d\n", ret);
			return ret;
		}
		channel = kzalloc(sizeof(*channel), GFP_KERNEL);
		if (!channel)
			return -ENOMEM;
		channel->handle = handle;
		channel->occupied = false;
		spin_lock(&nonatomic_handle_lock);
		list_add_tail(&channel->node, &nonatomic_handle_pool);
		spin_unlock(&nonatomic_handle_lock);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(scm_qcpe_hab_open_nonatomic);

void scm_qcpe_hab_close(void)
{
	struct list_head *entry, *n;
	struct hab_channel *channel = NULL;

	if (atomic_handle != 0) {
		habmm_socket_close(atomic_handle);
		atomic_handle = 0;
	}
	spin_lock(&nonatomic_handle_lock);
	list_for_each_safe(entry, n, &nonatomic_handle_pool) {
		channel = (struct hab_channel *) entry;
		habmm_socket_close(channel->handle);
		list_del(&channel->node);
		kfree(channel);
	}
	spin_unlock(&nonatomic_handle_lock);
}

EXPORT_SYMBOL_GPL(scm_qcpe_hab_close);

static uint32_t nonatomic_handle_acquire(void)
{
	struct list_head *entry;
	struct hab_channel *channel = NULL;
	uint32_t handle = 0;

	spin_lock(&nonatomic_handle_lock);
	list_for_each(entry, &nonatomic_handle_pool) {
		channel = (struct hab_channel *) entry;
		if (!channel->occupied) {
			channel->occupied = true;
			handle = channel->handle;
			break;
		}
	}
	spin_unlock(&nonatomic_handle_lock);

	return handle;
}

static void nonatomic_handle_release(uint32_t handle)
{
	struct list_head *entry;
	struct hab_channel *channel = NULL;

	spin_lock(&nonatomic_handle_lock);
	list_for_each(entry, &nonatomic_handle_pool) {
		channel = (struct hab_channel *) entry;
		if (channel->handle == handle && channel->occupied) {
			channel->occupied = false;
			break;
		}
	}
	spin_unlock(&nonatomic_handle_lock);
}

/*
 * Send SMC over HAB, receive the response. Both operations are blocking.
 * This is meant to be called from non-atomic context.
 */
static int scm_qcpe_hab_send_receive(struct smc_params_s *smc_params,
		u32 *size_bytes)
{
	int ret;
	uint64_t fn = smc_params->fn_id;
	uint32_t nonatomic_handle;

	nonatomic_handle = nonatomic_handle_acquire();

	if (nonatomic_handle == 0) {
		/* This should not happen */
		pr_err("Failed to acquire non-atomic handle\n");
		return -EBUSY;
	}

	ret = habmm_socket_send(nonatomic_handle, smc_params, sizeof(*smc_params), 0);
	if (ret) {
		pr_err("HAB send failed for 0x%llx, nonatomic, ret= 0x%x\n", fn, ret);
		return ret;
	}

	memset(smc_params, 0x0, sizeof(*smc_params));

	do {
		*size_bytes = sizeof(*smc_params);
		ret = habmm_socket_recv(nonatomic_handle, smc_params, size_bytes, 0,
					HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	} while (-EAGAIN == ret);

	nonatomic_handle_release(nonatomic_handle);

	if (ret) {
		pr_err("HAB recv failed for 0x%llx, nonatomic, ret= 0x%x\n",
				fn, ret);
		return ret;
	}

	return 0;
}

/*
 * Send SMC over HAB, receive the response, in non-blocking mode.
 * This is meant to be called from atomic context.
 */
static int scm_qcpe_hab_send_receive_atomic(struct smc_params_s *smc_params,
		u32 *size_bytes)
{
	static DEFINE_SPINLOCK(qcom_scm_hab_atomic_lock);
	int ret;
	int fn = smc_params->fn_id;

	spin_lock_bh(&qcom_scm_hab_atomic_lock);

	do {
		ret = habmm_socket_send(atomic_handle, smc_params, sizeof(*smc_params),
					HABMM_SOCKET_SEND_FLAGS_NON_BLOCKING);
	} while (-EAGAIN == ret);

	if (ret) {
		pr_err("HAB send failed for 0x%x, atomic, ret= 0x%x\n", fn, ret);
		spin_unlock_bh(&qcom_scm_hab_atomic_lock);
		return ret;
	}
	memset(smc_params, 0x0, sizeof(*smc_params));
	do {
		*size_bytes = sizeof(*smc_params);
		ret = habmm_socket_recv(atomic_handle, smc_params, size_bytes, 0,
					HABMM_SOCKET_RECV_FLAGS_NON_BLOCKING);
	} while ((-EAGAIN == ret) && (*size_bytes == 0));

	spin_unlock_bh(&qcom_scm_hab_atomic_lock);
	if (ret) {
		pr_err("HAB recv failed for syscall 0x%x, atomic, ret= 0x%x\n",
			fn, ret);
		return ret;
	}

	return 0;
}

int scm_call_qcpe(const struct arm_smccc_args *smc,
		struct arm_smccc_res *res, const bool atomic)
{
	u32 size_bytes;
	struct smc_params_s smc_params = {0,};
	int ret;

	smc_params.fn_id   = smc->args[0];
	smc_params.arginfo = smc->args[1];
	smc_params.args[0] = smc->args[2];
	smc_params.args[1] = smc->args[3];
	smc_params.args[2] = smc->args[4];
	smc_params.args[3] = smc->args[5];
	smc_params.args[4] = 0;

	if (!atomic) {
		ret = scm_qcpe_hab_send_receive(&smc_params, &size_bytes);
		if (ret) {
			pr_err("send/receive failed, non-atomic, ret= 0x%x\n",
				ret);
		}
	} else {
		ret = scm_qcpe_hab_send_receive_atomic(&smc_params,
							&size_bytes);
		if (ret) {
			pr_err("send/receive failed, ret= 0x%x\n", ret);
		}
	}

	if (size_bytes != sizeof(smc_params)) {
		pr_err("habmm_socket_recv expected size: %lu, actual=%u\n",
			sizeof(smc_params), size_bytes);
		ret = QCOM_SCM_ERROR;
	}

	if (ret) {
		res->a1 = res->a2 = res->a3 = (unsigned long) -1;
		res->a0 = ret;
		return ret;
	}

	res->a1 = smc_params.args[1];
	res->a2 = smc_params.args[2];
	res->a3 = smc_params.args[3];
	res->a0 = smc_params.args[0];

	return res->a0;
}
EXPORT_SYMBOL_GPL(scm_call_qcpe);

MODULE_DESCRIPTION("SCM HAB driver");
MODULE_LICENSE("GPL");
