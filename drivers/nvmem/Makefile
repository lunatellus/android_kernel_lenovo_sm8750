# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_NVMEM_SPMI_SDAM) += nvmem_qcom-spmi-sdam.o
nvmem_qcom-spmi-sdam-objs += qcom-spmi-sdam.o

obj-$(CONFIG_NVMEM_QCOM_QFPROM) += nvmem_qfprom.o
nvmem_qfprom-objs += qfprom.o

obj-$(CONFIG_NVMEM_LAYOUT_QCOM_ETH_MAC) += nvmem_layout_qcom_eth_mac.o
nvmem_layout_qcom_eth_mac-objs += layouts/qcom-eth-mac.o

