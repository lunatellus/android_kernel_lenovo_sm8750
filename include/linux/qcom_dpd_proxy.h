/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _DPD_PROXY_H
#define _DPD_PROXY_H

#include <linux/types.h>
#include <linux/scatterlist.h>
#include <linux/qtee_shmbridge.h>
#include <linux/firmware/qcom/si_object.h>
#include <linux/atomic.h>
#include <linux/completion.h>

struct dpd_scatterlist {
	struct si_object *shm;
	unsigned int nents_in_mt;
	struct sg_table sgt;
	size_t size;
	atomic_t mapcount;
	struct completion done;
	struct rcu_head rcu;
	int perms;
};

int dpd_svc_map(struct dpd_scatterlist *dpd_sg, u32 domain, u32 flags, u64 iova);
int dpd_svc_unmap(struct dpd_scatterlist *dpd_sg, u32 domain, u64 iova);
int dpd_svc_register_cbo(struct si_object *si);
int dpd_proxy_available(void);
struct dpd_scatterlist *dpd_mtree_lookup(unsigned long pfn);
#endif /* _DPD_PROXY_H */
