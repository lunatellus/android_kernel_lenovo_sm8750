def register_modules(registry):
    registry.register(
        name = "drivers/extcon/extcon-qti-spmi-misc",
        out = "extcon-qti-spmi-misc.ko",
        config = "CONFIG_EXTCON_QTI_SPMI_MISC",
        srcs = [
            # do not sort
            "drivers/extcon/extcon-qti-spmi-misc.c",
        ],
    )

    registry.register(
        name = "drivers/extcon/extcon-usb-gpio",
        out = "extcon-usb-gpio.ko",
        config = "CONFIG_EXTCON_USB_GPIO",
        srcs = [
            # do not sort
            "drivers/extcon/extcon-usb-gpio.c",
        ],
    )
