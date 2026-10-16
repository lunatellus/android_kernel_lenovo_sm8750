/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef __HAB_VIRQ_QVM_H
#define __HAB_VIRQ_QVM_H

int qvm_virq_tx_register(struct hvirq_dbl *dbl, int dbl_label);
int qvm_virq_rx_register(struct hvirq_dbl *dbl, int dbl_label);
int qvm_virq_send(struct hvirq_dbl *dbl);
int qvm_virq_tx_unregister(struct hvirq_dbl *dbl);
int qvm_virq_rx_unregister(struct hvirq_dbl *dbl);
int qvm_get_virq_num_id(void **virqdev, int label);
int qvm_init_virt_irq(void);
#endif
