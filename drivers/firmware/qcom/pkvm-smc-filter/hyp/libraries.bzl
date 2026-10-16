def register_libraries(registry):
    """Register pkvm-smc-filter EL2 hypervisor libraries."""
    registry.register(
        name = "pkvm-smc-filter-hyp-lib",
        srcs = [
            "drivers/firmware/qcom/pkvm-smc-filter/hyp/smc-filter-hyp.c",
            "drivers/firmware/qcom/pkvm-smc-filter/hyp/smc-filter-hyp.h",
            "drivers/firmware/qcom/pkvm-smc-filter/hyp/smc-forward.S",
        ],
        config = "CONFIG_PKVM_SMC_FILTER",
        pkvm_el2 = True,
    )
