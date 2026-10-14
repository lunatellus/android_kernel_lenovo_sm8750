def register_modules(registry):
    registry.register(
        name = "drivers/usb/repeater/repeater",
        out = "repeater.ko",
        config = "CONFIG_USB_REPEATER",
        srcs = [
            # do not sort
            "drivers/usb/repeater/repeater.c",
        ],
    )

    registry.register(
        name = "drivers/usb/repeater/repeater-qti-pmic-eusb2",
        out = "repeater-qti-pmic-eusb2.ko",
        config = "CONFIG_QTI_PMIC_EUSB2_REPEATER",
        srcs = [
            # do not sort
            "drivers/usb/repeater/repeater-qti-pmic-eusb2.c",
        ],
        deps = [
            # do not sort
            "drivers/usb/repeater/repeater",
        ],
    )

    registry.register(
        name = "drivers/usb/repeater/repeater-i2c-eusb2",
        out = "repeater-i2c-eusb2.ko",
        config = "CONFIG_I2C_EUSB2_REPEATER",
        srcs = [
            # do not sort
            "drivers/usb/repeater/repeater-i2c-eusb2.c",
        ],
        deps = [
            # do not sort
            "drivers/base/regmap/qti-regmap-debugfs",
            "drivers/usb/repeater/repeater",
        ],
    )

    registry.register(
        name = "drivers/usb/repeater/hq_ptn3222_repeater",
        out = "hq_ptn3222_repeater.ko",
        config = "CONFIG_HQ_PTN3222_REPEATER",
        srcs = [
            # do not sort
            "drivers/usb/repeater/hq_ptn3222_repeater.c",
        ],
        deps = [
            # do not sort
            "drivers/usb/repeater/repeater",
            "drivers/usb/typec/ucsi/ucsi_qti_glink",
            "drivers/base/regmap/qti-regmap-debugfs",
        ],
    )
