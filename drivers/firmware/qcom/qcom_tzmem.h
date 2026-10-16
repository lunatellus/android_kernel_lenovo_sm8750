/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023-2024 Linaro Ltd.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __QCOM_TZMEM_PRIV_H
#define __QCOM_TZMEM_PRIV_H

struct device;

bool qcom_tzmem_get_status(void);
int qcom_tzmem_enable(struct device *dev);

#if IS_ENABLED(CONFIG_QCOM_TZMEM_MODE_SHMBRIDGE)

enum bridge_owner {
	/**< Bridge is owned by tzmem/shmbridge. */
	SELF = 1,
	/**< Bridge is owned by other users. */
	OTHERS,
};

int32_t qcom_tzmem_query(phys_addr_t paddr);
int qcom_tzmem_shm_bridge_create_with_vmid(phys_addr_t paddr, size_t size, u32 *ns_vmid_list,
					   u32 *ns_vm_perm_list, u32 ns_vmid_num, u32 tz_perm,
					   enum bridge_owner owner, u64 *handle);
int qcom_tzmem_pm_freeze(void);
int qcom_tzmem_pm_restore(void);
int qcom_tzmem_pm_thaw(void);
#else
static inline int32_t qcom_tzmem_query(phys_addr_t paddr)
{
	return 0;
}
static inline int qcom_tzmem_shm_bridge_create_with_vmid(phys_addr_t paddr, size_t size,
		u32 *ns_vmid_list, u32 *ns_vm_perm_list, u32 ns_vmid_num, u32 tz_perm,
		enum bridge_owner owner, u64 *handle)
{
	return 0;
}
static int qcom_tzmem_pm_freeze(void)
{
	return 0;
}

static int qcom_tzmem_pm_restore(void)
{
	return 0;
}

static int qcom_tzmem_pm_thaw(void)
{
	return 0;
}
#endif /* CONFIG_QCOM_TZMEM_MODE_SHMBRIDGE */

#ifdef CONFIG_QCOM_TZMEM_FFA
int qcom_tzmem_ffa_mem_share(const void *vaddr, const dma_addr_t dma_addr,
			     size_t size, bool is_cached, uint64_t *ffa_handle);
int qcom_tzmem_ffa_mem_reclaim(uint64_t ffa_handle);

int qcom_tzmem_ffa_register(struct device *dev);
#else
static inline int qcom_tzmem_ffa_mem_share(const void *vaddr, const dma_addr_t dma_addr,
					   size_t size, bool is_cached, uint64_t *ffa_handle)
{
	return -EINVAL;
}

static inline int qcom_tzmem_ffa_mem_reclaim(uint64_t ffa_handle)
{
	return -EINVAL;
}

static inline int qcom_tzmem_ffa_register(struct device *dev)
{
	return -EOPNOTSUPP;
}
#endif /* CONFIG_QCOM_TZMEM_FFA */
#endif /* __QCOM_TZMEM_PRIV_H */
