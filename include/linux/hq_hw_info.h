/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Huaqin hardware information hub.
 *
 * Every vendor driver that knows something about the board (RAM size, UFS
 * size, panel, cameras, chargers, ...) registers a name under a numeric
 * hardware id through hq_regiser_hw_info().  The collected names are exposed
 * as a single sysfs directory:
 *
 *	/sys/class/hardinfo/interface/hardware_info/<name>
 *
 * with 0644 permissions.  Reading an attribute yields the registered name
 * (or, for the special ids below, a computed value); writing a name
 * overrides the auto detected device description.
 */
#ifndef HQ_HW_INFO_H
#define HQ_HW_INFO_H

/*
 * <linux/cdev.h> and <linux/syscalls.h> are unused here, but genksyms expands
 * every struct defined in the including translation unit.  Only with both
 * visible do the export CRCs match stock: register_kboj_under_hqsysfs
 * 0x65f7991a, and in wcd939x.c wcd939x_get_codec_variant 0x28fbc265 and
 * wcd939x_codec_get_dev_num 0x603b1d6a (struct cdev and the perf/trace types
 * otherwise stay "{ UNKNOWN }").
 */
#include <linux/cdev.h>
#include <linux/syscalls.h>
#include <linux/sysfs.h>
#include <linux/types.h>

/*
 * Per-id state.  "attr" has to stay the second member: the sysfs attribute
 * array holds pointers to it and show()/store() recover the id from the
 * address of ->attr.
 */
struct hq_hw_info {
	int id;
	struct attribute attr;
	int is_set;
	const char *dev_name;
};

/*
 * Hardware ids, values are part of the vendor ABI.  The enumerator list and its
 * spelling (implicit values inside each group, upper-case hex group starts and
 * the explicit HWID_VIBRATOR) are copied from the stock module's BTF/CRC: the
 * enum is expanded into the genksyms CRC of hq_regiser_hw_info(), so any other
 * spelling changes the exported CRC away from stock 0xb8057d22.
 */
enum hardware_id {
	HWID_NONE = 0x00,
	HWID_MEMORY = 0x10,
	HWID_EMMC,
	HWID_NAND,
	HWID_DDR,
	HWID_UFS,
	HWID_ROM,
	HWID_LCM = 0x20,
	HWID_BL,
	HWID_SUB_LCM,
	HWID_BIAS_IC,
	HWID_CTP,
	HWID_AUDIO,
	HWID_THERMAL,
	HWID_MAIN_CAM = 0x30,
	HWID_MAIN_CAM_2,
	HWID_MAIN_CAM_3,
	HWID_SUB_CAM,
	HWID_SUB_CAM_2,
	HWID_MAIN_LENS,
	HWID_MAIN_LENS_2,
	HWID_SUB_LENS,
	HWID_SUB_LENS_2,
	HWID_MAIN_OTP,
	HWID_MAIN_OTP_2,
	HWID_SUB_OTP,
	HWID_SUB_OTP_2,
	HWID_FLASHLIGHT,
	HWID_FLaSHLIGHT_2,
	HWID_MASTER1_CHARGE = 0x50,
	HWID_MASTER2_CHARGE,
	HWID_MASTER_CP_CHARGE,
	HWID_SLAVE_CP_CHARGE,
	HWID_PD_CHARGE,
	HWID_FG1_CHARGE,
	HWID_FG2_CHARGE,
	HWID_QC_CHARGE,
	HWID_SC7603A_CHARGE,
	HWID_SC7603B_CHARGE,
	HWID_GSENSOR = 0x70,
	HWID_ALSPS,
	HWID_RGB,
	HWID_TOF,
	HWID_SAR,
	HWID_HALL,
	HWID_GYRO,
	HWID_MSENSOR,
	HWID_IRDA,
	HWID_BAROMETER,
	HWID_PEDOMETER,
	HWID_HUMIDITY,
	HWID_PCBA = 0x80,
	HWID_PCBA_INFO,
	HWID_SIMSTATE,
	HWID_SDCARD_STATUS,
	HWID_SMARTPA,
	HWID_AUDIO_CODEC,
	HWID_BATERY = 0xA0,
	HWID_FUEL_GAUGE_IC,
	HWID_NFC = 0xC0,
	HWID_FP,
	HWID_TEE,
	HWID_USB_TYPE_C = 0xE0,
	HWID_VIBRATOR = 0xE1,
	HWID_SUMMARY = 0xF0,
	HWID_VER,
	HWID_PMIC_VERSION,
	HWID_END
};

/* Number of ids in huaqin_attrs[]. */
#define HQ_HW_INFO_MAX			44

/**
 * hq_regiser_hw_info() - publish the device name of one hardware id
 * @id: one of the HWID_* values
 * @device_name: device name, must not be NULL
 *
 * Return: 0 on success, -2 when @device_name is NULL, -3 for an unknown id and
 * -4 when the id has already been registered.
 */
int hq_regiser_hw_info(enum hardware_id id, char *device_name);

/**
 * hq_deregister_hw_info() - drop a previously registered device name
 * @id: one of the HWID_* values
 * @device_name: name to match against the registered one
 *
 * Return: 0 on success, -2 for a NULL @device_name, -3 for an unknown id, -4
 * when nothing was registered, -5 when no name is stored and -6 when
 * @device_name does not match the stored one.
 */
int hq_deregister_hw_info(enum hardware_id id, char *device_name);

/**
 * register_kboj_under_hqsysfs() - kobject_init_and_add() below /hardinfo
 * @kobj: kobject to initialise
 * @ktype: kobject type
 * @fmt: printf format for the kobject name
 * @...: arguments for @fmt
 *
 * Same as kobject_init_and_add() with the parent fixed to the "interface"
 * device created by this module.
 *
 * Return: 0 on success, negative errno otherwise.
 */
int register_kboj_under_hqsysfs(struct kobject *kobj, struct kobj_type *ktype,
				const char *fmt, ...);

#endif /* HQ_HW_INFO_H */
