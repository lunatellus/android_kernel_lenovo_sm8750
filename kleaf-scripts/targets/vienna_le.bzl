load(":configs/vienna_le.bzl", "vienna_le_config")
load(":configs/vienna_le_debug.bzl", "vienna_le_debug_config")
load(":kleaf-scripts/generic_le.bzl", "define_qcom_le_setup")
load(":kleaf-scripts/image_opts.bzl", "boot_image_opts")
load(":kleaf-scripts/le_build.bzl", "define_typical_le_build")
load(":target_variants.bzl", "le_variants")

target_name = "vienna-le"

def define_vienna_le():
    for variant in le_variants:
        board_kernel_cmdline_extras = []
        board_bootconfig_extras = []

        if variant == "debug-defconfig":
            board_bootconfig_extras += []
            board_kernel_cmdline_extras += []

            debug_build_img_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x00a90000",
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
                board_bootconfig_extras = board_bootconfig_extras,
            )

        else:
            board_kernel_cmdline_extras += []
            board_bootconfig_extras += []

            build_img_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x00a90000",
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
                board_bootconfig_extras = board_bootconfig_extras,
            )

    define_typical_le_build(
        name = target_name,
        build_config = "build.config.msm.vienna.le",
        debug_config = vienna_le_debug_config,
        config = vienna_le_config,
        debug_build_img_opts = debug_build_img_opts,
        build_img_opts = build_img_opts,
        debug_kwargs = {
            "config_path": "configs/vienna_le_debug.bzl",
        },
        perf_kwargs = {
            "config_path": "configs/vienna_le.bzl",
        },
    )
