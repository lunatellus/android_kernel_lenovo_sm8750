# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_MMC_CQHCI) += cqhci.o
cqhci-objs += cqhci-core.o
cqhci-$(CONFIG_MMC_CRYPTO) += cqhci-crypto.o
cqhci-$(CONFIG_MMC_CRYPTO_QTI) += cqhci-crypto-qti.o

obj-$(CONFIG_MMC_SDHCI_MSM) += sdhci-msm.o

