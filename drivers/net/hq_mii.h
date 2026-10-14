/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _HQ_MII_H
#define _HQ_MII_H

#include <linux/mii.h>

int hq_mii_link_ok(struct mii_if_info *mii);
int hq_mii_nway_restart(struct mii_if_info *mii);
void hq_mii_ethtool_gset(struct mii_if_info *mii, struct ethtool_cmd *ecmd);
void hq_mii_ethtool_get_link_ksettings(struct mii_if_info *mii,
				     struct ethtool_link_ksettings *cmd);
int hq_mii_ethtool_sset(struct mii_if_info *mii, struct ethtool_cmd *ecmd);
int hq_mii_ethtool_set_link_ksettings(struct mii_if_info *mii,
				    const struct ethtool_link_ksettings *cmd);
int hq_mii_check_gmii_support(struct mii_if_info *mii);
void hq_mii_check_link(struct mii_if_info *mii);
unsigned int hq_mii_check_media(struct mii_if_info *mii,
			       unsigned int ok_to_print,
			       unsigned int init_media);
int hq_generic_mii_ioctl(struct mii_if_info *mii_if,
			 struct mii_ioctl_data *mii_data, int cmd,
			 unsigned int *duplex_changed);

#endif /* _HQ_MII_H */
