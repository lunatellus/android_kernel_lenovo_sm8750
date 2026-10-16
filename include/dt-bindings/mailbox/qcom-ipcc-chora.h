/* SPDX-License-Identifier: GPL-2.0-only OR BSD-2-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __DT_BINDINGS_MAILBOX_IPCC_CHORA_H
#define __DT_BINDINGS_MAILBOX_IPCC_CHORA_H

/* Physical client IDs */
#define IPCC_MPROC_AOP			0
#define IPCC_MPROC_TZ			1
#define IPCC_MPROC_MPSS			2
#define IPCC_MPROC_LPASS		3
#define IPCC_MPROC_APSS_NS0		4
#define IPCC_MPROC_GPU			5
#define IPCC_MPROC_CAM			6
#define IPCC_MPROC_VPU			8
#define IPCC_MPROC_TME			12
#define IPCC_MPROC_WPSS			11
#define IPCC_MPROC_IPA			9
#define IPCC_MPROC_APSS_NS1		7

#define IPCC_COMPUTE_L0_MPSS		0
#define IPCC_COMPUTE_L0_LPASS		1
#define IPCC_COMPUTE_L0_APSS_NS0	2
#define IPCC_COMPUTE_L0_GPU		3
#define IPCC_COMPUTE_L0_CAM		4
#define IPCC_COMPUTE_L0_VPU		6
#define IPCC_COMPUTE_L0_DPU0		7
#define IPCC_COMPUTE_L0_APSS_NS1	5

#define IPCC_COMPUTE_L1_MPSS		0
#define IPCC_COMPUTE_L1_LPASS		1
#define IPCC_COMPUTE_L1_APSS_NS0	2
#define IPCC_COMPUTE_L1_GPU		3
#define IPCC_COMPUTE_L1_CAM		4
#define IPCC_COMPUTE_L1_VPU		6
#define IPCC_COMPUTE_L1_DPU0		7
#define IPCC_COMPUTE_L1_APSS_NS1	5

#define IPCC_PERIPH_MPSS		0
#define IPCC_PERIPH_LPASS		1
#define IPCC_PERIPH_APSS_NS0		2
#define IPCC_PERIPH_PCIE0		3
#define IPCC_PERIPH_WPSS		4
#define IPCC_PERIPH_APSS_NS1		5

#define IPCC_FENCE_APSS_NS0		0
#define IPCC_FENCE_GPU			1
#define IPCC_FENCE_DPU0			2
#define IPCC_FENCE_APSS_NS1		3

#endif
