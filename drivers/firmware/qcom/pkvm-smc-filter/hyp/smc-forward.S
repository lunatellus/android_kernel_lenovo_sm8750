/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include <linux/linkage.h>
#include <asm/assembler.h>

/*
 * Forward SMC with arguments in struct kvm_cpu_context, and
 * store the result into the same struct. Assumes SMCCC 1.2 or older.
 *
  * x0: struct user_pt_regs*
 */
SYM_FUNC_START(__forward_smc)
	/*
	 * Use x18 to keep the pointer to the host context registers.
	 * x18 is callee-saved in SMCCC but not in AAPCS64.
	 */
	mov	x18, x0
#define XREG_LDP(xl, xh)	ldp	x##xl, x##xh, [x18, ((xl) * 8)]
#define XREG_STP(xl, xh)	stp	x##xl, x##xh, [x18, ((xl) * 8)]

	/* Load the argument registers from the host context. */
	XREG_LDP(0, 1);
	XREG_LDP(2, 3);
	XREG_LDP(4, 5);
	XREG_LDP(6, 7);
	XREG_LDP(8, 9);
	XREG_LDP(10, 11);
	XREG_LDP(12, 13);
	XREG_LDP(14, 15);
	XREG_LDP(16, 17);

	smc	#0

	/* Restore the host context registers. */
	XREG_STP(0, 1);
	XREG_STP(2, 3);
	XREG_STP(4, 5);
	XREG_STP(6, 7);
	XREG_STP(8, 9);
	XREG_STP(10, 11);
	XREG_STP(12, 13);
	XREG_STP(14, 15);
	XREG_STP(16, 17);

	ret
SYM_FUNC_END(__forward_smc)
