/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 Lenovo (Huaqin) - hq_usbnet private API
 *
 * Declares the usbnet core entry points exported by hq_usbnet.ko plus the
 * hq_mii.ko helpers it consumes.  include/linux/usb/usbnet.h still declares
 * the in-tree names, so they cannot be used for cross-module binding here.
 */
#ifndef __LINUX_HQ_USBNET_H
#define __LINUX_HQ_USBNET_H

#include <linux/usb.h>
#include <linux/usb/usbnet.h>
#include <linux/ethtool.h>
#include <linux/mii.h>

extern int hq_usbnet_get_endpoints(struct usbnet *dev,
				   struct usb_interface *intf);
extern int hq_usbnet_get_ethernet_addr(struct usbnet *dev, int iMACAddress);
extern int hq_usbnet_status_start(struct usbnet *dev, gfp_t mem_flags);
extern void hq_usbnet_status_stop(struct usbnet *dev);
extern void hq_usbnet_skb_return(struct usbnet *dev, struct sk_buff *skb);
extern void hq_usbnet_update_max_qlen(struct usbnet *dev);
extern int hq_usbnet_change_mtu(struct net_device *net, int new_mtu);
extern void hq_usbnet_defer_kevent(struct usbnet *dev, int work);
extern void hq_usbnet_pause_rx(struct usbnet *dev);
extern void hq_usbnet_resume_rx(struct usbnet *dev);
extern void hq_usbnet_purge_paused_rxq(struct usbnet *dev);
extern void hq_usbnet_unlink_rx_urbs(struct usbnet *dev);
extern int hq_usbnet_stop(struct net_device *net);
extern int hq_usbnet_open(struct net_device *net);
extern int hq_usbnet_get_link_ksettings_mii(struct net_device *net,
					    struct ethtool_link_ksettings *cmd);
extern int hq_usbnet_get_link_ksettings_internal(struct net_device *net,
						 struct ethtool_link_ksettings *cmd);
extern int hq_usbnet_set_link_ksettings_mii(struct net_device *net,
					    const struct ethtool_link_ksettings *cmd);
extern u32 hq_usbnet_get_link(struct net_device *net);
extern int hq_usbnet_nway_reset(struct net_device *net);
extern void hq_usbnet_get_drvinfo(struct net_device *net,
				  struct ethtool_drvinfo *info);
extern u32 hq_usbnet_get_msglevel(struct net_device *net);
extern void hq_usbnet_set_msglevel(struct net_device *net, u32 level);
extern void hq_usbnet_set_rx_mode(struct net_device *net);
extern void hq_usbnet_tx_timeout(struct net_device *net, unsigned int txqueue);
extern netdev_tx_t hq_usbnet_start_xmit(struct sk_buff *skb,
					struct net_device *net);
extern void hq_usbnet_disconnect(struct usb_interface *intf);
extern int hq_usbnet_probe(struct usb_interface *intf,
			   const struct usb_device_id *prod);
extern int hq_usbnet_suspend(struct usb_interface *intf, pm_message_t message);
extern int hq_usbnet_resume(struct usb_interface *intf);
extern void hq_usbnet_device_suggests_idle(struct usbnet *dev);
extern int hq_usbnet_manage_power(struct usbnet *dev, int on);
extern void hq_usbnet_link_change(struct usbnet *dev, bool link, bool need_reset);

extern int hq_usbnet_read_cmd(struct usbnet *dev, u8 cmd, u8 reqtype,
			      u16 value, u16 index, void *data, u16 size);
extern int hq_usbnet_write_cmd(struct usbnet *dev, u8 cmd, u8 reqtype,
			       u16 value, u16 index, const void *data, u16 size);
extern int hq_usbnet_read_cmd_nopm(struct usbnet *dev, u8 cmd, u8 reqtype,
				   u16 value, u16 index, void *data, u16 size);
extern int hq_usbnet_write_cmd_nopm(struct usbnet *dev, u8 cmd, u8 reqtype,
				    u16 value, u16 index, const void *data,
				    u16 size);
extern int hq_usbnet_write_cmd_async(struct usbnet *dev, u8 cmd, u8 reqtype,
				     u16 value, u16 index, const void *data,
				     u16 size);

/* hq_mii.ko */
extern int hq_mii_link_ok(struct mii_if_info *mii);
extern int hq_mii_nway_restart(struct mii_if_info *mii);
extern void hq_mii_ethtool_get_link_ksettings(struct mii_if_info *mii,
					      struct ethtool_link_ksettings *cmd);
extern int hq_mii_ethtool_set_link_ksettings(struct mii_if_info *mii,
					     const struct ethtool_link_ksettings *cmd);

#endif /* __LINUX_HQ_USBNET_H */
