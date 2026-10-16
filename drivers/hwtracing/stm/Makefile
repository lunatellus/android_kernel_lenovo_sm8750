# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_STM_SOURCE_CONSOLE) += stm_console.o
stm_console-objs += console.o

obj-$(CONFIG_STM) += stm_core.o
stm_core-objs += core.o policy.o

obj-$(CONFIG_STM_SOURCE_FTRACE) += stm_ftrace.o
stm_ftrace-objs += ftrace.o

obj-$(CONFIG_STM_SOURCE_HEARTBEAT) += stm_heartbeat.o
stm_heartbeat-objs += heartbeat.o

obj-$(CONFIG_STM_PROTO_OST) += stm_p_ost.o
stm_p_ost-objs += p_ost.o

obj-$(CONFIG_STM_PASSTHROUGH) += stm_passthrough.o

