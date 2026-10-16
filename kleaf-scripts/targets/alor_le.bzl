load(":configs/alor_le.bzl", "alor_le_config")
load(":configs/alor_le_debug.bzl", "alor_le_debug_config")
load(":kleaf-scripts/image_opts.bzl", "boot_image_opts")
load(":kleaf-scripts/le_build.bzl", "define_typical_le_build")
load(":target_variants.bzl", "le_variants")

target_name = "alor-le"

def define_alor_le():
    for variant in le_variants:
        board_kernel_cmdline_extras = []
        board_bootconfig_extras = []

        if variant == "debug-defconfig":
            board_kernel_cmdline_extras += [
                # do not sort
                "console=ttyMSM0,115200n8",
                "qcom_geni_serial.con_enabled=1",
                "earlycon",
                "ufshcd_core.uic_cmd_timeout=2000",
            ]

            debug_build_img_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x00a9c000",
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
                board_bootconfig_extras = board_bootconfig_extras,
            )

        else:
            board_kernel_cmdline_extras += ["nosoftlockup console=ttynull qcom_geni_serial.con_enabled=0"]

            build_img_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x00a9c000",
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
                board_bootconfig_extras = board_bootconfig_extras,
            )

    define_typical_le_build(
        name = target_name,
        build_config = "build.config.msm.alor.le",
        debug_config = alor_le_debug_config,
        config = alor_le_config,
        debug_build_img_opts = debug_build_img_opts,
        build_img_opts = build_img_opts,
        debug_kwargs = {
            "config_path": "configs/alor_le_debug.bzl",
        },
        perf_kwargs = {
            "config_path": "configs/alor_le.bzl",
        },
    )
