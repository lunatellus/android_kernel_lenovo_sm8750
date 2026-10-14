def register_modules(registry):
    registry.register(
        name = "drivers/misc/awinic/haptic_hv/haptic",
        out = "haptic.ko",
        config = "CONFIG_AWINIC_HAPTIC_HV",
        srcs = [
            # do not sort
            "drivers/misc/awinic/haptic_hv/aw869x.c",
            "drivers/misc/awinic/haptic_hv/aw869xx.c",
            "drivers/misc/awinic/haptic_hv/aw8671x.c",
            "drivers/misc/awinic/haptic_hv/aw8672x.c",
            "drivers/misc/awinic/haptic_hv/aw8692x.c",
            "drivers/misc/awinic/haptic_hv/aw8693x.c",
            "drivers/misc/awinic/haptic_hv/haptic_hv.c",
            "drivers/misc/awinic/haptic_hv/haptic_hv.h",
            "drivers/misc/awinic/haptic_hv/haptic_hv_reg.h",
            "drivers/misc/awinic/haptic_hv/haptic_rtp_names.inc",
        ],
        copts = ["-DAW_DOUBLE", "-Wno-error=unused-result"],
        deps = ["drivers/misc/hq/hq_kallsyms/hq_kallsyms"],
    )
