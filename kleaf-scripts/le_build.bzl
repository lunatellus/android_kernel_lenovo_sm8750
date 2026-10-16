load("@bazel_skylib//rules:write_file.bzl", "write_file")
load(":kleaf-scripts/msm_kernel_extensions.bzl", "define_extras", "get_vendor_ramdisk_binaries")
load("//build/kernel/kleaf:hermetic_tools.bzl", "hermetic_genrule")
load(
    "//build/kernel/kleaf:kernel.bzl",
    "ddk_headers",
    "kernel_build_config",
    "kernel_images",
    "merged_kernel_uapi_headers",
)
load(":kleaf-scripts/abl.bzl", "define_abl_dist")
load(":kleaf-scripts/generic_le.bzl", "define_le", "define_qcom_le")
load(":kleaf-scripts/msm_dtc.bzl", "define_dtc_dist")
load(":qcom_modules.bzl", "registry")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def define_common_le_rules():
    write_file(
        name = "qcom_le_image_config",
        out = "le.image.config",
        content = [
            "KERNEL_DIR=common",
            "SOC_DIR={}".format(native.package_name()),
            "KERNEL_BINARY=Image",
            "DO_NOT_STRIP_MODULES=0",
            "",
        ],
    )

    kernel_build_config(
        name = "build.config.qcom.le.images",
        srcs = [
            # do not sort
            ":qcom_le_image_config",
        ],
    )

def define_single_le_build(
        name,
        variant,
        config_fragment,
        base_kernel,
        build_img_opts = None,
        dtb_target = None,
        ddk_config_deps = None,
        implicit_config_fragment = None,
        config_path = None):
    stem = "{}_{}".format(name, variant)
    modules = registry.define_modules(
        stem,
        config_fragment,
        base_kernel,
        ddk_config_deps,
        implicit_config_fragment,
        config_path = config_path,
    )

    if dtb_target:
        dtb_list, dtbo_list = define_qcom_le(
            stem = stem,
            target = dtb_target,
            defconfig = ":arch/arm64/configs/generic_le_defconfig",
            cmdline = build_img_opts.board_kernel_cmdline_extras if build_img_opts else [""],
        )
    else:
        dtb_list = None
        dtbo_list = None

    native.alias(
        name = "{}_abl".format(stem),
        actual = "//bootable/bootloader/edk2:{}_abl".format(stem),
    )

    kernel_build_config(
        name = "{}_build_config".format(stem),
        srcs = [
            # do not sort
            ":qcom_le_image_config",
        ],
    )

    if build_img_opts != None:
        board_cmdline_extras = " ".join(build_img_opts.board_kernel_cmdline_extras)

        if board_cmdline_extras:
            hermetic_genrule(
                name = "{}_extra_cmdline".format(stem),
                outs = ["board_extra_cmdline_{}".format(stem)],
                cmd = """
                    echo {} > "$@"
                """.format(board_cmdline_extras),
            )

        board_bc_extras = " ".join(build_img_opts.board_bootconfig_extras)

        if board_bc_extras:
            hermetic_genrule(
                name = "{}_extra_bootconfig".format(stem),
                outs = ["board_extra_bootconfig_{}".format(stem)],
                cmd = """
                    echo {} > "$@"
                """.format(board_bc_extras),
            )

    kernel_images(
        name = "{}_images".format(stem),
        kernel_modules_install = ":{}_modules_install".format(stem),
        kernel_build = "{}_dtb_build".format(stem),
        base_kernel_images = None,
        dtbo_srcs = [":{}_dtb_build/{}".format(stem, dtbo) for dtbo in dtbo_list] if dtbo_list else None,
        build_boot = True,
        build_initramfs = True,
        build_dtbo = True if dtbo_list else False,
        modules_list = "modules-lists/modules.list.msm.{}".format(name),
        vendor_ramdisk_binaries = get_vendor_ramdisk_binaries(stem, flavor = "le"),
        boot_image_outs = ["boot.img"],
        deps = [
            "modules-lists/modules.list.msm.{}".format(name),
        ],
    )

    hermetic_genrule(
        name = "{}_merge_msm_uapi_headers".format(stem),
        srcs = [
            # do not sort
            ":{}_merged_kernel_uapi_headers".format(stem),
            "msm_uapi_headers",
        ],
        outs = ["{}_kernel-uapi-headers.tar.gz".format(stem)],
        cmd = """
            mkdir -p intermediate_dir
            for file in $(SRCS)
            do
            tar xf $$file -C intermediate_dir
            done
            tar czf $(OUTS) -C intermediate_dir usr/
            rm -rf intermediate_dir
        """,
    )

    merged_kernel_uapi_headers(
        name = "{}_merged_kernel_uapi_headers".format(stem),
        kernel_build = "{}_dtb_build".format(stem),
    )

    hermetic_genrule(
        name = "{}_tar_kernel_headers".format(stem),
        srcs = [
            # do not sort
            ":additional_msm_headers",
            "//common:all_headers",
        ],
        outs = ["{}_kernel-headers.tar.gz".format(stem)],
        cmd = """
            mkdir -p kernel-headers
            for file in $(SRCS)
            do
            cp -D $$file kernel-headers
            done
            tar czf $(OUTS) kernel-headers
            rm -rf kernel-headers
        """,
    )

    dist_data = [
        "{}".format(base_kernel),
        ":{}_modules_install".format(stem),
        "{}_dtb_build".format(stem),
        ":{}_images".format(stem),
        "{}_merge_msm_uapi_headers".format(stem),
        "{}_dtb_build_config".format(stem),
        "{}_tar_kernel_headers".format(stem),
        # Keys needed for kernel module signing and verification
        ":signing_key",
        ":verity_key",
    ] + [
        ":{}/{}".format(stem, module)
        for module in modules
    ]

    if build_img_opts != None:
        board_cmdline_extras = " ".join(build_img_opts.board_kernel_cmdline_extras)

        if board_cmdline_extras:
            dist_data.append("{}_extra_cmdline".format(stem))

        board_bc_extras = " ".join(build_img_opts.board_bootconfig_extras)

        if board_bc_extras:
            dist_data.append("{}_extra_bootconfig".format(stem))

    pkg_files(
        name = "{}_dist_files".format(stem),
        srcs = dist_data,
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_dist".format(stem),
        srcs = [":{}_dist_files".format(stem)],
        destdir = "out/msm-kernel-{}-{}/dist".format(name, variant),
    )

    define_abl_dist(stem, name, variant)

    define_dtc_dist(stem, name, variant)

    define_extras(stem, kbuild_config = base_kernel)

def define_le_build(
        name,
        configs,
        **kwargs):
    for (variant, options) in configs.items():
        define_single_le_build(
            name = name,
            variant = variant,
            **(options | kwargs)
        )

def define_typical_le_build(
        name,
        build_config,
        config,
        debug_config,
        perf_kwargs = None,
        debug_kwargs = None,
        debug_build_img_opts = None,
        build_img_opts = None,
        **kwargs):
    """Define a typical LE build target.

    Defines a typical LE build with two variants: one optimized for
    performance and another that has uses the debug(consolidate) kernel.

    Args:
      name: The name of this target.
      build_config: Target specific build.config.msm.<target_name> file.
      config: The configuration to use when building the performance (GKI)
        build.
      debug_config: The configuration to use when building the
        consolidate (debug) build.
      perf_kwargs: Additional keyword arguments to pass to
        `define_single_le_build` when building the performance variant.
      debug_kwargs: Additional keyword arguments to pass to
        `define_single_le_build` when building the consolidate variant.
      **kwargs: Additional keyword arguments to pass to
        `define_single_le_build` for all variants.
    """

    define_le(name, build_config)

    if perf_kwargs == None:
        perf_kwargs = dict()
    if debug_kwargs == None:
        debug_kwargs = dict()

    # See b/370450569#comment34
    common_info = "{}_common_info".format(name)
    ddk_headers(
        name = common_info,
        kconfigs = [":kconfig.msm.generated"],
        defconfigs = [":{}_defconfig_defconfig".format(name)],
    )

    define_le_build(
        name,
        configs = {
            "defconfig": {
                "config_fragment": config,
                "base_kernel": ":{}_kernel_aarch64_le".format(name),
                "build_img_opts": build_img_opts,
                "ddk_config_deps": [common_info],
            } | perf_kwargs,
            "debug-defconfig": {
                "config_fragment": debug_config,
                "base_kernel": ":{}_kernel_aarch64_le_debug".format(name),
                "build_img_opts": debug_build_img_opts,
                "ddk_config_deps": [common_info],
                "implicit_config_fragment": config,
            } | debug_kwargs,
        },
        dtb_target = name,
        **kwargs
    )
