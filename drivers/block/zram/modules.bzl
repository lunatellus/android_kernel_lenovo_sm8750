def register_modules(registry):
    registry.register(
        name = "drivers/block/zram/zram",
        out = "zram.ko",
        config = "CONFIG_ZRAM",
        srcs = [
            # do not sort
            "drivers/block/zram/zcomp.c",
            "drivers/block/zram/zram_drv.c",
            "drivers/block/zram/zcomp.h",
            "drivers/block/zram/zram_drv.h",
            "drivers/soc/qcom/qpace/qpace.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qpace/qpace_drv",
            "mm/zsmalloc",
        ],
    )
