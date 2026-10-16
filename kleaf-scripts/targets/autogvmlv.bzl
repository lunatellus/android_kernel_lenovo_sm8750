load("//build/kernel/kleaf:kernel.bzl", "kernel_module_group")
load(":configs/autogvmlv.bzl", "autogvmlv_config")
load(":configs/autogvmlv_debug.bzl", "autogvmlv_debug_config")
load(":kleaf-scripts/autogvmlv_build.bzl", "define_typical_autogvmlv_build")

target = "autogvm"

def define_autogvmlv():
    define_typical_autogvmlv_build(
        name = "autogvm",
        config = autogvmlv_config,
        debug_config = autogvmlv_debug_config,
        dtb_target = "autogvm",
    )
