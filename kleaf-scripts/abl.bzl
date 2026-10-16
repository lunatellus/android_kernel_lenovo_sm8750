load(":kleaf-scripts/msm_common.bzl", "get_out_dir")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files", "strip_prefix")

def define_abl_dist(target, msm_target, variant):
    """Creates ABL distribution target

    Args:
      target: name of main Bazel target (e.g. `kalama_gki`)
    """

    pkg_files(
        name = "{}_abl_dist_files".format(target),
        srcs = ["//bootable/bootloader/edk2:{}_abl".format(target)],
        visibility = ["//visibility:private"],
        strip_prefix = strip_prefix.files_only(),
    )

    pkg_install(
        name = "{}_abl_dist".format(target),
        srcs = [":{}_abl_dist_files".format(target)],
        destdir = "{}/dist".format(get_out_dir(msm_target, variant)),
    )
