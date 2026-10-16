load(":configs/shikra_consolidate.bzl", "shikra_consolidate_config")
load(":configs/shikra_perf.bzl", "shikra_perf_config")
load(":configs/shikra_tuivm.bzl", "shikra_tuivm_config")
load(":configs/shikra_tuivm_debug.bzl", "shikra_tuivm_debug_config")
load(":kleaf-scripts/android_build.bzl", "define_typical_android_build")
load(":kleaf-scripts/image_opts.bzl", "boot_image_opts")
load(":kleaf-scripts/vm_build.bzl", "define_typical_vm_build")
load(":target_variants.bzl", "la_variants")

target_name = "shikra"

def define_shikra():
    for variant in la_variants:
        board_kernel_cmdline_extras = []
        board_bootconfig_extras = []
        kernel_vendor_cmdline_extras = ["bootconfig"]

        if variant == "consolidate":
            board_bootconfig_extras += ["androidboot.serialconsole=1"]
            board_kernel_cmdline_extras += [
                # do not sort
                "console=ttyMSM0,115200n8",
                "qcom_geni_serial.con_enabled=1",
                "earlycon",
            ]
            kernel_vendor_cmdline_extras += [
                # do not sort
                "console=ttyMSM0,115200n8",
                "qcom_geni_serial.con_enabled=1",
                "earlycon",
            ]

            consolidate_build_img_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x04a90000",
                kernel_vendor_cmdline_extras = kernel_vendor_cmdline_extras,
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
                board_bootconfig_extras = board_bootconfig_extras,
            )

        else:
            board_kernel_cmdline_extras += ["nosoftlockup console=ttynull qcom_geni_serial.con_enabled=0"]
            kernel_vendor_cmdline_extras += ["nosoftlockup console=ttynull qcom_geni_serial.con_enabled=0"]
            board_bootconfig_extras += ["androidboot.serialconsole=0"]

            perf_build_img_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x04a90000",
                kernel_vendor_cmdline_extras = kernel_vendor_cmdline_extras,
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
                board_bootconfig_extras = board_bootconfig_extras,
            )

    define_typical_android_build(
        name = "shikra",
        consolidate_config = shikra_consolidate_config,
        perf_config = shikra_perf_config,
        consolidate_build_img_opts = consolidate_build_img_opts,
        perf_build_img_opts = perf_build_img_opts,
        consolidate_kwargs = {
            "config_path": "configs/shikra_consolidate.bzl",
        },
        perf_kwargs = {
            "config_path": "configs/shikra_perf.bzl",
        },
    )

def define_shikra_tuivm():
    define_typical_vm_build(
        name = "shikra-tuivm",
        config = shikra_tuivm_config,
        debug_config = shikra_tuivm_debug_config,
        dtb_target = "shikra-tuivm",
        debug_kwargs = {
            "config_path": "configs/shikra_tuivm_debug.bzl",
        },
        config_kwargs = {
            "config_path": "configs/shikra_tuivm.bzl",
        },
    )

def define_shikra_oemvm():
    define_typical_vm_build(
        name = "shikra-oemvm",
        config = shikra_tuivm_config,
        debug_config = shikra_tuivm_debug_config,
        dtb_target = "shikra-oemvm",
        # Do not set config_path because it conflicts with shikra-tuivm
    )
