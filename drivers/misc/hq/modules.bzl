def register_modules(registry):
    registry.register(
        name = "drivers/misc/hq/hqsysfs/hqsysfs",
        out = "hqsysfs.ko",
        config = "CONFIG_HQ_SYSFS",
        srcs = ["drivers/misc/hq/hqsysfs/hqsysfs.c"],
    )
    registry.register(
        name = "drivers/misc/hq/hq_kallsyms/hq_kallsyms",
        out = "hq_kallsyms.ko",
        config = "CONFIG_HQ_KALLSYMS",
        srcs = ["drivers/misc/hq/hq_kallsyms/hq_kallsyms.c"],
    )
    registry.register(
        name = "drivers/misc/hq/memory/ffu/ufsffu",
        out = "ufsffu.ko",
        config = "CONFIG_HQ_UFSFFU",
        srcs = ["drivers/misc/hq/memory/ffu/ufsffu.c"],
    )
    registry.register(
        name = "drivers/misc/hq/memory/extend_reclaim/extend_reclaim",
        out = "extend_reclaim.ko",
        config = "CONFIG_HQ_EXTEND_RECLAIM",
        srcs = ["drivers/misc/hq/memory/extend_reclaim/extend_reclaim.c"],
    )
    registry.register(
        name = "drivers/misc/hq/magic_key/magic_key",
        out = "magic_key.ko",
        config = "CONFIG_HQ_MAGIC_KEY",
        srcs = ["drivers/misc/hq/magic_key/magic_key.c"],
    )
    registry.register(
        name = "drivers/misc/hq/register_dumper/register_dumper",
        out = "register_dumper.ko",
        config = "CONFIG_HQ_REGISTER_DUMPER",
        srcs = ["drivers/misc/hq/register_dumper/register_dumper.c"],
        deps = ["drivers/misc/hq/hq_kallsyms/hq_kallsyms"],
    )
