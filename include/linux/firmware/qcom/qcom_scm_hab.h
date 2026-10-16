/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.​
 */

#include <linux/arm-smccc.h>

#ifndef __QCOM_SCM_HAB_H_
#define __QCOM_SCM_HAB_H_

#if IS_ENABLED(CONFIG_MSM_HAB)
int scm_qcpe_hab_open_atomic(void);

int scm_qcpe_hab_open_nonatomic(uint32_t nchan);

void scm_qcpe_hab_close(void);

int scm_call_qcpe(const struct arm_smccc_args *smc,
		struct arm_smccc_res *res, const bool atomic);

#else
static inline int scm_qcpe_hab_open_atomic(void)
{
	return -EOPNOTSUPP;
}

static inline int scm_qcpe_hab_open_nonatomic(uint32_t nchan)
{
	return -EOPNOTSUPP;
}

static inline void scm_qcpe_hab_close(void)
{
}

static inline int scm_call_qcpe(const struct arm_smccc_args *smc,
		struct arm_smccc_res *res, const bool atomic)
{
	return -EOPNOTSUPP;
}
#endif /* CONFIG_MSM_HAB */

#endif /* __QCOM_SCM_HAB_H_ */
