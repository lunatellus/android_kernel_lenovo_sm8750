def register_modules(registry):
    registry.register(
        name = "drivers/leds/flash/leds-qcom-flash",
        out = "leds-qcom-flash.ko",
        config = "CONFIG_LEDS_QCOM_FLASH",
        srcs = [
            # do not sort
            "drivers/leds/flash/leds-qcom-flash.c",
        ],
        deps = [
            "drivers/power/supply/qti_battery_charger",
        ],
    )
