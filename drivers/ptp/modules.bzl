def register_modules(registry):
    registry.register(
        name = "drivers/ptp/ptp_qcom_tsc_vm",
        out = "ptp_qcom_tsc_vm.ko",
        config = "CONFIG_PTP_QCOM_CLOCK_TSC_VM",
        srcs = [
            # do not sort
            "drivers/ptp/ptp_qcom_tsc_vm.c",
        ],
        deps = [
            # do not sort
        ],
    )
