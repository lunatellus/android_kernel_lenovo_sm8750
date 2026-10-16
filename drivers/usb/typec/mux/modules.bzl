def register_modules(registry):
    registry.register(
        name = "drivers/usb/typec/mux/ps883x.c",
        out = "ps883x.ko",
        config = "CONFIG_TYPEC_MUX_PS883X",
        srcs = [
            # do not sort
            "drivers/usb/typec/mux/ps883x.c",
        ],
    )

    registry.register(
        name = "drivers/usb/typec/mux/nb7vpq904m",
        out = "nb7vpq904m.ko",
        config = "CONFIG_TYPEC_MUX_NB7VPQ904M",
        srcs = [
            # do not sort
            "drivers/usb/typec/mux/nb7vpq904m.c",
        ],
        deps = [
            # do not sort
            "drivers/gpu/drm/bridge/aux-bridge",
        ],
    )
