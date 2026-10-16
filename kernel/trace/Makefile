# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_IPC_LOGGING) += qcom_ipc_logging.o
qcom_ipc_logging-objs += ipc_logging.o ipc_logging_debug.o
qcom_ipc_logging-$(CONFIG_IPC_LOGGING_CDEV) += ipc_logging_cdev.o

