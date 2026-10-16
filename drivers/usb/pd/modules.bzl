def register_modules(registry):
    registry.register(
        name = "drivers/usb/pd/usbpd",
        out = "usbpd.ko",
        config = "CONFIG_USB_PD_POLICY",
        srcs = [
            # do not sort
            "drivers/usb/pd/policy_engine.c",
        ],
        hdrs = [
            "drivers/usb/pd/usbpd.h",
        ],
        deps = [
            # do not sort
            "kernel/trace/qcom_ipc_logging",
        ],
    )
    registry.register(
        name = "drivers/usb/pd/qpnp-pdphy",
        out = "qpnp-pdphy.ko",
        config = "CONFIG_QPNP_USB_PDPHY",
        srcs = [
            # do not sort
            "drivers/usb/pd/qpnp-pdphy.c",
        ],
        hdrs = [
            "drivers/usb/pd/usbpd.h",
        ],
        deps = [
            # do not sort
            "drivers/usb/pd/usbpd",
        ],
    )
