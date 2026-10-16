# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

# Subdirectories
obj-y += isl97900_led/
obj-y += lkdtm/

obj-$(CONFIG_QSEECOM_PROXY) += qseecom_proxy.o

obj-$(CONFIG_BOOTMARKER_PROXY) += bootmarker_proxy.o

obj-$(CONFIG_QTI_FASTRPC) += frpc-adsprpc.o
frpc-adsprpc-objs += fastrpc.o

