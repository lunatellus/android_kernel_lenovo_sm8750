# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_STMMAC_ETH) += stmmac.o
stmmac-objs += stmmac_main.o stmmac_ethtool.o stmmac_mdio.o ring_mode.o chain_mode.o dwmac_lib.o dwmac1000_core.o dwmac1000_dma.o dwmac100_core.o dwmac100_dma.o enh_desc.o norm_desc.o mmc_core.o stmmac_hwtstamp.o stmmac_ptp.o dwmac4_descs.o dwmac4_dma.o dwmac4_lib.o dwmac4_core.o dwmac5.o hwif.o stmmac_tc.o dwxgmac2_core.o dwxgmac2_dma.o dwxgmac2_descs.o dw25gmac.o stmmac_xdp.o stmmac_est.o

obj-$(CONFIG_STMMAC_PLATFORM) += stmmac_platform.o

obj-$(CONFIG_DWMAC_QCOM_ETHQOS) += dwmac-qcom-eth.o
dwmac-qcom-eth-objs += dwmac-qcom-ethqos.o

