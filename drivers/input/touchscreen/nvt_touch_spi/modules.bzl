def register_modules(registry):
    registry.register(
        name = "drivers/input/touchscreen/nvt_touch_spi/nvt_36xxx",
        out = "nvt_36xxx.ko",
        config = "CONFIG_TOUCHSCREEN_NVT_SPI",
        srcs = [
            # do not sort
            "drivers/input/touchscreen/nvt_touch_spi/nt36xxx.c",
            "drivers/input/touchscreen/nvt_touch_spi/nt36xxx_fw_update.c",
            "drivers/input/touchscreen/nvt_touch_spi/nt36xxx_ext_proc.c",
            "drivers/input/touchscreen/nvt_touch_spi/nt36xxx_mp_ctrlram.c",
            "drivers/input/touchscreen/nvt_touch_spi/nt36xxx_platform.c",
            "drivers/input/touchscreen/nvt_touch_spi/nt36xxx.h",
            "drivers/input/touchscreen/nvt_touch_spi/nt36xxx_mem_map.h",
            "drivers/input/touchscreen/nvt_touch_spi/nt36xxx_mp_ctrlram.h",
        ],
        deps = [
            "drivers/misc/hq/hqsysfs/hqsysfs",
            "drivers/soc/qcom/panel_event_notifier",
        ],
    )
