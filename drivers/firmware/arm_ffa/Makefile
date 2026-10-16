# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_ARM_FFA_TRANSPORT) += arm_ffa_transport.o
arm_ffa_transport-objs += bus.o

obj-$(CONFIG_ARM_FFA_TRANSPORT) += arm_ffa.o
arm_ffa-objs += smccc.o driver.o

