def register_libraries(registry):
    """Register pkvm-geni EL2 hypervisor libraries."""
    registry.register(
        name = "pkvm-geni-hyp-lib",
        srcs = [
            "drivers/tty/serial/pkvm-geni/hyp/geni-hyp.c",
        ],
        config = "CONFIG_SERIAL_PKVM_GENI",
        pkvm_el2 = True,
    )
