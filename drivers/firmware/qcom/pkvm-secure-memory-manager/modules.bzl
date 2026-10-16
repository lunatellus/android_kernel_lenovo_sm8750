def register_modules(registry):
    """Register pkvm-secure-memory-manager EL1 host module that depends on EL2 hyp library."""
    registry.register(
        name = "drivers/firmware/qcom/pkvm-secure-memory-manager",
        out = "pkvm-secure-memory-manager.ko",
        config = "CONFIG_PKVM_SECURE_MEMORY_MANAGER",
        srcs = [
            # do not sort
            "drivers/firmware/qcom/pkvm-secure-memory-manager/secure-memory-manager-host.c",
            "drivers/firmware/qcom/pkvm-secure-memory-manager/hyp/smm-pkvm.h",
        ],
        deps = [
            "drivers/firmware/qcom/si_core/si_core_module",
        ],
        lib_deps = [
            "pkvm-secure-memory-manager-hyp-lib",
        ],
    )
