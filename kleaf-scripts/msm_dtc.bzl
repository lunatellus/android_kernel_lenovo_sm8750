load("//build/kernel/kleaf:hermetic_tools.bzl", "hermetic_genrule")
load(":kleaf-scripts/msm_common.bzl", "get_out_dir")
load("@rules_pkg//pkg:install.bzl", "pkg_install")
load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files", "strip_prefix")

def define_dtc_dist(target, msm_target, variant):
    """Create distribution targets for device tree compiler and associated tools

    Args:
      target: name of main Bazel target (e.g. `kalama_gki`)
    """
    dtc_bin_targets = [
        "@dtc//:dtc",
        "@dtc//:fdtget",
        "@dtc//:fdtput",
        "@dtc//:fdtdump",
        "@dtc//:fdtoverlay",
        "@dtc//:fdtoverlaymerge",
    ]
    dtc_lib_targets = [
        "@dtc//:dtc_gen",
        "@dtc//:libfdt",
    ]
    dtc_inc_targets = [
        "@dtc//:libfdt/fdt.h",
        "@dtc//:libfdt/libfdt.h",
        "@dtc//:libfdt/libfdt_env.h",
    ]

    dtc_tar_cmd = "mkdir -p bin lib include\n"
    for label in dtc_bin_targets:
        dtc_tar_cmd += "cp $(locations {}) bin/\n".format(label)
    for label in dtc_lib_targets:
        dtc_tar_cmd += "cp $(locations {}) lib/\n".format(label)
    for label in dtc_inc_targets:
        dtc_tar_cmd += "cp $(locations {}) include/\n".format(label)
    dtc_tar_cmd += """
      chmod 755 bin/* lib/*
      chmod 644 include/*
      tar -czf "$@" bin lib include
    """

    hermetic_genrule(
        name = "{}_dtc_tarball".format(target),
        srcs = dtc_bin_targets + dtc_lib_targets + dtc_inc_targets,
        outs = ["{}_dtc.tar.gz".format(target)],
        cmd = dtc_tar_cmd,
    )

    native.alias(
        name = "{}_dtc".format(target),
        actual = ":{}_dtc_tarball".format(target),
    )

    for (kind, targets, mode) in [
        ("bin", dtc_bin_targets, "755"),
        ("lib", dtc_lib_targets, "755"),
        ("include", dtc_inc_targets, "644"),
    ]:
        pkg_files(
            name = "{}_dtc_{}_files".format(target, kind),
            srcs = targets,
            prefix = kind,
            attributes = pkg_attributes(mode = mode),
            strip_prefix = strip_prefix.files_only(),
            visibility = ["//visibility:private"],
        )

    pkg_install(
        name = "{}_dtc_dist".format(target),
        srcs = [
            ":{}_dtc_bin_files".format(target),
            ":{}_dtc_lib_files".format(target),
            ":{}_dtc_include_files".format(target),
        ],
        destdir = "{}/host".format(get_out_dir(msm_target, variant)),
    )
