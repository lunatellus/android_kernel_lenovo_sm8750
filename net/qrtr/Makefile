# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_QRTR_GUNYAH) += qrtr-gunyah.o
qrtr-gunyah-objs += gunyah.o

obj-$(CONFIG_QRTR_GENPOOL) += qrtr-genpool.o
qrtr-genpool-objs += genpool.o

obj-$(CONFIG_QRTR) += qrtr.o
qrtr-objs += af_qrtr.o ns.o

obj-$(CONFIG_QRTR_MHI) += qrtr-mhi.o
qrtr-mhi-objs += mhi.o

obj-$(CONFIG_QRTR_SMD) += qrtr-smd.o
qrtr-smd-objs += smd.o

obj-$(CONFIG_QRTR_TUN) += qrtr-tun.o
qrtr-tun-objs += tun.o

