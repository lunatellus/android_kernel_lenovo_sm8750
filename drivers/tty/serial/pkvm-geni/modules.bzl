def register_modules(registry):
    """Register pkvm-geni EL1 host module that depends on EL2 hyp library."""
    registry.register(
        name = "drivers/tty/serial/pkvm-geni",
        out = "pkvm-geni.ko",
        config = "CONFIG_SERIAL_PKVM_GENI",
        srcs = [
            # do not sort
            "drivers/tty/serial/pkvm-geni/geni-pkvm.h",
            "drivers/tty/serial/pkvm-geni/geni-host.c",
        ],
        lib_deps = [
            "pkvm-geni-hyp-lib",
        ],
    )
