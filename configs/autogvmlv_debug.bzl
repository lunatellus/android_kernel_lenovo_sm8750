load(":configs/autogvmlv.bzl", "autogvmlv_config")

autogvmlv_debug_config = autogvmlv_config | {
    # keep sorted
    "CONFIG_MHI_BUS_DEBUG": "y",
    "CONFIG_USB_LINK_LAYER_TEST": "m",
}
