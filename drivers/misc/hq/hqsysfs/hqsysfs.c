// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Huaqin hardware information driver.
 *
 * Publishes a per-hardware-id device name below
 * /sys/class/hardinfo/interface/hardware_info/ and lets vendor drivers
 * register (and override) those names.  A few ids are computed by this module
 * itself: the RAM size, the UFS size and the PCBA/board name.
 */
#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pm_runtime.h>
#include <scsi/scsi.h>
#include <scsi/scsi_host.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>
#include <ufs/ufshcd.h>
#include <linux/unaligned.h>

#include <linux/hq_hw_info.h>

#define HQ_CLASS_NAME	"hardinfo"
#define HQ_DEV_NAME	"interface"
#define HQ_KOBJ_NAME	"hardware_info"

/* GPIO that reports the microSD card presence. */
#define HQ_SDCARD_GPIO	567

/*
 * Preloader data published before any vendor driver has registered:
 * firmware version, PMIC type and the haptic (vibrator) chip.  Stock .data
 * 0x00/0x10/0x20 holds ids 0xf1, 0xf2, 0xe1.
 */
struct hq_pre_dev_map {
	enum hardware_id id;
	char *dev_name;
};

struct hq_pre_dev_map pre_dev_maps[] = {
	{ HWID_VER,			"2023-08-28 V0.1" },
	{ HWID_PMIC_VERSION,		"PM8550" },
	{ HWID_VIBRATOR,		"AW86937" },
};

struct hq_pre_dev {
	int ram;			/* RAM size in GB */
	char ram_str[12];
	int rom;			/* UFS size in GB */
	char rom_str[16];
	struct scsi_device *scsi_dev;
	struct ufs_hba *hba;
	char pcba[64];
};

static struct hq_pre_dev *g_pre_dev;
static struct device *huaqin_hw_device;

/*
 * Board identification, filled from the module parameters.  modprobe passes
 * them from the kernel command line (hqsysfs.pcba_config=PRC
 * hqsysfs.pcba_stage=SOVP).  Stock uses module_param_string() with 10 byte
 * buffers (struct kparam_string maxlen 10 at .rodata+0x8/+0x28).
 */
static char config[10];
static char stage[10];

module_param_string(pcba_config, config, sizeof(config), 0644);
MODULE_PARM_DESC(pcba_config, "pcba config, such as PRC or ROW");
module_param_string(pcba_stage, stage, sizeof(stage), 0644);
MODULE_PARM_DESC(pcba_stage, "pcba stage, such as EVT or PVT");

static struct class *huaqin_class;
static struct kobject huaqin_kobj;

/*
 * One instance per hardware id.  ->attr is embedded so that the sysfs
 * attribute array can point straight at it.
 */
#define HQ_HW_INFO(_id, _name)					\
	{								\
		.id		= (_id),				\
		.attr		= { .name = (_name), .mode = 0644 },	\
	}

static struct hq_hw_info hw_info_ver		= HQ_HW_INFO(HWID_VER, "ver");
static struct hq_hw_info hw_info_hw_summary	= HQ_HW_INFO(HWID_SUMMARY, "hw_summary");
static struct hq_hw_info hw_info_ram		= HQ_HW_INFO(HWID_DDR, "ram");
static struct hq_hw_info hw_info_rom		= HQ_HW_INFO(HWID_ROM, "rom");
static struct hq_hw_info hw_info_lcm		= HQ_HW_INFO(HWID_LCM, "lcm");
static struct hq_hw_info hw_info_bl		= HQ_HW_INFO(HWID_BL, "bl");
static struct hq_hw_info hw_info_bias		= HQ_HW_INFO(HWID_BIAS_IC, "bias");
static struct hq_hw_info hw_info_ctp		= HQ_HW_INFO(HWID_CTP, "ctp");
static struct hq_hw_info hw_info_sub_cam		= HQ_HW_INFO(HWID_SUB_CAM, "sub_cam");
static struct hq_hw_info hw_info_main0_cam	= HQ_HW_INFO(HWID_SUB_CAM_2, "main0_cam");
static struct hq_hw_info hw_info_main1_cam	= HQ_HW_INFO(HWID_MAIN_CAM, "main1_cam");
static struct hq_hw_info hw_info_main2_cam	= HQ_HW_INFO(HWID_MAIN_CAM_2, "main2_cam");
static struct hq_hw_info hw_info_main3_cam	= HQ_HW_INFO(HWID_MAIN_CAM_3, "main3_cam");
static struct hq_hw_info hw_info_main_cam_len	= HQ_HW_INFO(HWID_MAIN_LENS, "main_cam_len");
static struct hq_hw_info hw_info_flashlight	= HQ_HW_INFO(HWID_FLASHLIGHT, "flashlight");
static struct hq_hw_info hw_info_gsensor	= HQ_HW_INFO(HWID_GSENSOR, "gsensor");
static struct hq_hw_info hw_info_rgb		= HQ_HW_INFO(HWID_RGB, "rgb");
static struct hq_hw_info hw_info_tof		= HQ_HW_INFO(HWID_TOF, "tof");
static struct hq_hw_info hw_info_sar		= HQ_HW_INFO(HWID_SAR, "sar");
static struct hq_hw_info hw_info_hall		= HQ_HW_INFO(HWID_HALL, "hall");
static struct hq_hw_info hw_info_msensor	= HQ_HW_INFO(HWID_MSENSOR, "msensor");
static struct hq_hw_info hw_info_gyro		= HQ_HW_INFO(HWID_GYRO, "gyro");
static struct hq_hw_info hw_info_alsps		= HQ_HW_INFO(HWID_ALSPS, "alsps");
static struct hq_hw_info hw_info_irda		= HQ_HW_INFO(HWID_IRDA, "irda");
static struct hq_hw_info hw_info_fuel_gauge_ic	= HQ_HW_INFO(HWID_FUEL_GAUGE_IC, "fuel_gauge_ic");
static struct hq_hw_info hw_info_nfc		= HQ_HW_INFO(HWID_NFC, "nfc");
static struct hq_hw_info hw_info_pcba_config	= HQ_HW_INFO(HWID_PCBA, "pcba_config");
static struct hq_hw_info hw_info_pmic_version	= HQ_HW_INFO(HWID_PMIC_VERSION, "pmic_version");
static struct hq_hw_info hw_info_master1_charge	= HQ_HW_INFO(HWID_MASTER1_CHARGE, "master1_charge");
static struct hq_hw_info hw_info_master2_charge	= HQ_HW_INFO(HWID_MASTER2_CHARGE, "master2_charge");
static struct hq_hw_info hw_info_master_cp_charge = HQ_HW_INFO(HWID_MASTER_CP_CHARGE, "master_cp_charge");
static struct hq_hw_info hw_info_slave_cp_charge	= HQ_HW_INFO(HWID_SLAVE_CP_CHARGE, "slave_cp_charge");
static struct hq_hw_info hw_info_pd_charge	= HQ_HW_INFO(HWID_PD_CHARGE, "pd_charge");
static struct hq_hw_info hw_info_fg1_charge	= HQ_HW_INFO(HWID_FG1_CHARGE, "fg1_charge");
static struct hq_hw_info hw_info_fg2_charge	= HQ_HW_INFO(HWID_FG2_CHARGE, "fg2_charge");
static struct hq_hw_info hw_info_qc_charge	= HQ_HW_INFO(HWID_QC_CHARGE, "qc_charge");
static struct hq_hw_info hw_info_sc7603a_charge	= HQ_HW_INFO(HWID_SC7603A_CHARGE, "sc7603a_charge");
static struct hq_hw_info hw_info_sc7603b_charge	= HQ_HW_INFO(HWID_SC7603B_CHARGE, "sc7603b_charge");
static struct hq_hw_info hw_info_vibrator	= HQ_HW_INFO(HWID_VIBRATOR, "vibrator");
static struct hq_hw_info hw_info_thermal_ntc	= HQ_HW_INFO(HWID_THERMAL, "thermal_ntc");
static struct hq_hw_info hw_info_sdcard_status	= HQ_HW_INFO(HWID_SDCARD_STATUS, "sdcard_status");
static struct hq_hw_info hw_info_smartpa		= HQ_HW_INFO(HWID_SMARTPA, "smartpa");
static struct hq_hw_info hw_info_audio_codec	= HQ_HW_INFO(HWID_AUDIO_CODEC, "audio_codec");

static struct attribute *huaqin_attrs[] = {
	&hw_info_ver.attr,
	&hw_info_hw_summary.attr,
	&hw_info_ram.attr,
	&hw_info_rom.attr,
	&hw_info_lcm.attr,
	&hw_info_bl.attr,
	&hw_info_bias.attr,
	&hw_info_ctp.attr,
	&hw_info_sub_cam.attr,
	&hw_info_main0_cam.attr,
	&hw_info_main1_cam.attr,
	&hw_info_main2_cam.attr,
	&hw_info_main3_cam.attr,
	&hw_info_main_cam_len.attr,
	&hw_info_flashlight.attr,
	&hw_info_gsensor.attr,
	&hw_info_rgb.attr,
	&hw_info_tof.attr,
	&hw_info_sar.attr,
	&hw_info_hall.attr,
	&hw_info_msensor.attr,
	&hw_info_gyro.attr,
	&hw_info_alsps.attr,
	&hw_info_irda.attr,
	&hw_info_fuel_gauge_ic.attr,
	&hw_info_nfc.attr,
	&hw_info_pcba_config.attr,
	&hw_info_pmic_version.attr,
	&hw_info_master1_charge.attr,
	&hw_info_master2_charge.attr,
	&hw_info_master_cp_charge.attr,
	&hw_info_slave_cp_charge.attr,
	&hw_info_pd_charge.attr,
	&hw_info_fg1_charge.attr,
	&hw_info_fg2_charge.attr,
	&hw_info_qc_charge.attr,
	&hw_info_sc7603a_charge.attr,
	&hw_info_sc7603b_charge.attr,
	&hw_info_vibrator.attr,
	&hw_info_thermal_ntc.attr,
	&hw_info_sdcard_status.attr,
	&hw_info_smartpa.attr,
	&hw_info_audio_codec.attr,
	NULL
};

static struct attribute_group huaqin_group = {
	.attrs = huaqin_attrs,
};


static const struct sysfs_ops huaqin_sysfs_ops;

static struct kobj_type huaqin_ktype = {
	.sysfs_ops = &huaqin_sysfs_ops,
	.default_groups = (const struct attribute_group *[]){
		&huaqin_group,
		NULL,
	},
};

static const char *sdcard_state(void)
{
	if (gpiod_get_raw_value(gpio_to_desc(HQ_SDCARD_GPIO)) == 1)
		return "Insert";

	return "Not Insert";
}

static ssize_t huaqin_show(struct kobject *kobj, struct attribute *a,
			    char *buf)
{
	ssize_t count = 0;
	struct hq_hw_info *hw = container_of(a, struct hq_hw_info, attr);

	if (!hw)
		return sprintf(buf, "Data error\n");

	if (hw->id == HWID_SUMMARY) {
		int iterator = 0;
		struct attribute *attr = huaqin_attrs[iterator];

		while (attr) {
			hw = container_of(attr, struct hq_hw_info, attr);
			iterator += 1;
			attr = huaqin_attrs[iterator];

			if (hw->is_set && hw->dev_name &&
			    strcmp(hw->attr.name, "sdcard_status") &&
			    strcmp(hw->attr.name, "thermal_ntc"))
				count += sprintf(buf + count, "%s: %s\n",
						 hw->attr.name, hw->dev_name);

			if (!strcmp(hw->attr.name, "sdcard_status"))
				count += sprintf(buf + count, "%s: %s\n",
						 hw->attr.name, sdcard_state());
		}
		return count;
	} else if (hw->id == HWID_SDCARD_STATUS) {
		return sprintf(buf, "%s\n", sdcard_state());
	} else if (hw->id == HWID_THERMAL) {
		char *tz_buf = kstrdup(hw->dev_name, GFP_KERNEL);
		char *tz_names = tz_buf;
		struct thermal_zone_device *tz;
		char *zone_name;
		int temp = 0;

		if (hw->is_set && hw->dev_name) {
			zone_name = strsep(&tz_names, " ");
			while (zone_name) {
				tz = thermal_zone_get_zone_by_name(zone_name);
				if (!IS_ERR_OR_NULL(tz)) {
					thermal_zone_get_temp(tz, &temp);
					count += sprintf(buf + count, "%s: %d\n",
							 zone_name, temp);
				}
				zone_name = strsep(&tz_names, " ");
			}
		}
		kfree(tz_buf);
		return count;
	}

	if (!hw->is_set)
		return sprintf(buf, "Not support!\n");

	if (!hw->dev_name)
		return sprintf(buf, "Installed with no device Name!\n");

	return sprintf(buf, "%s\n", hw->dev_name);
}

static ssize_t huaqin_store(struct kobject *kobj, struct attribute *attr,
			     const char *buf, size_t count)
{
	/*
	 * sysfs hands over a NUL-terminated copy of the write, which may be
	 * longer than any fixed stack buffer: match against it directly.
	 */
	const char *tmp = buf;

	pr_err("%s %s \n", __func__, tmp);

	if (strstr(tmp, "Accelerometer")) {
		if (strstr(tmp, "TDK-Invensense"))
			hq_regiser_hw_info(HWID_GSENSOR, "icm4x6xa");
		else
			hq_regiser_hw_info(HWID_GSENSOR, "lsm6dsv");
	}

	if (strstr(tmp, "Gyroscope")) {
		if (strstr(tmp, "TDK-Invensense"))
			hq_regiser_hw_info(HWID_GYRO, "icm4x6xa");
		else
			hq_regiser_hw_info(HWID_GYRO, "lsm6dsv");
	}

	if (strstr(tmp, "Magnetometer")) {
		if (strstr(tmp, "memsic"))
			hq_regiser_hw_info(HWID_MSENSOR, "mmc56x3x");
		else
			hq_regiser_hw_info(HWID_MSENSOR, "qmc5603");
	}

	if (strstr(tmp, "Alsps")) {
		hq_regiser_hw_info(HWID_ALSPS, "mn78");
		hq_regiser_hw_info(HWID_HALL, "hall");
	}

	if (strstr(tmp, "Sar"))
		hq_regiser_hw_info(HWID_SAR, "aw963xx0");

	return count;
}

static const struct sysfs_ops huaqin_sysfs_ops = {
	.show = huaqin_show,
	.store = huaqin_store,
};

/**
 * ufs_read_desc_param() - read a UFS descriptor parameter
 * @hba: UFS host controller
 * @desc_id: descriptor to read from
 * @desc_index: index inside the descriptor
 * @param_offset: byte offset inside the descriptor
 * @buf: destination buffer, at least @size bytes
 * @size: number of bytes to read, 1, 2, 4 or 8
 *
 * Reads @size bytes and byte swaps them to host order.  Any other size up
 * to 8 stores only the first byte.
 *
 * Return: 0 on success, -EINVAL for a @size above 8 or a failed read.
 */
int ufs_read_desc_param(struct ufs_hba *hba, enum desc_idn desc_id,
			u8 desc_index, u8 param_offset, u8 *buf, u8 size)
{
	u8 desc_buf[8] = {0};
	int ret;

	if (size > 8)
		return -EINVAL;

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_read_desc_param(hba, desc_id, desc_index, param_offset,
				     desc_buf, size);
	pm_runtime_put_sync(hba->dev);
	if (ret)
		return -EINVAL;

	switch (size) {
	case 1:
		*buf = *desc_buf;
		break;
	case 2:
		*((u16 *)buf) = get_unaligned_be16(desc_buf);
		break;
	case 4:
		*((u32 *)buf) = get_unaligned_be32(desc_buf);
		break;
	case 8:
		*((u64 *)buf) = get_unaligned_be64(desc_buf);
		break;
	default:
		/* Stock copies a single byte for any other size. */
		*buf = *desc_buf;
		break;
	}

	return ret;
}

struct ufs_hba *get_ufs_hba_data(void)
{
	struct Scsi_Host *host;

	if (!g_pre_dev->scsi_dev) {
		host = scsi_host_lookup(0);
		if (host) {
			g_pre_dev->scsi_dev = scsi_device_lookup(host, 0, 0, 0);
			if (g_pre_dev->scsi_dev)
				pr_info("[ufs_info] hardwareinfo-> scsi device proc name is %s\n",
					g_pre_dev->scsi_dev->host->hostt->proc_name);
			scsi_host_put(host);
		}
	}

	if (!g_pre_dev->scsi_dev)
		return NULL;

	host = g_pre_dev->scsi_dev->host;
	return host ? shost_priv(host) : NULL;
}

static void get_ram_size(void)
{
	struct sysinfo si = {0};
	unsigned int total_kb;
	unsigned int in_gb;
	int ram;

	si_meminfo(&si);

	/*
	 * Stock computes the size in kB as a 32-bit value, so only the lower
	 * bits of si.totalram take part.
	 */
	total_kb = si.totalram << (PAGE_SHIFT - 10);
	in_gb = total_kb >> 20;
	printk("hardwareinfo-> in_mega is %u\n", in_gb);

	if (in_gb >= 256) {
		if (!g_pre_dev->ram)
			g_pre_dev->ram = 1;
	}
	else if (in_gb >= 128)
		g_pre_dev->ram = 256;
	else if (in_gb >= 64)
		g_pre_dev->ram = 128;
	else if (in_gb >= 32)
		g_pre_dev->ram = 64;
	else if (in_gb >= 16)
		g_pre_dev->ram = 32;
	else if (in_gb >= 12)
		g_pre_dev->ram = 16;
	else if (in_gb >= 8)
		g_pre_dev->ram = 12;
	else if (in_gb >= 4)
		g_pre_dev->ram = 8;
	else if (in_gb >= 3)
		g_pre_dev->ram = 4;
	else if (in_gb >= 2)
		g_pre_dev->ram = 3;
	else if (in_gb >= 1)
		g_pre_dev->ram = 2;
	else {
		/* Unrecognised size, keep whatever was detected before. */
		if (!g_pre_dev->ram)
			g_pre_dev->ram = 1;
	}
	ram = g_pre_dev->ram;

	printk("hardwareinfo-> ram size is %dGB\n", ram);
	sprintf(g_pre_dev->ram_str, "%dGB", ram);
}

static void get_ufs_size(void)
{
	struct ufs_hba *hba;
	u64 capacity = 0;	/* qTotalRawDeviceCapacity, 512 byte units */
	u64 ufs_size;

	hba = get_ufs_hba_data();
	g_pre_dev->hba = hba;
	if (!hba) {
		pr_err("%s:hardwareinfo-> get hba struct fail\n", __func__);
		strcpy(g_pre_dev->rom_str, "error!");
		return;
	}

	ufs_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY, 0,
			    GEOMETRY_DESC_PARAM_DEV_CAP, (u8 *)&capacity,
			    sizeof(capacity));
	ufs_size = (capacity << 9) >> 30;	/* GB */
	printk("hardwareinfo-> ufs size %llu\n", ufs_size);

	if (ufs_size > 512 && ufs_size <= 1024)
		g_pre_dev->rom = 1024;
	else if (ufs_size > 256)
		g_pre_dev->rom = 512;
	else if (ufs_size > 128)
		g_pre_dev->rom = 256;
	else if (ufs_size > 64)
		g_pre_dev->rom = 128;
	else if (ufs_size > 32)
		g_pre_dev->rom = 64;
	else if (ufs_size > 16)
		g_pre_dev->rom = 32;
	else if (ufs_size > 8)
		g_pre_dev->rom = 8;
	else {
		g_pre_dev->rom = 0;
		pr_info("hardwareinfo-> unkonwn ufs size %llu\n", ufs_size);
	}

	sprintf(g_pre_dev->rom_str, "%dGB", g_pre_dev->rom);
}

void load_preloader_devices(int (*register_hw)(enum hardware_id, char *))
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(pre_dev_maps); i++)
		register_hw(pre_dev_maps[i].id, pre_dev_maps[i].dev_name);

	g_pre_dev = kzalloc(sizeof(*g_pre_dev), GFP_KERNEL);
	if (!g_pre_dev) {
		printk("hardwareinfo-> init g_pre_dev failed!\n");
		return;
	}

	get_ram_size();
	register_hw(HWID_DDR, g_pre_dev->ram_str);

	get_ufs_size();
	register_hw(HWID_ROM, g_pre_dev->rom_str);

	pr_info("config is %s, stage is %s\n", config, stage);
	if (config[0] && stage[0])
		sprintf(g_pre_dev->pcba, "PCBA_ELDEN_%s_%s", stage, config);
	else
		strcpy(g_pre_dev->pcba, "PCBA_ELDEN_UNKNOW");
	register_hw(HWID_PCBA, g_pre_dev->pcba);
}

/*
 * Stock (.text 0x6b4) forwards fmt to kobject_init_and_add() and leaves the
 * caller's variadic registers in x4..x7 untouched, effectively forwarding
 * those arguments as well.
 */
int register_kboj_under_hqsysfs(struct kobject *kobj, struct kobj_type *ktype,
				const char *fmt, ...)
{
	return kobject_init_and_add(kobj, ktype, &huaqin_hw_device->kobj, fmt);
}
EXPORT_SYMBOL_GPL(register_kboj_under_hqsysfs);

int hq_deregister_hw_info(enum hardware_id id, char *device_name)
{
	int ret = 0;
	int find_hw_id = 0;
	int iterator = 0;
	struct hq_hw_info *hw;
	struct attribute *attr = huaqin_attrs[iterator];

	if (!device_name) {
		pr_err("[%s]: device_name does not allow empty\n", __func__);
		ret = -2;
		goto err;
	}

	while (attr) {
		hw = container_of(attr, struct hq_hw_info, attr);
		iterator += 1;
		attr = huaqin_attrs[iterator];

		if (!hw)
			continue;

		if (id == hw->id) {
			find_hw_id = 1;
			if (!hw->is_set) {
				pr_err("[%s]: device has not registed hw->id:0x%x . Cant be deregistered\n",
				       __func__, id);
				ret = -4;
				goto err;
			}
			if (!hw->dev_name) {
				pr_err("[%s]:hw_id is 0x%x Device name cant be NULL\n",
				       __func__, id);
				ret = -5;
				goto err;
			}
			if (strncmp(hw->dev_name, device_name, strlen(hw->dev_name))) {
				pr_err("[%s]: hw_id is 0x%x Registered device name %s, want to deregister: %s\n",
				       __func__, id, hw->dev_name, device_name);
				ret = -6;
				goto err;
			}
			hw->dev_name = NULL;
			hw->is_set = 0;
			break;
		}
	}

	if (!find_hw_id) {
		pr_err("[%s]: Cant find correct hardware_id: 0x%x\n", __func__, id);
		ret = -3;
	}
err:
	return ret;
}
EXPORT_SYMBOL(hq_deregister_hw_info);

int hq_regiser_hw_info(enum hardware_id id, char *device_name)
{
	int ret = 0;
	int find_hw_id = 0;
	int iterator = 0;
	struct hq_hw_info *hw;
	struct attribute *attr = huaqin_attrs[iterator];

	if (!device_name) {
		pr_err("[%s]: device_name does not allow empty\n", __func__);
		ret = -2;
		goto err;
	}

	while (attr) {
		hw = container_of(attr, struct hq_hw_info, attr);
		iterator += 1;
		attr = huaqin_attrs[iterator];

		if (!hw)
			continue;

		if (id == hw->id) {
			find_hw_id = 1;
			if (hw->is_set) {
				pr_err("[%s]: device has already registed hw->id:0x%x hw_device_name:%s\n",
				       __func__, id, hw->dev_name);
				ret = -4;
				goto err;
			}
			hw->dev_name = device_name;
			hw->is_set = 1;
			break;
		}
	}

	if (!find_hw_id) {
		pr_err("[%s]: Cant find correct hardware_id: 0x%x\n", __func__, id);
		ret = -3;
	}
err:
	return ret;
}
EXPORT_SYMBOL(hq_regiser_hw_info);

static int __init create_sysfs(void)
{
	int ret;

	huaqin_class = class_create(HQ_CLASS_NAME);
	if (IS_ERR(huaqin_class))
		return PTR_ERR(huaqin_class);

	huaqin_hw_device = device_create(huaqin_class, NULL, 0, NULL,
					  HQ_DEV_NAME);
	if (IS_ERR(huaqin_hw_device)) {
		ret = PTR_ERR(huaqin_hw_device);
		class_destroy(huaqin_class);
		return ret;
	}

	ret = kobject_init_and_add(&huaqin_kobj, &huaqin_ktype,
				   &huaqin_hw_device->kobj, HQ_KOBJ_NAME);
	if (ret < 0) {
		kobject_put(&huaqin_kobj);
		device_destroy(huaqin_class, 0);
		class_destroy(huaqin_class);
		return ret;
	}

	load_preloader_devices(hq_regiser_hw_info);

	return ret;
}

static int __init hqsysfs_init(void)
{
	return create_sysfs();
}

static void __exit hqsysfs_exit(void)
{
	if (!huaqin_class)
		return;

	kobject_put(&huaqin_kobj);
	if (g_pre_dev) {
		if (g_pre_dev->scsi_dev)
			scsi_device_put(g_pre_dev->scsi_dev);
		kfree(g_pre_dev);
	}
	device_destroy(huaqin_class, 0);
	class_destroy(huaqin_class);
}

module_init(hqsysfs_init);
module_exit(hqsysfs_exit);

MODULE_AUTHOR("KaKa Ni <nigang@huaqin.com>");
MODULE_DESCRIPTION("Huaqin Hardware Info Driver");
MODULE_LICENSE("GPL");
