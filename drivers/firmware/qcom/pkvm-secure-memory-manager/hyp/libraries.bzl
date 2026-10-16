def register_libraries(registry):
    """Register pkvm-secure-memory-manager EL2 hyp libraries."""
    registry.register(
        name = "pkvm-secure-memory-manager-hyp-lib",
        srcs = [
            "drivers/firmware/qcom/pkvm-secure-memory-manager/hyp/secure-memory-manager-hyp.c",
            "drivers/firmware/qcom/pkvm-secure-memory-manager/hyp/smm-pkvm.h",
        ],
        config = "CONFIG_PKVM_SECURE_MEMORY_MANAGER",
        pkvm_el2 = True,
    )
