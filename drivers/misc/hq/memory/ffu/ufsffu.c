// SPDX-License-Identifier: GPL-2.0
/*
 * Reconstructed from the Lenovo/Huaqin stock ufsffu.ko.
 *
 * This source is a behavioral reconstruction, not the original Lenovo
 * source. The driver loads an OEM ffu_config.txt table,
 * matches a UFS device, publishes per-device controls, and performs UFS FFU
 * with SCSI WRITE BUFFER commands.
 */
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <linux/firmware.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/blkdev.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_host.h>
#include <ufs/ufshcd.h>

#define FFU_CONFIG "ffu_config.txt"
#define FFU_CHUNK (512 * 1024)
#define FFU_MAX_HOSTS 6
#define FFU_VENDOR_LEN 9
#define FFU_PRODUCT_LEN 17
#define FFU_REV_LEN 5
#define FFU_NAME_LEN 65

enum ffu_state {
	FFU_FLAG_UNSET = 0,
	FFU_FLAG_OFF = 1,
	FFU_FLAG_ON = 2,
	FFU_FLAG_ERROR = 3,
};

struct fw_update_checklist {
	char vendor_id[FFU_VENDOR_LEN];
	char product_id[FFU_PRODUCT_LEN];
	char product_rev_from[FFU_REV_LEN];
	char product_rev_to[FFU_REV_LEN];
	char firmware_name[FFU_NAME_LEN];
	size_t firmware_size;
	void *fw_addr;
};
struct ffu_inquiry {
	char vendor_id[FFU_VENDOR_LEN];
	char product_id[FFU_PRODUCT_LEN];
	char product_rev[FFU_REV_LEN];
};

/* Private state uses the native 6.12 workqueue, kobject and mutex layouts. */
struct ffu_data {
	struct delayed_work ffu_delayed_work;
	struct scsi_device *sdev;
	char *part_info_part_name;
	struct kobject kobj;
	struct mutex sysfs_lock;
	struct ufsffu_sysfs_entry *sysfs_entries;
	u32 bootreason;
	u32 batterstatus;
	u8 off_charge_flag;
	u8 one_time_entry;
	bool sysfs_initialized;
};


static struct ffu_data *p_ffudata;
const struct firmware *cfg_entry;
const struct firmware *ufs_firmware;
static u8 need_update_k;



struct ufsffu_sysfs_entry {
	struct attribute attr;
	ssize_t (*show)(struct ffu_data *, char *);
	ssize_t (*store)(struct ffu_data *, const char *, size_t);
};
int start_ffu(struct ffu_data *data);
static ssize_t ufsffu_attr_show(struct kobject *, struct attribute *, char *);
static ssize_t ufsffu_attr_store(struct kobject *, struct attribute *,
			       const char *, size_t);
static ssize_t ufsffu_sysfs_show_ffu_flag(struct ffu_data *, char *);
static void ffu_ffu_delayed_work(struct work_struct *);
static noinline int ufs_fw_update_write(struct scsi_device *, int, const u8 *);
static noinline int ufs_scsi_write_buf(struct scsi_device *, const u8 *, u32, u32);

static const struct sysfs_ops ufsffu_sysfs_ops = {
	.show = ufsffu_attr_show,
	.store = ufsffu_attr_store,
};



int build_ffu_update_list(struct ufs_hba *hba,
			  struct fw_update_checklist **need_update,
			  struct ffu_inquiry *device)
{
	struct device *dev = hba->dev;
	char *value;
	size_t len;
	int list_count = 0, alloc_count, skip, field, ret;

	ret = request_firmware(&cfg_entry, FFU_CONFIG, dev);
	if (!ret)
		goto config_loaded;
	pr_err("ufs_ffu: load %s failed, ret=%d\n", FFU_CONFIG, ret);
	ret = request_firmware(&cfg_entry, FFU_CONFIG, dev);
	if (!ret)
		goto config_loaded;
	pr_err("ufs_ffu: load %s failed, ret=%d\n", FFU_CONFIG, ret);
	ret = request_firmware(&cfg_entry, FFU_CONFIG, dev);
	if (!ret)
		goto config_loaded;
	pr_err("ufs_ffu: load %s failed, ret=%d\n", FFU_CONFIG, ret);
	pr_err("ufs_ffu: error, retry=%d\n", 3);
config_loaded:
	if (!cfg_entry)
		goto list_done;
	pr_notice("ufs_ffu: cfg size: %zu\n", cfg_entry->size);
	pr_notice("ufs_ffu: cfg data: %s\n", cfg_entry->data);
	/* Keep auto-zeroed cursor lifetimes at the stock parsing call sites. */
	char *config_data;

	config_data = kstrdup(cfg_entry->data, GFP_KERNEL);
	char *record;

	record = strsep(&config_data, "\n");
	while (record && *record) {
		value = strsep(&record, " ");
		while (value && *value) {
			if (!strncmp(value, device->product_id, strlen(value)))
				list_count++;
			value = strsep(&record, " ");
		}
		record = strsep(&config_data, "\n");
	}
	pr_notice("ufs_ffu: maybe need update ufs count:%d\n", list_count);
	if (list_count <= 0) {
empty_list:
		*need_update = (struct fw_update_checklist *)&need_update_k;
	} else {
		alloc_count = list_count;
		*need_update = kmalloc(alloc_count * sizeof(**need_update), GFP_KERNEL);
		if (IS_ERR_OR_NULL(*need_update)) {
			pr_err("ufs_ffu: kmalloc error!\n");
			list_count = 0;
			goto empty_list;
		}
		memset(*need_update, 0, list_count * sizeof(**need_update));
		kfree(config_data);
		config_data = kstrdup(cfg_entry->data, GFP_KERNEL);
		record = NULL;
		record = strsep(&config_data, "\n");
		list_count = 0;
		/*
		 * The first pass only counts records naming this product, but
		 * every record from the same vendor is parsed into the next slot.
		 * Stop once the allocation is full instead of writing past it.
		 */
		while (record && *record && list_count < alloc_count) {
			value = strsep(&record, " ");
			if (!value)
				goto next_record;
			skip = 0;
			field = 0;
			while (!skip) {
				if (!*value)
					break;
				pr_notice("ufs_ffu: %d:%s, struct item:%d\n",
					  field, value, list_count);
				switch (field) {
				case 0:
					len = min(strlen(value), FFU_VENDOR_LEN - 1);
					if (strncmp(value, device->vendor_id, len)) {
						pr_notice("ufs_ffu: not %s vendor_id, skip!",
							  value);
						skip = 1;
						goto next_field;
					}
#line 308 "drivers/misc/hq/memory/ffu/ufsffu.c"
					memcpy(((*need_update) + list_count)->vendor_id, value, len);
					break;
				case 1:
					len = min(strlen(value), FFU_PRODUCT_LEN - 1);
					if (strncmp(value, device->product_id, len)) {
						pr_notice("ufs_ffu: not %s product_id, skip!",
							  value);
						skip = 1;
						goto next_field;
					}
#line 319 "drivers/misc/hq/memory/ffu/ufsffu.c"
					memcpy(((*need_update) + list_count)->product_id, value, len);
					break;
				case 2:
					len = min(strlen(value), FFU_REV_LEN - 1);
#line 323 "drivers/misc/hq/memory/ffu/ufsffu.c"
					memcpy(((*need_update) + list_count)->product_rev_from, value, len);
					break;
				case 3:
					len = min(strlen(value), FFU_REV_LEN - 1);
#line 327 "drivers/misc/hq/memory/ffu/ufsffu.c"
					memcpy(((*need_update) + list_count)->product_rev_to, value, len);
					break;
				case 4:
					len = min(strlen(value), FFU_NAME_LEN - 1);
#line 331 "drivers/misc/hq/memory/ffu/ufsffu.c"
					memcpy(((*need_update) + list_count)->firmware_name, value, len);
					ret = request_firmware(&ufs_firmware, value, dev);
					if (ret) {
						pr_err("ufs_ffu: load %s failed, ret=%d\n",
						       value, ret);
						goto list_done;
					}
					((*need_update) + list_count)->firmware_size =
						ufs_firmware->size;
					((*need_update) + list_count)->fw_addr =
						kvmalloc(ufs_firmware->size + 1, GFP_KERNEL);
					if (!((*need_update) + list_count)->fw_addr) {
						pr_err("ufs_ffu: can't alloc memory for %s!",
						       value);
					} else {
						memcpy(((*need_update) + list_count)->fw_addr,
						       ufs_firmware->data, ufs_firmware->size);
					}
					pr_notice("ufs_ffu: %s, index:%d, fw_size:%zu\n",
						  value, field,
						  ((*need_update) + list_count)->firmware_size);
					if (ufs_firmware)
						release_firmware(ufs_firmware);
					skip = 0;
					list_count++;
					goto next_field;
				}
				skip = 0;
next_field:
				field++;
				value = strsep(&record, " ");
				if (!value)
					break;
			}
next_record:
			record = strsep(&config_data, "\n");
		}
		kfree(config_data);
	}
list_done:
	pr_notice("ufs_ffu: list_count:%d\n", list_count);
	return list_count;
}
static char boot_mode[10];
module_param_string(boot_mode, boot_mode, sizeof(boot_mode), 0444);
MODULE_PARM_DESC(boot_mode, "This is a flag to check boot mode!");

static int ffu_flag = 1;
module_param(ffu_flag, int, 0644);
MODULE_PARM_DESC(ffu_flag, "This is a flag to control do ffu or not!");

int start_ffu(struct ffu_data *data)
{
	struct fw_update_checklist *items;
	struct ffu_inquiry device = {};
	struct ufs_hba *hba = shost_priv(data->sdev->host);
	const u8 *inquiry = hba->ufs_device_wlun->inquiry;
	int item_count, ret = 0;
	unsigned int i;

	memcpy(device.vendor_id, inquiry + 8, sizeof(device.vendor_id) - 1);
	items = NULL;
	memcpy(device.product_id, inquiry + 16, sizeof(device.product_id) - 1);
	memcpy(device.product_rev, inquiry + 32, sizeof(device.product_rev) - 1);
	pr_err("ufs_ffu: check ffu_arry, vendor:%s, product:%s, fw_rev:%s\n",
	       device.vendor_id, device.product_id, device.product_rev);
	item_count = build_ffu_update_list(hba, &items, &device);
	for (i = 0; i < item_count; i++) {
		struct fw_update_checklist *item = &items[i];
		char *restart_message;

		pr_info("ufs_ffu: firmware_name:%s vendor_id:%s, product_id:%s from->to(%s - %s), fw_size:%zu\n",
			  item->firmware_name, item->vendor_id, item->product_id,
			  item->product_rev_from, item->product_rev_to,
			  item->firmware_size);
		if (strncmp(device.vendor_id, item->vendor_id,
			    strlen(item->vendor_id)))
			continue;
		if (strncmp(device.product_id, item->product_id,
			    strlen(item->product_id)))
			continue;
		pr_info("ufs_ffu: match vendor:%s, product id:%s hw_sectors %d\n",
			device.vendor_id, device.product_id,
			data->sdev->request_queue->limits.max_hw_sectors);
		if (!strncmp(device.product_rev, item->product_rev_from,
			     strlen(item->product_rev_from))) {
			pr_info("ufs_ffu: match product revision from:%s\n",
				device.product_rev);
			pr_info("ufs_ffu: start write buffer\n");
			pr_info("ufs_ffu: fw_size = %zu\n",
				item->firmware_size);
			ret = ufs_fw_update_write(data->sdev, item->firmware_size,
						  item->fw_addr);
			if (ret) {
				restart_message = "ffu update failed!";
				pr_err("ufs_ffu: ufs ffu failed, don't clear fail flag, reboot directly\n");
			} else {
				restart_message = "ffu update success!";
				pr_notice("ufs_ffu: write buffer success\n");
				pr_info("ufs_ffu: clear fail flag, check revision after reboot\n");
			}
			msleep(20);
			kernel_restart(restart_message);
			break;
		}
		if (!strncmp(device.product_rev, item->product_rev_to,
			     strlen(item->product_rev_to)))
			pr_notice("ufs_ffu: ufs_ffu firmware is already the newest now\n");
	}
	if (cfg_entry)
		release_firmware(cfg_entry);
	return ret;
}
#line 288 "ufsffu.c"
static int __init ufs_ffu_init(void)
{
	struct ffu_data *data;

	data = kmalloc(sizeof(*data), GFP_KERNEL | __GFP_ZERO);
	if (!data)
		return -ENOMEM;
	p_ffudata = data;
	pr_notice("ufs_ffu: %s---\n ", __func__);
	INIT_DELAYED_WORK(&data->ffu_delayed_work, ffu_ffu_delayed_work);
	queue_delayed_work_on(WORK_CPU_UNBOUND, system_wq, &data->ffu_delayed_work, 3);
	return 0;
}

static void __exit ufsffu_exit(void)
{
	struct ffu_data *data = p_ffudata;

	cancel_delayed_work_sync(&data->ffu_delayed_work);
	if (data->sysfs_initialized) {
		kobject_uevent(&data->kobj, KOBJ_REMOVE);
		kobject_put(&data->kobj);
	} else {
		kfree(data);
	}
	pr_notice("ufs_ffu: %s:exit!", "ufs_ffu_exit");
}

static noinline int ufs_fw_update_write(struct scsi_device *sdev, int firmware_size,
			const u8 *firmware_data)
{
	u8 *firmware_buf;
	u32 offset, size;
	int ret = 0;

	firmware_buf = kmalloc(min_t(size_t, firmware_size, FFU_CHUNK), GFP_KERNEL);
	if (!firmware_buf)
		return -ENOMEM;

	/* SCSI needs a directly mapped buffer; firmware may be vmalloc-backed. */
	for (offset = 0; offset < firmware_size; offset += size) {
		size = min_t(u32, firmware_size - offset, FFU_CHUNK);
		memcpy(firmware_buf, firmware_data + offset, size);
		ret = ufs_scsi_write_buf(sdev, firmware_buf, offset, size);
		if (ret)
			break;
	}
	kfree(firmware_buf);
	return ret;
}
static noinline int ufs_scsi_write_buf(struct scsi_device *sdev, const u8 *data,
				       u32 offset, u32 size)
{
	struct scsi_sense_hdr sshdr = {};
	u8 cdb[10];
	struct scsi_exec_args args = {
		.sshdr = &sshdr,
		.req_flags = BLK_MQ_REQ_PM,
	};
	struct ufs_hba *hba = shost_priv(sdev->host);
	unsigned long irq_flags;
	int ret;
	spin_lock_irqsave(hba->host->host_lock, irq_flags);
	ret = scsi_device_get(sdev);
	if (!ret && !scsi_device_online(sdev)) {
		scsi_device_put(sdev);
		pr_err("ufs_ffu: get device fail\n");
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, irq_flags);
	if (ret)
		return ret;
	hba->host->eh_noresume = true;
	cdb[0] = WRITE_BUFFER;
	cdb[1] = 0x0e;
	cdb[2] = 0;
	cdb[3] = (offset >> 16) & 0xff;
	cdb[4] = (offset >> 8) & 0xff;
	cdb[5] = offset & 0xff;
	cdb[6] = (size >> 16) & 0xff;
	cdb[7] = (size >> 8) & 0xff;
	cdb[8] = size & 0xff;
	cdb[9] = 0;
	ret = scsi_execute_cmd(sdev, cdb, REQ_OP_DRV_OUT, (void *)data, size,
			       3750, 0, &args);
	if (ret)
		sdev_printk(KERN_ERR, sdev,
			    "WRITE BUFFER failed for firmware upgrade %d\n", ret);
	sdev_printk(KERN_ERR, sdev,
		    "sense key:0x%x; asc:0x%x; ascq:0x%x\n",
		    sshdr.sense_key, sshdr.asc, sshdr.ascq);
	scsi_device_put(sdev);
	hba->host->eh_noresume = false;
	return ret;
}



static struct ufsffu_sysfs_entry ufsffu_sysfs_entries[2] = {
	{
		.attr = {
			.name = "ffu_flag",
			.mode = 0444,
		},
		.show = ufsffu_sysfs_show_ffu_flag,
		.store = NULL,
	},
};

static void ufsffu_release(struct kobject *kobj)
{
	kfree(container_of(kobj, struct ffu_data, kobj));
}

static struct kobj_type ufsffu_ktype = {
	.release = ufsffu_release,
	.sysfs_ops = &ufsffu_sysfs_ops,
};
static __always_inline void
ufsffu_create_sysfs(struct ffu_data *ffudata, struct device *dev)
{
	struct ufsffu_sysfs_entry *entry;
	int ret;

	ffudata->sysfs_entries = ufsffu_sysfs_entries;
	kobject_init(&ffudata->kobj, &ufsffu_ktype);
	ffudata->sysfs_initialized = true;
	mutex_init(&ffudata->sysfs_lock);
	pr_notice("ufs_ffu: creates sysfs ufsffu %p dev->kobj %p",
		  &ffudata->kobj, &dev->kobj);
	ret = kobject_add(&ffudata->kobj, &dev->kobj, "ufsffu");
	if (ret) {
		pr_err("ufs_ffu: kobject_add failed");
		return;
	}
	for (entry = ffudata->sysfs_entries; entry->attr.name; entry++) {
		pr_notice("ufs_ffu: sysfs attr creates: %s", entry->attr.name);
		ret = sysfs_create_file_ns(&ffudata->kobj, &entry->attr, NULL);
		if (ret) {
			pr_err("ufs_ffu: create entry(%s) failed",
			       entry->attr.name);
			ret = kobject_uevent(&ffudata->kobj, KOBJ_REMOVE);
			pr_notice("ufs_ffu: kobject removed (%d)", ret);
			kobject_del(&ffudata->kobj);
			return;
		}
	}
	kobject_uevent(&ffudata->kobj, KOBJ_ADD);
}


static void ffu_ffu_delayed_work(struct work_struct *work)
{
	struct ffu_data *data = container_of(to_delayed_work(work),
					     struct ffu_data, ffu_delayed_work);
	struct Scsi_Host *shost;
	u64 host;
	const char *notice = KERN_NOTICE "ufs_ffu: one time entry is true\n";

	p_ffudata = data;
	*(u16 *)&data->off_charge_flag = 0;
	pr_notice("ufs_ffu: ufs_ffu boot_mode is %s\n", boot_mode);
	if (!strcmp(boot_mode, "charger"))
		data->off_charge_flag = 1;
	if (data->off_charge_flag & 0xff)
		pr_notice("ufs_ffu: ufs_ffu enter off charger\n");
	if (data->off_charge_flag & 0xff) {
		pr_err("ufs_ffu: off_charge_flag is set\n");
		goto done;
	}

	host = 0;
	do {
		const char *proc_name;

		pr_notice("ufs_ffu: lun = %d\n", (int)host);
		shost = scsi_host_lookup(0);
		if (!shost)
			goto next_lun;
		data->sdev = scsi_device_lookup(shost, 0, 0, host);
		scsi_host_put(shost);
		if (!data->sdev)
			goto next_lun;
		if (data->one_time_entry)
			goto print_notice;
		proc_name = data->sdev->host->hostt->proc_name;
		if (!proc_name) {
			notice = KERN_NOTICE "ufs_ffu: proc name is null\n";
			goto print_notice;
		}
		if (strncmp(proc_name, "ufshcd", 6)) {
			notice = KERN_NOTICE "ufs_ffu: proc name is not ufs\n";
			goto print_notice;
		}
		ufsffu_create_sysfs(data,
				   ((struct ufs_hba *)shost_priv(data->sdev->host))->dev);
		if (ffu_flag == 1)
			start_ffu(data);
		break;
next_lun:
		host++;
	} while (host != FFU_MAX_HOSTS);
	data->one_time_entry = 1;
	goto done;
print_notice:
	printk("%s", notice);
done:
	if (data->sdev) {
		scsi_device_put(data->sdev);
		data->sdev = NULL;
	}
	return;
}
static ssize_t ufsffu_sysfs_show_ffu_flag(struct ffu_data *data,
					  char *buf)
{
	return snprintf(buf, 4, "%d\n", ffu_flag);
}

static ssize_t ufsffu_attr_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct ffu_data *data = container_of(kobj, struct ffu_data, kobj);
	struct ufsffu_sysfs_entry *entry =
		container_of(attr, struct ufsffu_sysfs_entry, attr);
	ssize_t ret;

	if (!entry->show)
		return -EIO;
	mutex_lock(&data->sysfs_lock);
	ret = entry->show(data, buf);
	mutex_unlock(&data->sysfs_lock);
	return ret;
}


static ssize_t ufsffu_attr_store(struct kobject *kobj, struct attribute *attr,
				 const char *buf, size_t count)
{
	struct ffu_data *data = container_of(kobj, struct ffu_data, kobj);
	struct ufsffu_sysfs_entry *entry =
		container_of(attr, struct ufsffu_sysfs_entry, attr);
	ssize_t ret;

	if (!entry->store)
		return -EIO;
	mutex_lock(&data->sysfs_lock);
	ret = entry->store(data, buf, count);
	mutex_unlock(&data->sysfs_lock);
	return ret;
}
module_init(ufs_ffu_init);
module_exit(ufsffu_exit);
MODULE_AUTHOR("hq bsp system team");
MODULE_DESCRIPTION("ufs firware ffu driver");
MODULE_LICENSE("GPL");
