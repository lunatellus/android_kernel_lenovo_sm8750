load(":kleaf-scripts/msm_kernel_extensions.bzl", "define_combined_vm_image", "define_extras", "get_dtb_list")
load(":kleaf-scripts/image_opts.bzl", "vm_image_opts")
load(":kleaf-scripts/msm_common.bzl", "get_out_dir")
load(":kleaf-scripts/msm_dtc.bzl", "define_dtc_dist")
load(":target_variants.bzl", "vm_types", "vm_variants")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

target_name = "malabar-vms"

def define_malabar_vms(vm_image_opts = vm_image_opts()):
    base_target = "malabar-tuivm"
    for variant in vm_variants:
        base_tv = "{}_{}".format(base_target, variant)

        dtb_list = get_dtb_list(base_target)
        compiled_dtbs = [":malabar-{}_{}_dtb_build/{}".format(vt, variant, t) for vt in vm_types for t in dtb_list]

        if variant == "debug-defconfig":
            base_kernel = "kernel_aarch64_qtvm_debug"
        else:
            base_kernel = "kernel_aarch64_qtvm"

        out_dtb_list = [":malabar-{}_{}_vm_dtb_img".format(vt, variant) for vt in vm_types]
        dist_targets = (
            ["malabar-{}_{}_vm_dist".format(vt, variant) for vt in vm_types] +
            ["malabar-{}_{}_dist".format(vt, variant) for vt in vm_types] +
            [":malabar-{}_{}_modules_install".format(vt, variant) for vt in vm_types] +
            [":malabar-{}_{}_signed_modules".format(vt, variant) for vt in vm_types] +
            ["malabar-{}_{}_merge_msm_uapi_headers".format(vt, variant) for vt in vm_types] +
            [base_kernel] +
            [":signing_key"] +
            [":verity_key"] +
            [":malabar-{}_{}_dtb_build".format(vt, variant) for vt in vm_types]
        ) + out_dtb_list

        pkg_files(
            name = "{}_{}_dist_files".format(target_name, variant),
            srcs = dist_targets + compiled_dtbs,
            visibility = ["//visibility:private"],
            strip_prefix = strip_prefix.files_only(),
        )

        pkg_install(
            name = "{}_{}_dist".format(target_name, variant),
            srcs = [":{}_{}_dist_files".format(target_name, variant)],
            destdir = "{}/dist".format(get_out_dir(target_name, variant)),
        )

        pkg_files(
            name = "{}_{}_host_dist_files".format(target_name, variant),
            srcs = [
                ":gen-headers_install.sh",
                ":unifdef",
            ],
            visibility = ["//visibility:private"],
            strip_prefix = strip_prefix.files_only(),
        )

        pkg_install(
            name = "{}_{}_host_dist".format(target_name, variant),
            srcs = [":{}_{}_host_dist_files".format(target_name, variant)],
            destdir = "{}/host".format(get_out_dir(target_name, variant)),
        )

        archive_targets = ["malabar-{}_{}_unstripped_modules_tar".format(vt, variant) for vt in vm_types]
        pkg_files(
            name = "{}_{}_um_dist_files".format(target_name, variant),
            srcs = archive_targets,
            visibility = ["//visibility:private"],
            strip_prefix = strip_prefix.files_only(),
        )

        pkg_install(
            name = "{}_{}_um_dist".format(target_name, variant),
            srcs = [":{}_{}_um_dist_files".format(target_name, variant)],
            destdir = "{}/dist".format(get_out_dir(target_name, variant)),
        )

        define_dtc_dist("{}_{}".format(target_name, variant), target_name, variant)
        define_extras(base_tv, kbuild_config = base_kernel, alias = "{}_{}".format(target_name, variant))
        define_combined_vm_image(target_name, variant, vm_image_opts.vm_size_ext4)
