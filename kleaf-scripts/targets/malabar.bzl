load("//build/kernel/kleaf:kernel.bzl", "kernel_abi", "kernel_module_group")
load(":configs/malabar_consolidate.bzl", "malabar_consolidate_config")
load(":configs/malabar_perf.bzl", "malabar_perf_config")
load(":configs/malabar_tuivm.bzl", "malabar_tuivm_config")
load(":configs/malabar_tuivm_debug.bzl", "malabar_tuivm_debug_config")
load(":kleaf-scripts/android_build.bzl", "define_typical_android_build")
load(":kleaf-scripts/image_opts.bzl", "boot_image_opts")
load(":kleaf-scripts/vm_build.bzl", "define_typical_vm_build")
load(":target_variants.bzl", "la_variants")

target_name = "malabar"

def define_malabar():
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
                "ufshcd_core.uic_cmd_timeout=2000",
            ]

            consolidate_build_img_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x4c8c000",
                kernel_vendor_cmdline_extras = kernel_vendor_cmdline_extras,
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
                board_bootconfig_extras = board_bootconfig_extras,
            )

        else:
            board_kernel_cmdline_extras += ["nosoftlockup console=ttynull qcom_geni_serial.con_enabled=0"]
            kernel_vendor_cmdline_extras += ["nosoftlockup console=ttynull qcom_geni_serial.con_enabled=0"]
            board_bootconfig_extras += ["androidboot.serialconsole=0"]

            perf_build_img_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x4c8c000",
                kernel_vendor_cmdline_extras = kernel_vendor_cmdline_extras,
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
                board_bootconfig_extras = board_bootconfig_extras,
            )

    define_typical_android_build(
        name = "malabar",
        consolidate_config = malabar_consolidate_config,
        perf_config = malabar_perf_config,
        consolidate_build_img_opts = consolidate_build_img_opts,
        perf_build_img_opts = perf_build_img_opts,
        consolidate_kwargs = {
            "config_path": "configs/malabar_consolidate.bzl",
        },
        perf_kwargs = {
            "config_path": "configs/malabar_perf.bzl",
        },
    )

    kernel_abi(
        name = "malabar_perf_abi",
        kernel_build = "//common:kernel_aarch64",
        kernel_modules = [
            ":malabar_perf_all_modules",
        ],
    )

def define_malabar_tuivm():
    define_typical_vm_build(
        name = "malabar-tuivm",
        config = malabar_tuivm_config,
        debug_config = malabar_tuivm_debug_config,
        dtb_target = "malabar-tuivm",
        debug_kwargs = {
            "config_path": "configs/malabar_tuivm_debug.bzl",
        },
        config_kwargs = {
            "config_path": "configs/malabar_tuivm.bzl",
        },
    )

def define_malabar_oemvm():
    define_typical_vm_build(
        name = "malabar-oemvm",
        config = malabar_tuivm_config,
        debug_config = malabar_tuivm_debug_config,
        dtb_target = "malabar-oemvm",
        # Do not set config_path because it conflicts with malabar-tuivm
    )
