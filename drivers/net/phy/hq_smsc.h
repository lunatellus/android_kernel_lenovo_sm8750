/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _HQ_SMSCPHY_H
#define _HQ_SMSCPHY_H

#include <linux/phy.h>

int hq_smsc_phy_config_intr(struct phy_device *phydev);
irqreturn_t hq_smsc_phy_handle_interrupt(struct phy_device *phydev);
int hq_smsc_phy_config_init(struct phy_device *phydev);
int hq_lan87xx_read_status(struct phy_device *phydev);
int hq_smsc_phy_get_tunable(struct phy_device *phydev,
			  struct ethtool_tunable *tuna, void *data);
int hq_smsc_phy_set_tunable(struct phy_device *phydev,
			  struct ethtool_tunable *tuna, const void *data);
int hq_smsc_phy_probe(struct phy_device *phydev);

#endif /* _HQ_SMSCPHY_H */
