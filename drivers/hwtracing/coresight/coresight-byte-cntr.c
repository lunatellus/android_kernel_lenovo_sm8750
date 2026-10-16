// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/of_irq.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/property.h>
#include <linux/jiffies.h>

#include "coresight-priv.h"
#include "coresight-byte-cntr.h"
#include "coresight-common.h"

#define CSR_BYTECNTVAL		(0x06C)

static void tmc_etr_read_bytes(struct byte_cntr *byte_cntr_data, long offset,
			       size_t bytes, size_t *len, char **bufp)
{
	struct tmc_drvdata *tmcdrvdata = byte_cntr_data->tmcdrvdata;
	struct etr_buf *etr_buf = tmcdrvdata->sysfs_buf;
	size_t actual;

	/*
	 * sysfs_buf can be freed and set to NULL concurrently by the ETR
	 * disable path (tmc_etr_sync_sysfs_buf).  Signal the caller by
	 * zeroing *len so the read path can propagate the error.
	 */
	if (!etr_buf) {
		*len = 0;
		return;
	}

	if (*len >= bytes)
		*len = bytes;
	else if (((uint32_t)offset % bytes) + *len > bytes)
		*len = bytes - ((uint32_t)offset % bytes);

	actual = tmc_etr_buf_get_data(etr_buf, offset, *len, bufp);
	*len = actual;
	if ((actual == bytes || (actual + (uint32_t)offset) % bytes == 0)) {
		if (atomic_dec_if_positive(&byte_cntr_data->irq_cnt) < 0) {
			/* Counter was already zero, restore it */
			atomic_inc(&byte_cntr_data->irq_cnt);
		}
	}
}


static irqreturn_t etr_handler(int irq, void *data)
{
	struct byte_cntr *byte_cntr_data = data;
	struct tmc_drvdata *tmcdrvdata = byte_cntr_data->tmcdrvdata;

	if (tmcdrvdata->out_mode == TMC_ETR_OUT_MODE_USB) {
		atomic_inc(&byte_cntr_data->irq_cnt);
		wake_up(&byte_cntr_data->usb_wait_wq);
	} else if (tmcdrvdata->out_mode == TMC_ETR_OUT_MODE_MEM) {
		atomic_inc(&byte_cntr_data->irq_cnt);
		wake_up(&byte_cntr_data->wq);
	}

	byte_cntr_data->total_irq++;

	return IRQ_HANDLED;
}


static long tmc_etr_flush_remaining_bytes(struct tmc_drvdata *tmcdrvdata, long offset,
			size_t len, char **bufpp)
{
	long req_size, actual = 0;
	struct etr_buf *etr_buf;
	struct byte_cntr *byte_cntr_data;

	if (!tmcdrvdata)
		return -EINVAL;

	byte_cntr_data = tmcdrvdata->byte_cntr;
	if (!byte_cntr_data)
		return -EINVAL;

	etr_buf = tmcdrvdata->sysfs_buf;
	/*
	 * sysfs_buf may have been freed and NULL'd by tmc_etr_sync_sysfs_buf
	 * before we get here.  Treat this as no data available.
	 */
	if (!etr_buf)
		return -EINVAL;

	req_size = ((byte_cntr_data->rwp_offset < offset) ? tmcdrvdata->size : 0) +
		byte_cntr_data->rwp_offset - offset;

	if (req_size > len)
		req_size = len;

	if (req_size > 0)
		actual = tmc_etr_buf_get_data(etr_buf, offset, req_size, bufpp);

	return actual;
}


static ssize_t tmc_etr_byte_cntr_read(struct file *fp, char __user *data,
				       size_t len, loff_t *ppos)
{
	struct byte_cntr *byte_cntr_data = fp->private_data;
	struct tmc_drvdata *tmcdrvdata = byte_cntr_data->tmcdrvdata;
	char *bufp = NULL;
	long actual, rwp, avail, req_size;
	int ret;

	if (!data)
		return -EINVAL;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);

	/*
	 * ----------------------------------------------------------------
	 * Phase 1: Tracing already stopped.
	 * Drain any remaining data and signal EOF when done.
	 * ----------------------------------------------------------------
	 */
	if (!byte_cntr_data->read_active)
		goto flush_and_eof;

	/*
	 * ----------------------------------------------------------------
	 * Phase 2: Tracing is active.
	 * Loop until a block of data is ready, then deliver it.
	 * The loop exits when:
	 *   - data is available and copied to user space  (goto copy)
	 *   - tracing stops                               (goto flush_and_eof)
	 *   - the process receives a signal               (return -ERESTARTSYS)
	 * ----------------------------------------------------------------
	 */
	while (byte_cntr_data->enable) {

		/*
		 * Step 2a - Wait for data when no IRQ has fired yet.
		 *
		 * If the ETR has already written more than one block since
		 * the last read (req_size > block_size), skip the wait and
		 * go straight to the phantom-IRQ guard in Step 2b.
		 */
		if (!atomic_read(&byte_cntr_data->irq_cnt)) {
			rwp = tmc_get_rwp_offset(tmcdrvdata);
			if (rwp < 0)
				goto err;
			req_size = ((rwp < byte_cntr_data->offset) ?
				    tmcdrvdata->size : 0)
				    + rwp - byte_cntr_data->offset;

			if (req_size <= (long)byte_cntr_data->block_size) {
				mutex_unlock(&byte_cntr_data->byte_cntr_lock);
				/*
				 * Wait for an IRQ (block_size bytes written) or
				 * for tracing to stop.
				 */
				ret = wait_event_interruptible_timeout(
					byte_cntr_data->wq,
					atomic_read(&byte_cntr_data->irq_cnt) > 0
					|| !byte_cntr_data->enable,
					msecs_to_jiffies(5000));
				mutex_lock(&byte_cntr_data->byte_cntr_lock);

				if (ret < 0) {
					/* Interrupted by a signal. */
					mutex_unlock(&byte_cntr_data->byte_cntr_lock);
					return -ERESTARTSYS;
				}

				if (!ret) {
					/*
					 * 5-second timeout: flush whatever the
					 * ETR has written since we last checked.
					 */
					dev_dbg(&tmcdrvdata->csdev->dev,
						"timeout: irq_cnt: %d, req_size: 0x%lx, rwp offset %lx, offset %lx\n",
						atomic_read(&byte_cntr_data->irq_cnt),
						req_size, rwp,
						byte_cntr_data->offset);
					byte_cntr_data->rwp_offset =
						tmc_get_rwp_offset(tmcdrvdata);
					if (byte_cntr_data->rwp_offset < 0)
						goto err;
					actual = tmc_etr_flush_remaining_bytes(
						tmcdrvdata,
						byte_cntr_data->offset,
						len, &bufp);
					if (actual > 0) {
						len = actual;
						goto copy;
					} else if (actual < 0) {
						/* Error occurred during flush */
						mutex_unlock(&byte_cntr_data->byte_cntr_lock);
						return actual;
					}
					/* avail == 0: loop back and wait for new data. */
					continue;
				}

				/* Woken by IRQ, stop event, or req_size. */
				if (!byte_cntr_data->read_active)
					goto flush_and_eof;
			}
		}

		/*
		 * Step 2b - Phantom-IRQ guard.
		 *
		 * When req_size > block_size caused the wait to return
		 * immediately, we read one block below without consuming an
		 * IRQ (irq_cnt stays 0).  The hardware IRQ fires shortly
		 * after for that already-consumed block, making irq_cnt = 1.
		 * On the next call we arrive here with irq_cnt > 0 but the
		 * ETR may have written fewer than block_size bytes from the
		 * new offset, so a blind tmc_etr_read_bytes() would include
		 * stale wrap-around data.
		 *
		 * Re-read the hardware RWP.  If fewer than block_size bytes
		 * are available the irq_cnt is stale: consume it and flush
		 * only the genuine data.  If avail == 0 loop back to wait.
		 */
		rwp = tmc_get_rwp_offset(tmcdrvdata);
		if (rwp < 0)
			goto err;
		avail = ((rwp < (long)byte_cntr_data->offset) ?
			 (long)tmcdrvdata->size : 0)
			 + rwp - (long)byte_cntr_data->offset;

		if (avail < (long)byte_cntr_data->block_size) {
			atomic_dec_if_positive(&byte_cntr_data->irq_cnt);
			byte_cntr_data->rwp_offset = rwp;
			actual = tmc_etr_flush_remaining_bytes(
				tmcdrvdata, byte_cntr_data->offset,
				len, &bufp);
			if (actual > 0) {
				len = actual;
				goto copy;
			} else if (actual < 0) {
				/* Error occurred during flush */
				mutex_unlock(&byte_cntr_data->byte_cntr_lock);
				return actual;
			}
			/* avail == 0: loop back and wait for new data. */
			continue;
		}

		/*
		 * Step 2c - At least block_size bytes are available.
		 * Read exactly one block and deliver it.
		 */
		tmc_etr_read_bytes(byte_cntr_data, byte_cntr_data->offset,
				   byte_cntr_data->block_size, &len, &bufp);
		if (!len)
			goto err;
		goto copy;
	}

	/*
	 * ----------------------------------------------------------------
	 * Phase 3: enable == false (defensive path - normally unreachable).
	 * Update rwp_offset defensively before falling through to flush_and_eof.
	 * ----------------------------------------------------------------
	 */
	byte_cntr_data->rwp_offset = tmc_get_rwp_offset(tmcdrvdata);
	if (byte_cntr_data->rwp_offset < 0)
		goto err;

flush_and_eof:
	/*
	 * Flush whatever data remains between the current offset and
	 * rwp_offset (set by tmc_etr_byte_cntr_stop() or just above).
	 * Return -EINVAL to signal EOF when nothing is left.
	 */
	actual = tmc_etr_flush_remaining_bytes(tmcdrvdata,
			byte_cntr_data->offset, len, &bufp);
	if (actual > 0) {
		len = actual;
		goto copy;
	}
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);
	return -EINVAL;

copy:
	if (copy_to_user(data, bufp, len)) {
		mutex_unlock(&byte_cntr_data->byte_cntr_lock);
		dev_dbg(&tmcdrvdata->csdev->dev,
			"%s: copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	byte_cntr_data->total_size += len;
	if (byte_cntr_data->offset + len >= tmcdrvdata->size)
		byte_cntr_data->offset = 0;
	else
		byte_cntr_data->offset += len;

	mutex_unlock(&byte_cntr_data->byte_cntr_lock);
	return len;

err:
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);
	return -EINVAL;
}

void tmc_etr_byte_cntr_start(struct byte_cntr *byte_cntr_data)
{
	if (!byte_cntr_data)
		return;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);

	if (byte_cntr_data->block_size == 0
		|| byte_cntr_data->read_active) {
		mutex_unlock(&byte_cntr_data->byte_cntr_lock);
		return;
	}

	atomic_set(&byte_cntr_data->irq_cnt, 0);
	byte_cntr_data->enable = true;
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);
}
EXPORT_SYMBOL(tmc_etr_byte_cntr_start);

void tmc_etr_byte_cntr_stop(struct byte_cntr *byte_cntr_data)
{
	if (!byte_cntr_data)
		return;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);
	byte_cntr_data->rwp_offset =
		tmc_get_rwp_offset(byte_cntr_data->tmcdrvdata);
	byte_cntr_data->enable = false;
	byte_cntr_data->read_active = false;
	atomic_set(&byte_cntr_data->irq_cnt, 0);
	wake_up(&byte_cntr_data->wq);
	coresight_csr_set_byte_cntr(byte_cntr_data->csr,
				byte_cntr_data->irqctrl_offset, 0);
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);

}
EXPORT_SYMBOL(tmc_etr_byte_cntr_stop);


static int tmc_etr_byte_cntr_release(struct inode *in, struct file *fp)
{
	struct byte_cntr *byte_cntr_data = fp->private_data;
	struct device *dev = &byte_cntr_data->tmcdrvdata->csdev->dev;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);
	byte_cntr_data->read_active = false;

	atomic_set(&byte_cntr_data->irq_cnt, 0);

	if (byte_cntr_data->enable)
		coresight_csr_set_byte_cntr(byte_cntr_data->csr,
				byte_cntr_data->irqctrl_offset, 0);

	disable_irq_wake(byte_cntr_data->byte_cntr_irq);

	dev_dbg(dev, "send data total size: %lld bytes, irq_cnt: %lld, offset: %lu, rwp_offset: %lu\n",
		byte_cntr_data->total_size, byte_cntr_data->total_irq,
		byte_cntr_data->offset,	byte_cntr_data->rwp_offset);
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);

	return 0;
}

static int tmc_etr_byte_cntr_open(struct inode *in, struct file *fp)
{
	struct byte_cntr *byte_cntr_data =
			container_of(in->i_cdev, struct byte_cntr, dev);
	struct tmc_drvdata *tmcdrvdata = byte_cntr_data->tmcdrvdata;

	mutex_lock(&byte_cntr_data->byte_cntr_lock);

	if (byte_cntr_data->read_active) {
		mutex_unlock(&byte_cntr_data->byte_cntr_lock);
		return -EBUSY;
	}

	if (coresight_get_mode(tmcdrvdata->csdev) != CS_MODE_SYSFS ||
			!byte_cntr_data->block_size) {
		mutex_unlock(&byte_cntr_data->byte_cntr_lock);
		return -EINVAL;
	}

	enable_irq_wake(byte_cntr_data->byte_cntr_irq);
	/* IRQ is a '8- byte' counter and to observe interrupt at
	 * 'block_size' bytes of data
	 */

	coresight_csr_set_byte_cntr(byte_cntr_data->csr, byte_cntr_data->irqctrl_offset,
				(byte_cntr_data->block_size) / 8);

	fp->private_data = byte_cntr_data;
	nonseekable_open(in, fp);
	byte_cntr_data->enable = true;
	byte_cntr_data->read_active = true;
	byte_cntr_data->total_size = 0;
	byte_cntr_data->offset = tmc_get_rwp_offset(tmcdrvdata);
	byte_cntr_data->total_irq = 0;
	mutex_unlock(&byte_cntr_data->byte_cntr_lock);
	return 0;
}

static const struct file_operations byte_cntr_fops = {
	.owner		= THIS_MODULE,
	.open		= tmc_etr_byte_cntr_open,
	.read		= tmc_etr_byte_cntr_read,
	.release	= tmc_etr_byte_cntr_release,
};

static int byte_cntr_register_chardev(struct byte_cntr *byte_cntr_data)
{
	int ret;
	unsigned int baseminor = 0;
	unsigned int count = 1;
	struct device *device;
	dev_t dev;

	ret = alloc_chrdev_region(&dev, baseminor, count, byte_cntr_data->name);
	if (ret < 0) {
		pr_err("alloc_chrdev_region failed %d\n", ret);
		return ret;
	}
	cdev_init(&byte_cntr_data->dev, &byte_cntr_fops);

	byte_cntr_data->dev.owner = THIS_MODULE;
	byte_cntr_data->dev.ops = &byte_cntr_fops;

	ret = cdev_add(&byte_cntr_data->dev, dev, 1);
	if (ret)
		goto exit_unreg_chrdev_region;

	byte_cntr_data->driver_class = class_create(byte_cntr_data->class_name);
	if (IS_ERR(byte_cntr_data->driver_class)) {
		ret = -ENOMEM;
		pr_err("class_create failed %d\n", ret);
		goto exit_unreg_chrdev_region;
	}

	device = device_create(byte_cntr_data->driver_class, NULL,
			       byte_cntr_data->dev.dev, byte_cntr_data,
			       byte_cntr_data->name);

	if (IS_ERR(device)) {
		pr_err("class_device_create failed %d\n", ret);
		ret = -ENOMEM;
		goto exit_destroy_class;
	}

	return 0;

exit_destroy_class:
	class_destroy(byte_cntr_data->driver_class);
exit_unreg_chrdev_region:
	unregister_chrdev_region(byte_cntr_data->dev.dev, 1);
	return ret;
}

struct byte_cntr *byte_cntr_init(struct device *dev,
				 struct tmc_drvdata *drvdata)
{
	struct device_node *np = dev->of_node;
	int byte_cntr_irq;
	int ret;
	struct byte_cntr *byte_cntr_data;

	byte_cntr_irq = of_irq_get_byname(np, "byte-cntr-irq");
	if (byte_cntr_irq < 0)
		return NULL;

	byte_cntr_data = devm_kzalloc(dev, sizeof(*byte_cntr_data), GFP_KERNEL);
	if (!byte_cntr_data)
		return NULL;

	ret = devm_request_irq(dev, byte_cntr_irq, etr_handler,
			       IRQF_TRIGGER_RISING | IRQF_SHARED,
			       dev_name(dev), byte_cntr_data);
	if (ret) {
		dev_err(dev, "Byte_cntr interrupt registration failed\n");
		return NULL;
	}

	ret = of_property_read_u32(dev->of_node, "csr-irqctrl-offset",
				&byte_cntr_data->irqctrl_offset);

	if (ret) {
		dev_dbg(dev, "Get byte cntr csr irqctrl offset failed\n");
		byte_cntr_data->irqctrl_offset = CSR_BYTECNTVAL;
	}

	ret = device_property_read_string(dev, "byte-cntr-name", &byte_cntr_data->name);
	if (ret) {
		dev_dbg(dev, "Get byte cntr name failed\n");
		byte_cntr_data->name = "byte-cntr";
	}

	ret = device_property_read_string(dev, "byte-cntr-class-name",
				&byte_cntr_data->class_name);
	if (ret) {
		dev_dbg(dev, "Get byte cntr class name failed\n");
		byte_cntr_data->class_name = "coresight-tmc-etr-stream";
	}

	ret = byte_cntr_register_chardev(byte_cntr_data);
	if (ret) {
		dev_err(dev, "Byte_cntr char dev registration failed\n");
		return NULL;
	}

	byte_cntr_data->byte_cntr_irq = byte_cntr_irq;
	byte_cntr_data->csr = drvdata->csr;
	byte_cntr_data->tmcdrvdata = drvdata;
	atomic_set(&byte_cntr_data->irq_cnt, 0);
	init_waitqueue_head(&byte_cntr_data->wq);
	mutex_init(&byte_cntr_data->byte_cntr_lock);

	return byte_cntr_data;
}

void byte_cntr_remove(struct byte_cntr *byte_cntr_data)
{
	device_destroy(byte_cntr_data->driver_class,
				byte_cntr_data->dev.dev);
	class_destroy(byte_cntr_data->driver_class);
	unregister_chrdev_region(byte_cntr_data->dev.dev, 1);
}
