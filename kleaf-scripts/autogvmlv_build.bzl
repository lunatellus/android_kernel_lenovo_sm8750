load(
    ":kleaf-scripts/msm_kernel_extensions.bzl",
    "define_extras",
    "get_dtb_list",
    "get_dtbo_list",
)
load("//build/kernel/kleaf:hermetic_tools.bzl", "hermetic_genrule")
load("//build/kernel/kleaf:kernel.bzl", "ddk_headers", "merged_kernel_uapi_headers")
load(":kleaf-scripts/dtbs.bzl", "define_qcom_dtbs")
load(":kleaf-scripts/image_opts.bzl", "vm_image_opts")
load(":kleaf-scripts/msm_dtc.bzl", "define_dtc_dist")
load(":qcom_modules.bzl", "registry")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def define_make_vm_dtb_img(target, dtb_list, page_size):
    compiled_dtbs = [":{}_dtb_build/{}".format(target, t) for t in dtb_list]
    dtb_cmd = "compiled_dtb_list=\"{}\"\n".format(" ".join(["$(location {})".format(d) for d in compiled_dtbs]))
    dtb_cmd += """
      set +x
      $(location //prebuilts/kernel-build-tools:mkdtboimg) \\
        create "$@" --page_size={page_size} $${{compiled_dtb_list}}
      set -x
    """.format(page_size = page_size)

    hermetic_genrule(
        name = "{}_vm_dtb_img".format(target),
        srcs = compiled_dtbs,
        outs = ["{}-dtb.img".format(target)],
        tools = ["//prebuilts/kernel-build-tools:mkdtboimg"],
        cmd = dtb_cmd,
    )

    pkg_files(
        name = "{}_vm_dist_files".format(target),
        srcs = [
            "{}-dtb.img".format(target),
        ] + compiled_dtbs,
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_vm_dist".format(target),
        srcs = [":{}_vm_dist_files".format(target)],
        destdir = "out/msm-kernel-{}/dist".format(target),
    )

def define_make_vm_dtbo_img(target, dtbo_list):
    compiled_dtbos = [":{}_dtb_build/{}".format(target, t) for t in dtbo_list]

    pkg_files(
        name = "{}_vm_dtbo_dist_files".format(target),
        srcs = compiled_dtbos,
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_vm_dtbo_dist".format(target),
        srcs = [":{}_vm_dtbo_dist_files".format(target)],
        destdir = "out/msm-kernel-{}/dist".format(target),
    )

def define_single_autogvmlv_build(
        name,
        config_fragment,
        base_kernel,
        dtb_target = None,
        ddk_config_deps = None,
        implicit_config_fragment = None,
        config_path = None):
    modules = registry.define_modules(
        name,
        config_fragment,
        base_kernel,
        ddk_config_deps = ddk_config_deps,
        implicit_config_fragment = implicit_config_fragment,
        config_path = config_path,
    )

    if dtb_target:
        define_qcom_dtbs(
            stem = name,
            target = dtb_target,
            defconfig = ":arch/arm64/configs/generic_auto_defconfig",
        )

    hermetic_genrule(
        name = "{}_merge_msm_uapi_headers".format(name),
        srcs = [
            # do not sort
            ":{}_merged_kernel_uapi_headers".format(name),
            "msm_uapi_headers",
        ],
        outs = ["{}_kernel-uapi-headers.tar.gz".format(name)],
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
        name = "{}_merged_kernel_uapi_headers".format(name),
        kernel_build = base_kernel,
    )

    pkg_files(
        name = "{}_host_dist_files".format(name),
        srcs = [
            ":gen-headers_install.sh",
            ":unifdef",
        ],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_host_dist".format(name),
        srcs = [":{}_host_dist_files".format(name)],
        destdir = "out/msm-kernel-{}/host".format(name),
    )

    pkg_files(
        name = "{}_dist_files".format(name),
        srcs = [
            ":{}_modules_install".format(name),
            #":{}_signed_modules".format(name),
            ":{}_merge_msm_uapi_headers".format(name),
            ":signing_key",
            ":verity_key",
            base_kernel,
            ":{}_dtb_build/.config".format(name),
        ],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_dist".format(name),
        srcs = [":{}_dist_files".format(name)],
        destdir = "out/msm-kernel-{}/dist".format(name),
    )

def define_autogvmlv_build(
        name,
        configs,
        **kwargs):
    for (variant, options) in configs.items():
        define_single_autogvmlv_build(
            name = "{}_{}".format(name, variant),
            **(options | kwargs)
        )

        dtb_list = get_dtb_list(name)
        dtbo_list = get_dtbo_list(name)

        vm_opts = vm_image_opts()

        #use only dtbs related to the variant for dtb image creation
        seg_dtb_list = [dtb for dtb in dtb_list if "-vm-" in dtb]

        define_make_vm_dtb_img(name + "_" + variant, seg_dtb_list, vm_opts.dummy_img_size)

        seg_dtbo_list = [dtbo for dtbo in dtbo_list if "-vm-" in dtbo]

        define_make_vm_dtbo_img(name + "_" + variant, seg_dtbo_list)

        k_config = "kernel_aarch64_autogvmlv"
        if "debug" in variant:
            k_config = "kernel_aarch64_autogvmlv_debug"

        define_extras(name + "_" + variant, kbuild_config = k_config)
        define_dtc_dist(name + "_" + variant, "autogvm", variant)

def define_typical_autogvmlv_build(
        name,
        config,
        debug_config,
        config_kwargs = None,
        debug_kwargs = None,
        **kwargs):
    if config_kwargs == None:
        config_kwargs = dict()
    if debug_kwargs == None:
        debug_kwargs = dict()

    common_info = "{}_common_info".format(name)

    define_autogvmlv_build(
        name = name,
        configs = {
            "debug-defconfig": {
                "config_fragment": debug_config,
                "base_kernel": ":kernel_aarch64_autogvmlv_debug",
                "ddk_config_deps": [common_info],
                "implicit_config_fragment": config,
            } | debug_kwargs,
            "defconfig": {
                "config_fragment": config,
                "base_kernel": ":kernel_aarch64_autogvmlv",
                "ddk_config_deps": [common_info],
            } | config_kwargs,
        },
        **kwargs
    )
