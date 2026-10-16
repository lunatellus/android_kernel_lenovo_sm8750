# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_QCOM_MEM_BUF_DEV) += mem_buf_dev.o
mem_buf_dev-objs += mem-buf-dev.o mem-buf-ids.o mem_buf_dma_buf.o
mem_buf_dev-$(CONFIG_QCOM_MEM_BUF_DEV_GH) += mem-buf-dev-gh.o

obj-$(CONFIG_QCOM_MEM_BUF) += mem_buf.o
mem_buf-objs += mem-buf.o
mem_buf-$(CONFIG_QCOM_MEM_BUF_GH) += mem-buf-gh.o

obj-$(CONFIG_QCOM_MEM_BUF_MSGQ) += mem_buf_msgq.o
mem_buf_msgq-objs += mem-buf-msgq.o

