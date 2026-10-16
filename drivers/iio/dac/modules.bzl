def register_modules(registry):
    registry.register(
        name = "drivers/iio/dac/qcom-spmi-vdac",
        out = "qcom-spmi-vdac.ko",
        config = "CONFIG_QCOM_SPMI_VDAC",
        srcs = [
            # do not sort
            "drivers/iio/dac/qcom-spmi-vdac.c",
        ],
    )
