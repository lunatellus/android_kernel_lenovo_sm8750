load("@bazel_skylib//lib:paths.bzl", "paths")
load(
    "//build/kernel/kleaf:kernel.bzl",
    "ddk_config",
    "ddk_library",
    "kernel_compile_commands",
)
load(
    ":kleaf-scripts/defconfig_fragment.bzl",
    "define_defconfig_fragment",
)

def _generate_ddk_libraries(
        lib_map,
        target_variant,
        config_fragment,
        base_kernel,
        ddk_config_deps,
        implicit_config_fragment,
        config_path,
        common_deps = [
            ":additional_msm_headers",
            "//common:all_headers",
        ]):
    if implicit_config_fragment:
        config_fragment = implicit_config_fragment | config_fragment
    lib_names = {}
    for config, libs in lib_map.items():
        for obj in libs:
            is_enabled = (config == None) or (config_fragment.get(config) in ["y", "m"])
            flat_name = "{}_{}".format(target_variant, obj.name)
            if is_enabled:
                lib_names[obj.name] = flat_name
            else:
                lib_names[obj.name] = "{}/{}".format(target_variant, obj.name)
    for config, libs in lib_map.items():
        for obj in libs:
            is_enabled = (config == None) or (config_fragment.get(config) in ["y", "m"])
            if is_enabled:
                flat_name = lib_names[obj.name]
                legacy_name = "{}/{}".format(target_variant, obj.name)
                deps = [":{}".format(lib_names.get(dep)) for dep in obj.deps if lib_names.get(dep)] + obj.lib_deps
                hdrs = [h for h in (obj.hdrs or [])]
                includes = (obj.includes or []) + {paths.dirname(hdr): "" for hdr in hdrs}.keys()
                actual_srcs = obj.srcs
                if obj.extra_args and obj.extra_args.get("pkvm_el2"):
                    actual_srcs = []
                    for idx, src in enumerate(obj.srcs):
                        copied_src_name = "{}/{}/{}".format(target_variant, obj.name, src)
                        safe_src_name = src.replace("/", "_")
                        genrule_name = "{}_{}_src_{}_{}".format(target_variant, obj.name, idx, safe_src_name)
                        native.genrule(
                            name = genrule_name,
                            srcs = [src],
                            outs = [copied_src_name],
                            cmd = "cp $< $@",
                            visibility = ["//visibility:private"],
                        )
                        actual_srcs.append(copied_src_name)
                src_hdrs = [src for src in actual_srcs if src.endswith(".h")]
                extra_includes = {paths.dirname(hdr): "" for hdr in src_hdrs}.keys()
                ddk_library(
                    name = flat_name,
                    srcs = actual_srcs,
                    hdrs = obj.hdrs,
                    deps = deps + common_deps,
                    includes = includes + extra_includes,
                    kernel_build = ":{}_base_kernel".format(target_variant),
                    config = ":{}_config".format(target_variant),
                    visibility = ["//visibility:public"],
                    **(obj.extra_args or {})
                )
                if flat_name != legacy_name:
                    native.alias(
                        name = legacy_name,
                        actual = ":" + flat_name,
                        visibility = ["//visibility:public"],
                    )
            else:
                native.alias(
                    name = "{}/{}".format(target_variant, obj.name),
                    actual = ":all_headers",
                    visibility = ["//visibility:public"],
                )
    kernel_compile_commands(
        name = "{}_libraries_compile_commands".format(target_variant),
        deps = [name for name in lib_names.values() if "_" in name],
    )
    return lib_names.values()

def create_library_registry():
    lib_map = {}

    def register(
            name,
            srcs,
            hdrs = None,
            config = None,
            deps = None,
            lib_deps = None,
            includes = None,
            **kwargs):
        """Register a library with the registry.

        Args:
          name: A unique name for the library.
            For example: pkvm-geni-hyp-lib
          srcs: A list of source files to compile the library.
          hdrs: A list of header files exported by this library.
          config: A Kconfig symbol which needs to be enabled for this library to
            be compiled.
            Optional. If unspecified, the library will be built for every target.
          deps: List of dependent libraries.
          lib_deps: List of dependent DDK libraries.
          includes: List of include directories exported by this library.
          **kwargs: Additional ddk_library() arguments.
            For example: pkvm_el2 = True
        """
        if not lib_map.get(config):
            lib_map[config] = []
        lib_map[config].append(struct(
            name = name,
            srcs = srcs,
            hdrs = hdrs,
            config = config,
            deps = deps or [],
            lib_deps = lib_deps or [],
            includes = includes,
            extra_args = kwargs,
        ))

    def define_libraries(
            target_variant,
            config_fragment,
            base_kernel,
            ddk_config_deps = None,
            implicit_config_fragment = None,
            config_path = None,
            common_deps = [
                ":additional_msm_headers",
                "//common:all_headers",
            ]):
        """Define register libraries for a target/variant.

        Creates the following rules:
          {target_variant}_{lib_name} - ddk_library target (flat name)
          {target_variant}_libraries_compile_commands - compile_commands for all libraries

        Args:
            target_variant: Base name of the target (e.g. hamoa_consolidate)
            config_fragment: A dictionary containing Kconfig symbols and their values.
            base_kernel: A kernel_build() to base the library build.
            ddk_config_deps: Additional dependencies to pass to ddk_config().
            implicit_config_fragment: Additional Kconfig symbols.
            config_path: Path to the file which declares config_fragment.
            common_deps: List of dependencies common to all libraries.

        Returns:
            The list of all enabled libraries (flat names).
        """
        return _generate_ddk_libraries(
            lib_map,
            target_variant,
            config_fragment,
            base_kernel,
            ddk_config_deps,
            implicit_config_fragment,
            config_path,
            common_deps,
        )

    return struct(
        lib_map = lib_map,
        register = register,
        get = lib_map.get,
        define_libraries = define_libraries,
    )
