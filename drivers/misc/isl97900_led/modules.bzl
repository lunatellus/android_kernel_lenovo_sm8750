def register_modules(registry):
    registry.register(
        name = "drivers/misc/isl97900_led/isl97900_led",
        out = "isl97900_led.ko",
        config = "CONFIG_ISL97900_LED",
        srcs = [
            # do not sort
            "drivers/misc/isl97900_led/isl97900_led.c",
        ],
        deps = [
            # do not sort
            "drivers/base/regmap/qti-regmap-debugfs",
        ],
    )
