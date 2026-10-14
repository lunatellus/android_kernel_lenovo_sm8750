load(":drivers/input/touchscreen/nvt_touch_spi/modules.bzl", register_nvt_touch_spi = "register_modules")

def register_modules(registry):
    register_nvt_touch_spi(registry)
    registry.register(
        name = "drivers/input/touchscreen/cap1296",
        out = "cap1296.ko",
        config = "CONFIG_TOUCHSCREEN_CAP1296",
        srcs = [
            # do not sort
            "drivers/input/touchscreen/cap1296.c",
            "drivers/input/touchscreen/cap1296.h",
        ],
    )
