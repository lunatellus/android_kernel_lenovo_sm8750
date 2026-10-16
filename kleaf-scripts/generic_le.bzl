load("@bazel_skylib//rules:write_file.bzl", "write_file")
load(":kleaf-scripts/msm_kernel_extensions.bzl", "get_dtb_list", "get_dtbo_list", "get_dtstree")
load("//build/kernel/kleaf:constants.bzl", "aarch64_outs")
load(
    "//build/kernel/kleaf:kernel.bzl",
    "gki_artifacts",
    "kernel_build",
    "kernel_build_config",
    "kernel_compile_commands",
    "kernel_images",
    "kernel_modules_install",
    "kernel_unstripped_modules_archive",
)
load("//common:modules.bzl", "get_gki_modules_list", "get_kunit_modules_list")
load(":kleaf-scripts/image_opts.bzl", "boot_image_opts")

def define_qcom_le_setup(name, config_file):
    """
    Generates a generic LE build config.
    Args:
        config_file: Label of a Bazel file containing overrides.

    """
    le_boot_opts = boot_image_opts()

    # defaults including le_boot_opts
    defaults = {
        "KERNEL_DIR": "common",
        "SOC_DIR": native.package_name(),
        "export DTC_INCLUDE": "${ROOT_DIR}/${SOC_DIR}/include",
        "BOOT_IMAGE_HEADER_VERSION": le_boot_opts.boot_image_header_version,
        "LZ4_RAMDISK": le_boot_opts.lz4_ramdisk,
        "PAGE_SIZE": le_boot_opts.page_size,
        "BASE_ADDRESS": le_boot_opts.base_address,
        "DTC_FLAGS": "\"-@\"",
        "KERNEL_BINARY": "Image",
    }

    # write defaults to a file
    write_file(
        name = "{}_build_config".format(name),
        out = "{}.build.config.msm.le".format(name),
        content = ["{}={}".format(k, v) for k, v in defaults.items()] + [""],
    )

    # merge: defaults → user config
    config_sources = [
        ":{}_build_config".format(name),
        config_file,
    ]

    kernel_build_config(
        name = "{}.build.config.qcom.le".format(name),
        srcs = config_sources,
    )

def define_qcom_le(
        stem,
        target,
        defconfig,
        cmdline = [""]):
    write_file(
        name = "{}_cmdline_extras".format(stem),
        out = "{}.cmdline.extras".format(stem),
        content = ["KERNEL_VENDOR_CMDLINE='{}'".format(" ".join(cmdline)), ""],
    )

    kernel_build_config(
        name = "{}.build.config.qcom.le".format(stem),
        srcs = [
            ":{}_cmdline_extras".format(stem),
            ":{}.build.config.qcom.le".format(target),  # merged build config
        ],
    )

    dtb_list = get_dtb_list(target)
    dtbo_list = get_dtbo_list(target)

    kernel_build(
        name = "{}_dtb_build".format(stem),
        srcs = [
            ":additional_msm_headers_aarch64_globs",
            "//common:kernel_aarch64_sources",
            ":soc_repo_sources",
        ],
        build_config = ":{}.build.config.qcom.le".format(stem),
        dtstree = get_dtstree(target),
        outs = dtb_list + dtbo_list + [
            "vmlinux",
            "Image",
            "System.map",
            ".config",
        ],
        base_kernel = ":{}_base_kernel".format(stem),
        kconfig_ext = ":kconfig.msm.generated",
        makefile = "//common:Makefile",
        defconfig = defconfig,
        system_trusted_key = ":verity_cert.pem",
        module_signing_key = ":signing_key",
        post_defconfig_fragments = [
            ":{}_merged_defconfig".format(stem),
        ],
        make_goals = [
            "dtbs",
        ],
        visibility = ["//visibility:public"],
    )
    return [dtb_list, dtbo_list]

def define_base_kernel(name, base_kernel, defconfig, defconfig_fragments = None):
    out_list = aarch64_outs + [
        ".config",
        "Module.symvers",
        "utsrelease.h",
        "scripts/sign-file",
        "certs/signing_key.x509",
    ]
    for item in ["Image.lz4", "Image.gz"]:
        if item in out_list:
            out_list.remove(item)
    kernel_build(
        name = base_kernel,
        srcs = [
            "//common:kernel_aarch64_sources",
            ":soc_repo_sources",
        ],
        outs = out_list,
        arch = "arm64",
        make_goals = [
            "Image",
            "modules",
        ],
        build_config = ":{}.build.config.qcom.le".format(name),
        defconfig = defconfig,
        pre_defconfig_fragments = defconfig_fragments,
        module_outs = get_gki_modules_list("arm64") + get_kunit_modules_list("arm64"),
        keep_module_symvers = True,
        pack_module_env = True,
        system_trusted_key = ":verity_cert.pem",
        module_signing_key = ":signing_key",
        visibility = ["//visibility:public"],
    )

def define_le(name, build_config):
    define_qcom_le_setup(name, build_config)
    define_base_kernel(
        name,
        "{}_kernel_aarch64_le".format(name),
        "arch/arm64/configs/generic_le_defconfig",
    )
    define_base_kernel(
        name,
        "{}_kernel_aarch64_le_debug".format(name),
        "arch/arm64/configs/generic_le_defconfig",
        defconfig_fragments = ["arch/arm64/configs/consolidate.fragment"],
    )
