/* SPDX-License-Identifier: GPL-2.0 */
/*
 * hqsysfs.h - Huaqin/Lenovo shared helper API used by lenovo_sys_temp.
 *
 * The declaration is ABI-equivalent to the stock hqsysfs.ko export (see
 * below).  Stock hq_regiser_hw_info at hqsysfs .text 0x864 copies x0 into w2
 * (the id) and stores x1 unchanged into the hardware-info entry; it returns
 * 0 or -2/-3/-4 and never copies the string, which is why the caller here
 * keeps the buffer alive forever.
 *
 * Repeated locally so this module does not need the hqsysfs headers.
 * modpost resolves the symbol and records its CRC from the hqsysfs
 * module built in the same tree.
 */
#ifndef _LENOVO_HQSYSFS_H_
#define _LENOVO_HQSYSFS_H_

/* id used for the thermal sensor name list.  Stock lenovo_sys_temp .text
 * 0x520 loads 0x26 (HWID_THERMAL in hqsysfs's enum hardware_id).
 */
#define HWID_THERMAL_NTC	38

/*
 * ABI-equivalent to hqsysfs's `int hq_regiser_hw_info(enum hardware_id id,
 * char *device_name)` (the enum is int-sized, the callee only stores the
 * pointer).  The import CRC modpost records comes from the exporter's
 * Module.symvers, not from this declaration, so hqsysfs has to be
 * built in the same tree.
 */
extern int hq_regiser_hw_info(int id, const char *dev_name);

#endif /* _LENOVO_HQSYSFS_H_ */