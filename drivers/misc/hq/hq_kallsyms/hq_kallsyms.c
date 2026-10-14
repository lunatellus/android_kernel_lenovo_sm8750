// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Huaqin common kallsyms helper.
 *
 * kallsyms_lookup_name() is not exported to modules on GKI, so the address is
 * recovered with a kprobe on the symbol name and the probe is removed again
 * right away.  The resolved address is then wrapped so that out-of-tree modules
 * can look up kernel symbols without a kallsyms hack of their own.
 *
 * The "factory build" flag is derived from the DT /chosen/bootargs property.
 */
#include <linux/init.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/string.h>

/* Resolved address of the kernel's kallsyms_lookup_name(). */
static unsigned long (*g_kallsyms_lookup_name)(const char *name);

/*
 * Set when the DT /chosen/bootargs contains "factory_build=1".
 * Read by other out-of-tree modules (e.g. haptic.ko), so it must stay a
 * non-static symbol of exactly one byte.
 */
bool is_factory_build;
EXPORT_SYMBOL_GPL(is_factory_build);

static void check_factory_build(void)
{
	const struct device_node *node;
	const char *bootargs;

	is_factory_build = false;

	/*
	 * The GKI flavour of of_find_node_opts_by_path() takes the path as
	 * its first argument plus an optional "const char **" options out
	 * parameter.
	 */
	node = of_find_node_opts_by_path("/chosen", NULL);
	if (node) {
		bootargs = of_get_property(node, "bootargs", NULL);
		if (bootargs && strstr(bootargs, "factory_build=1"))
			is_factory_build = true;
	}

	pr_info("%s: kernel factory_build = %d\n", __func__, is_factory_build);
}

/**
 * hq_kallsyms_lookup_name() - look up a kernel symbol
 * @name: NUL terminated symbol name
 *
 * Return: address of @name, or 0 when it cannot be resolved.
 *
 * The stock export is declared `unsigned long __nocfi ...`: genksyms hashes
 * the attribute, so this exact spelling is what yields the stock symbol CRC
 * 0xc3371615 that the importers (haptic, smsc95xx, register_dumper) expect.
 */
unsigned long __nocfi hq_kallsyms_lookup_name(const char *name)
{
	pr_info("%s: %s\n", __func__, name);
	pr_info("%s: by g_kallsyms_lookup_name!\n", __func__);

	if (g_kallsyms_lookup_name)
		return g_kallsyms_lookup_name(name);

	pr_err("%s: g_kallsyms_lookup_name not init\n", __func__);
	return 0;
}
EXPORT_SYMBOL_GPL(hq_kallsyms_lookup_name);

static int __init hq_kallsyms_init(void)
{
	struct kprobe kp;
	int ret;

	pr_info("%s Entry\n", __func__);

	check_factory_build();

	memset(&kp, 0, sizeof(kp));
	kp.symbol_name = "kallsyms_lookup_name";

	ret = register_kprobe(&kp);
	if (ret < 0) {
		pr_err("register kallsyms_lookup_name kporbe failed!, ret = %d\n",
		       ret);
		pr_err("%s: hq_get_kallsyms failed!\n", __func__);
		return 0;
	}

	g_kallsyms_lookup_name = (unsigned long (*)(const char *))kp.addr;
	if (!g_kallsyms_lookup_name)
		pr_err("g_kallsyms_lookup_name is null\n");

	unregister_kprobe(&kp);
	return 0;
}

static void __exit hq_kallsyms_exit(void)
{
	pr_info("%s Entry\n", __func__);
}

module_init(hq_kallsyms_init);
module_exit(hq_kallsyms_exit);

MODULE_AUTHOR("hq bsp system team");
MODULE_DESCRIPTION("hq common kernel kallsyms driver");
MODULE_LICENSE("GPL");
