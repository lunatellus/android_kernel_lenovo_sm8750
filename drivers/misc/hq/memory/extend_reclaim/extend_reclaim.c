// SPDX-License-Identifier: GPL-2.0
/* Reconstructed from the stock extend_reclaim.ko for the 6.6.89 kernel. */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/sysfs.h>
#include <trace/hooks/vmscan.h>

enum extend_reclaim_type {
	EXTEND_RECLAIM_TYPE_BASE = 0,
	EXTEND_RECLAIM_TYPE_FILE = 0,
	EXTEND_RECLAIM_TYPE_ANON = 1,
	EXTEND_RECLAIM_TYPE_ALL = 2,
};

static struct kobject *kobj;
static enum extend_reclaim_type reclaim_type;
static int extend_reclaim_debug;

static char reclaim_type_str[3][5] = { "file", "anon", "all" };

enum scan_balance {
	SCAN_EQUAL,
	SCAN_FRACT,
	SCAN_ANON,
	SCAN_FILE,
};

static void extend_reclaim_tune_scan_type_handler(void *unused,
						  enum scan_balance *scan_type)
{
	if (READ_ONCE(reclaim_type) == 1 &&
	    !strcmp(current->comm, "idle_page_size"))
		*scan_type = 2; /* SCAN_ANON is 2 in mm/vmscan.c. */
}

static ssize_t reclaim_show(struct kobject *kobj, struct kobj_attribute *attr,
			    char *buf)
{
	memcpy(buf, __func__, 13);
	return 12;
}
static ssize_t reclaim_store(struct kobject *kobj, struct kobj_attribute *attr,
			     const char *buf, size_t count)
{
	unsigned long nr_to_reclaim = 0, nr;
	unsigned long nr_reclaimed;

	if (sscanf(buf, "%d %ld", &reclaim_type, &nr_to_reclaim) != 2) {
		pr_err("extend_reclaim: invalid input: %s", buf);
		return -EINVAL;
	}
	nr = nr_to_reclaim;
	if (reclaim_type >= ARRAY_SIZE(reclaim_type_str)) {
		pr_err("extend_reclaim: invalid input: reclaim type = %s, nr_to_reclaim = %ld",
		       reclaim_type_str[reclaim_type], nr);
		return -EINVAL;
	}
	nr_reclaimed = try_to_free_mem_cgroup_pages(NULL, nr, GFP_KERNEL,
			(reclaim_type == EXTEND_RECLAIM_TYPE_ANON ||
			 reclaim_type == EXTEND_RECLAIM_TYPE_ALL) ?
				MEMCG_RECLAIM_MAY_SWAP : 0, NULL);
	if (extend_reclaim_debug)
		pr_info("extend_reclaim: current->comm = %s, reclaim type = %s, nr_to_reclaim = %ld, nr_reclaimed = %ld",
			current->comm, reclaim_type_str[reclaim_type],
			nr_to_reclaim, nr_reclaimed);
	return count;
}

static ssize_t debug_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	return sprintf(buf, "%d", READ_ONCE(extend_reclaim_debug));
}

static ssize_t debug_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t count)
{
	int parsed;

	parsed = sscanf(buf, "%d", &extend_reclaim_debug);
	if (parsed != 1) {
		pr_err("extend_reclaim: invalid input: %s", buf);
		return -EINVAL;
	}
	return count;
}

static struct kobj_attribute reclaim_attr = __ATTR(reclaim, 0644,
						   reclaim_show, reclaim_store);
static struct kobj_attribute debug_attr = __ATTR(debug, 0644,
						 debug_show, debug_store);

static struct attribute *extend_reclaim_attrs[] = {
	&reclaim_attr.attr,
	&debug_attr.attr,
	NULL,
};

static const struct attribute_group extend_reclaim_attr_group = {
	.attrs = extend_reclaim_attrs,
};

static int __init extend_reclaim_init(void)
{
	int ret;

	kobj = kobject_create_and_add("extend_reclaim", kernel_kobj);
	if (!kobj) {
		ret = -ENOMEM;
		goto fail;
	}

	ret = sysfs_create_group(kobj, &extend_reclaim_attr_group);
	if (ret) {
		kobject_put(kobj);
		goto fail;
	}
	register_trace_android_vh_tune_scan_type(
		extend_reclaim_tune_scan_type_handler, NULL);
	pr_info("extend_reclaim: module init!");
	return 0;

fail:
	pr_err("extend_reclaim module init failed error = %d!", ret);
	return ret;
}

static void __exit extend_reclaim_exit(void)
{
	if (kobj)
		kobject_put(kobj);
	unregister_trace_android_vh_tune_scan_type(
		extend_reclaim_tune_scan_type_handler, NULL);
	pr_info("extend_reclaim: module exit!");
}

module_init(extend_reclaim_init);
module_exit(extend_reclaim_exit);
MODULE_DESCRIPTION("global page reclaim interface");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("chenzhiwei<chenzhiwei@xiaomi.com>");
