/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#ifndef _EMAC_MDIO_FE_API_H_
#define _EMAC_MDIO_FE_API_H_
#include <linux/phy.h>
#if IS_ENABLED(CONFIG_EMAC_MDIO_FE)
/**
 * Returns >=0 on success or < 0 on error.
 * -ETIME means waiting for Backend reply failure.
 * -EIO means mdio hw status is not up.
 */
int virtio_mdio_read(int addr, int regnum);
/**
 * Returns >=0 on success or < 0 on error.
 * -ETIME means waiting for Backend reply failure.
 * -EIO means mdio hw status is not up.
 */
int virtio_mdio_write(int addr, int regnum, u16 val);
/**
 * Returns >=0 on success or < 0 on error.
 * -ETIME means waiting for Backend reply failure.
 * -EIO means mdio hw status is not up.
 */
int virtio_mdio_read_c45(int addr, int devnum, int regnum);
/**
 * Returns >=0 on success or < 0 on error.
 * -ETIME means waiting for Backend reply failure.
 * -EIO means mdio hw status is not up.
 */
int virtio_mdio_write_c45(int addr, int devnum, int regnum, u16 val);
#else
static inline int virtio_mdio_read(int addr, int regnum)
{
	/* Not enabled */
	return 0;
}
static inline int virtio_mdio_write(int addr, int regnum, u16 val)
{
	/* Not enabled */
	return 0;
}
static inline int virtio_mdio_read_c45(int addr, int devnum, int regnum)
{
	/* Not enabled */
	return 0;
}
static inline int virtio_mdio_write_c45(int addr, int devnum, int regnum, u16 val)
{
	/* Not enabled */
	return 0;
}
#endif /* CONFIG_EMAC_MDIO_FE */
#endif /* _EMAC_MDIO_FE_API_H_ */
