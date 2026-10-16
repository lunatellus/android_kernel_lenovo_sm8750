def register_modules(registry):
    """Register pkvm-smc-filter EL1 host module that depends on EL2 hyp library."""
    registry.register(
        name = "drivers/firmware/qcom/pkvm-smc-filter",
        out = "pkvm-smc-filter.ko",
        config = "CONFIG_PKVM_SMC_FILTER",
        srcs = [
            # do not sort
            "drivers/firmware/qcom/pkvm-smc-filter/smc-filter-host.c",
        ],
        lib_deps = [
            "pkvm-smc-filter-hyp-lib",
        ],
    )
