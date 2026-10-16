# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_CPU_IDLE_GOV_QCOM_LPM) += qcom_lpm.o
qcom_lpm-objs += qcom-cluster-lpm.o qcom-lpm-sysfs.o qcom-lpm.o

obj-$(CONFIG_CPU_IDLE_SIMPLE_GOV_QCOM_LPM) += qcom_simple_lpm.o
qcom_simple_lpm-objs += qcom-simple-cluster-lpm.o qcom-simple-lpm-sysfs.o qcom-simple-lpm.o

