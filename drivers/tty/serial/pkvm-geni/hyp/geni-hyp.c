// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include <asm/kvm_pkvm_module.h>
#include <linux/io.h>
#include <linux/soc/qcom/geni-se.h>

/**********************************
 * QCOM GENI UART putc functionality for EL2
 */

/* UART specific GENI registers */
#define SE_UART_TX_TRANS_LEN		0x270
/* UART M_CMD OP codes */
#define UART_START_TX			0x1

/* Reasonable UART polling iterations value until timeout */
#define UART_TIMEOUT	100000

/* UART resource base, set by EL1 init */
resource_size_t uart_start;
/* Remapped UART base address */
static void __iomem *uart_addr;

/* Wait for the M_GENI_CMD_ACTIVE bit to be cleared */
static inline int geni_uart_wait_active_clear(void)
{
	u32 timeout = UART_TIMEOUT;

	while ((readl(uart_addr + SE_GENI_STATUS) & M_GENI_CMD_ACTIVE) != 0U) {
		if (--timeout == 0)
			return -1; /* Timeout error */
	}
	return 0;
}

/* UART putc */
static void geni_hyp_putc(char c)
{
	if (geni_uart_wait_active_clear())
		return;

	/* Transmit one char */
	writel(1, uart_addr + SE_UART_TX_TRANS_LEN);
	writel(UART_START_TX << M_OPCODE_SHFT, uart_addr + SE_GENI_M_CMD0);
	writel(c, uart_addr + SE_GENI_TX_FIFOn);

	(void)geni_uart_wait_active_clear();
}

/*
 * EL2 UART module init
 */
int geni_hyp_init(const struct pkvm_module_ops *ops)
{
	int ret = ops->create_private_mapping(uart_start, PAGE_SIZE,
			PAGE_HYP_DEVICE, (unsigned long *)&uart_addr);
	if (ret)
		return ret;

	ret = ops->register_serial_driver(geni_hyp_putc);
	if (ret)
		return ret;

	ops->puts("[pKVM EL2] qcom GENI UART EL2 driver loaded");
	return 0;
}
