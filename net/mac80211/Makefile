# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_MAC80211) += mac80211.o
mac80211-objs += aead_api.o aes_cmac.o aes_gmac.o agg-rx.o agg-tx.o airtime.o cfg.o chan.o driver-ops.o eht.o ethtool.o fils_aead.o he.o ht.o ibss.o iface.o key.o link.o main.o michael.o mlme.o ocb.o offchannel.o parse.o rate.o rx.o s1g.o scan.o spectmgmt.o sta_info.o status.o tdls.o tkip.o trace.o tx.o util.o vht.o wbrf.o wep.o wme.o wpa.o
mac80211-$(CONFIG_MAC80211_LEDS) += led.o
mac80211-$(CONFIG_MAC80211_DEBUGFS) += debugfs.o debugfs_sta.o debugfs_netdev.o debugfs_key.o
mac80211-$(CONFIG_MAC80211_MESH) += mesh.o mesh_pathtbl.o mesh_plink.o mesh_hwmp.o mesh_sync.o mesh_ps.o
mac80211-$(CONFIG_PM) += pm.o
mac80211-$(CONFIG_MAC80211_RC_MINSTREL) += rc80211_minstrel_ht.o

