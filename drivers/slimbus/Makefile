# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_SLIMBUS) += slimbus.o
slimbus-objs += core.o messaging.o sched.o stream.o

obj-$(CONFIG_SLIM_QCOM_NGD_CTRL) += slim-qcom-ngd-ctrl.o
slim-qcom-ngd-ctrl-objs += qcom-ngd-ctrl.o

