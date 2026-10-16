def register_modules(registry):
    registry.register(
        name = "drivers/firmware/arm_ffa_transport",
        out = "arm_ffa_transport.ko",
        config = "CONFIG_ARM_FFA_TRANSPORT",
        srcs = [
            # do not sort
            "drivers/firmware/arm_ffa/bus.c",
            "drivers/firmware/arm_ffa/common.h",
        ],
    )
    registry.register(
        name = "drivers/firmware/arm_ffa",
        out = "arm_ffa.ko",
        config = "CONFIG_ARM_FFA_TRANSPORT",
        srcs = [
            # do not sort
            "drivers/firmware/arm_ffa/smccc.c",
            "drivers/firmware/arm_ffa/driver.c",
            "drivers/firmware/arm_ffa/common.h",
        ],
        deps = [
            "drivers/firmware/arm_ffa_transport",
        ],
    )
