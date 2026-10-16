load(":configs/bengal_le.bzl", "bengal_le_config")
load(":configs/bengal_le_debug.bzl", "bengal_le_debug_config")
load(":kleaf-scripts/image_opts.bzl", "boot_image_opts")
load(":kleaf-scripts/le_build.bzl", "define_typical_le_build")
load(":target_variants.bzl", "le_variants")

target_name = "bengal-le"

def define_bengal_le():
    for variant in le_variants:
        board_kernel_cmdline_extras = []

        if variant == "debug-defconfig":
            board_kernel_cmdline_extras += [
                # do not sort
                "console=ttyMSM0,115200n8",
                "qcom_geni_serial.con_enabled=1",
                "earlycon=qcom_geni,0x4a90000",
                "androidboot.serialconsole=1",
            ]

            debug_build_img_opts = boot_image_opts(
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
            )

        else:
            board_kernel_cmdline_extras += [
                "nosoftlockup",
                "console=ttynull",
                "qcom_geni_serial.con_enabled=0",
                "androidboot.serialconsole=0",
            ]

            build_img_opts = boot_image_opts(
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
            )

    define_typical_le_build(
        name = target_name,
        build_config = "build.config.msm.bengal.le",
        debug_config = bengal_le_debug_config,
        config = bengal_le_config,
        debug_build_img_opts = debug_build_img_opts,
        build_img_opts = build_img_opts,
        debug_kwargs = {
            "config_path": "configs/bengal_le_debug.bzl",
        },
        perf_kwargs = {
            "config_path": "configs/bengal_le.bzl",
        },
    )
