# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_MHI_BUS) += mhi.o
mhi-objs += boot.o init.o main.o pm.o
mhi-$(CONFIG_MHI_BUS_MISC) += misc.o
mhi-$(CONFIG_MHI_BUS_DEBUG) += debugfs.o

