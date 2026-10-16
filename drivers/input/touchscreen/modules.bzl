def register_modules(registry):
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
