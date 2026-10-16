# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_QTI_MPAM) += mpam.o
mpam-objs += mpam.o mpam_rpmsg.o

obj-$(CONFIG_QTI_CPU_MPAM_INTERFACE) += cpu_mpam.o

obj-$(CONFIG_QTI_PLATFORM_MPAM_INTERFACE) += platform_mpam.o

obj-$(CONFIG_QTI_MPAM_MSC) += mpam_msc.o

obj-$(CONFIG_QTI_MPAM_MSC_SLC) += mpam_msc_slc.o

obj-$(CONFIG_QTI_SLC_MPAM_INTERFACE) += slc_mpam.o

