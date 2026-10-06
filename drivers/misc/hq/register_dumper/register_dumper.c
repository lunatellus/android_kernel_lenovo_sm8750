// SPDX-License-Identifier: GPL-2.0
/* Lenovo/Huaqin register dumper reconstructed from stock ZUXOS 1.5.10.259. */
#include <linux/init.h>
#include <linux/cache.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kprobes.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/regmap.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#define DEVICE_NAME_LEN 0x30
#define REGISTER_LINE_LEN 0x0c

struct regmap_dumper {
	struct list_head list;
	struct regmap *regmap;
	struct device *dev;
	char name[DEVICE_NAME_LEN];
	unsigned int index;
};

extern void *hq_kallsyms_lookup_name(const char *name);

static int regmap_register_hook_pre_handler(struct kprobe *probe,
					    struct pt_regs *regs);
static struct kprobe regmap_register_hook = {
	.symbol_name = "devm_regmap_qti_debugfs_register",
	.pre_handler = regmap_register_hook_pre_handler,
};
static struct proc_dir_entry *proc_dumper __read_mostly;
static LIST_HEAD(dumper_list);
static unsigned int count;
static char cur_dev[DEVICE_NAME_LEN];
static int (*regmap_get_max_register_l)(struct regmap *);
static int (*regmap_get_val_bytes_l)(struct regmap *);

static int regmap_register_hook_pre_handler(struct kprobe *probe,
					    struct pt_regs *regs)
{
	struct {
		struct device *dev;
		struct regmap *map;
	} *args = (void *)regs;
	struct device *dev = args->dev;
	struct regmap *map = args->map;
	struct regmap_dumper *dumper;

	if (!map)
		pr_err("%s: %s, regmap is null for %s\n", "register_dumper",
		       "regmap_register_hook_pre_handler",
		       dev->init_name ? dev->init_name : dev->kobj.name);

	list_for_each_entry(dumper, &dumper_list, list) {
		if (dumper->regmap == map) {
			pr_err("%s: %s, dumper %d already registered for regmap\n",
			       "register_dumper", "regmap_register_hook_pre_handler",
			       dumper->index);
			return 0;
		}
	}

	dumper = kzalloc(sizeof(*dumper), GFP_ATOMIC);
	if (!dumper)
		return 0;

	dumper->regmap = map;
	dumper->dev = dev;
	snprintf(dumper->name, sizeof(dumper->name), "%s",
		 dev->init_name ? dev->init_name : dev->kobj.name);
	dumper->index = count++;
	list_add(&dumper->list, &dumper_list);
	pr_info("%s: %s, dev name = %s, index = %d\n", "register_dumper",
		"regmap_register_hook_pre_handler",
		dev->init_name ? dev->init_name : dev->kobj.name,
		dumper->index);
	return 0;
}

static int device_list_show(struct seq_file *seq, void *unused)
{
	struct regmap_dumper *dumper;

	list_for_each_entry(dumper, &dumper_list, list)
		seq_printf(seq, "%s\n", dumper->name);
	return 0;
}

static int device_list_open(struct inode *inode, struct file *file)
{
	return single_open(file, device_list_show, NULL);
}

static ssize_t device_list_write(struct file *file, const char __user *buffer,
				 size_t count_in, loff_t *ppos)
{
	char *name = cur_dev;
	size_t len;
	size_t i;

	memset(name, 0, sizeof(cur_dev));
	len = count_in > DEVICE_NAME_LEN ?
		DEVICE_NAME_LEN - 1 : count_in;
	if (copy_from_user(name, buffer, len)) {
		pr_err("%s: write failed!\n", "register_dumper");
		return -EFAULT;
	}
	for (i = 0; i < DEVICE_NAME_LEN - 1; i++) {
		if (name[i] == '\n')
			name[i] = '\0';
	}
	pr_info("%s: %s write register name is %s\n", "register_dumper",
		"device_list_write", name);
	return len;

}

static const struct proc_ops device_list_ops = {
	.proc_open = device_list_open,
	.proc_read = seq_read,
	.proc_write = device_list_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static int help_info_show(struct seq_file *seq, void *unused)
{
	seq_printf(seq, "cat /proc/register_dumper/device_list to get regmap list.\n");
	seq_printf(seq, "echo xxxx > /proc/register_dumper/device_list\n");
	seq_printf(seq, "\t\t(xxx is device name, such as 0-07...)\n");
	seq_printf(seq, "cat /proc/register_dumper/registers\n");
	return 0;
}

static int help_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, help_info_show, NULL);
}

static const struct proc_ops help_info_ops = {
	.proc_open = help_info_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static void *dump_registers_seq_start(struct seq_file *seq, loff_t *pos)
{
	pr_info("%s: %s pos = %lld\n", "register_dumper",
		"dump_registers_seq_start", *pos);
	return seq_list_start(&dumper_list, *pos);
}

static void dump_registers_seq_stop(struct seq_file *seq, void *v)
{
	pr_info("%s: %s\n", "register_dumper", "dump_registers_seq_stop");
}

static void *dump_registers_seq_next(struct seq_file *seq, void *v,
				     loff_t *pos)
{
	pr_info("%s: %s pos = %lld\n", "register_dumper",
		"dump_registers_seq_next", *pos);
	return seq_list_next(v, &dumper_list, pos);
}

static int dump_registers_seq_show(struct seq_file *seq, void *v)
{
	struct regmap_dumper *dumper = list_entry(v, struct regmap_dumper, list);
	struct regmap *map;
	size_t cur_len;
	int max_register;
	int addr_width, value_width, reg, value, ret;
	char line[REGISTER_LINE_LEN] = { 0 };
	int prefix;
	char *dst;

	cur_len = strlen(cur_dev);
	pr_info("%s: %s cur_dev:%s dumper_name:%s size:%lu\n", "register_dumper",
		"dump_registers_seq_show", cur_dev, dumper->name, cur_len);
	cur_len = strlen(cur_dev);
	if (strncmp(cur_dev, dumper->name, cur_len)) {
		pr_err("%s: skip not matched map: %s\n", "register_dumper",
		       dumper->name);
		return 0;
	}

	map = dumper->regmap;
	pr_info("%s: matched map: %s, rg is %s!\n", "register_dumper",
		dumper->name, map ? "not null" : "null");
	if (!map) {
		pr_err("%s: skip not matched map: %s\n", "register_dumper",
		       dumper->name);
		return 0;
	}
	pr_info("%s: %s +++\n", "register_dumper", "dump_registers_seq_show");

	max_register = 0;
	if (!IS_ERR_OR_NULL(regmap_get_max_register_l))
		max_register = regmap_get_max_register_l(map);
	addr_width = snprintf(NULL, 0, "%x", max_register);
	value_width = 0;
	if (!IS_ERR_OR_NULL(regmap_get_val_bytes_l))
		value_width = regmap_get_val_bytes_l(map) * 2;
	for (reg = 0; reg <= max_register; reg++) {
		prefix = addr_width + 2;
		memset(line, 0, sizeof(line));
		snprintf(line, sizeof(line), "%.*x: ", addr_width, reg);
		ret = regmap_read(map, reg, &value);
		dst = line + prefix;
		if (!ret)
			scnprintf(dst, sizeof(line), "%.*x", value_width, value);
		else
			memset(dst, 'X', value_width);
		line[(size_t)prefix + value_width] = '\n';
		seq_printf(seq, "%s", line);
	}
	pr_info("%s: %s ---\n", "register_dumper", "dump_registers_seq_show");
	return 0;
}

static const struct seq_operations dump_registers_ops = {
	.start = dump_registers_seq_start,
	.stop = dump_registers_seq_stop,
	.next = dump_registers_seq_next,
	.show = dump_registers_seq_show,
};

static int __init register_dumper_init(void)
{
	void *sym;
	int ret;

	pr_info("%s Entry\n", __func__);
	sym = hq_kallsyms_lookup_name("regmap_get_max_register");
	if (unlikely(!sym))
		goto get_max_failed;
	regmap_get_max_register_l = sym;
get_val_bytes:
	sym = hq_kallsyms_lookup_name("regmap_get_val_bytes");
	if (unlikely(!sym))
		goto get_val_bytes_failed;
	regmap_get_val_bytes_l = sym;
register_hook:
	ret = register_kprobe(&regmap_register_hook);
	if (ret < 0)
		goto register_hook_failed;
	proc_dumper = proc_mkdir("register_dumper", NULL);
	if (!proc_dumper)
		goto proc_mkdir_failed;
	if (proc_create("device_list", 0666, proc_dumper, &device_list_ops) &&
	    proc_create("help_info", 0444, proc_dumper, &help_info_ops) &&
	    proc_create_seq_private("registers", 0664, proc_dumper,
				    &dump_registers_ops, 0, NULL))
		goto exit;
remove_tree:
	remove_proc_subtree("register_dumper", NULL);
	return 0;

register_hook_failed:
	pr_err("%s: register_regmap_register_hook failed!\n",
	       __func__);
	return 0;
proc_mkdir_failed:
	pr_err("%s: proc_mkdir register_dumper failed!\n",
	       __func__);
	goto remove_tree;
exit:
	pr_info("%s Exit!\n", __func__);
	return 0;
get_max_failed:
	pr_err("%s: %s find regmap_get_max_register failed!\n",
	       "register_dumper", __func__);
	goto get_val_bytes;
get_val_bytes_failed:
	pr_err("%s: %s find regmap_get_val_bytes failed!\n",
	       "register_dumper", __func__);
	goto register_hook;
}

static void __exit register_dumper_exit(void)
{
	pr_info("%s Entry\n", "register_dumper_exit");
	remove_proc_subtree("register_dumper", NULL);
}

module_init(register_dumper_init);
module_exit(register_dumper_exit);
MODULE_AUTHOR("hq bsp system team");
MODULE_DESCRIPTION("hq common kernel register dumper");
MODULE_LICENSE("GPL");
