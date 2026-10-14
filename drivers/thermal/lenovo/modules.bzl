def register_modules(registry):
    registry.register(
        name = "drivers/thermal/lenovo/lenovo_sys_temp",
        out = "lenovo_sys_temp.ko",
        config = "CONFIG_LENOVO_SYS_TEMP",
        srcs = [
            "drivers/thermal/lenovo/lenovo_sys_temp.c",
            "drivers/thermal/lenovo/temperature_model.h",
        ],
        deps = ["drivers/misc/hq/hqsysfs/hqsysfs"],
    )
    registry.register(
        name = "drivers/thermal/lenovo/lenovo_thermal_control",
        out = "lenovo_thermal_control.ko",
        config = "CONFIG_LENOVO_THERMAL_CONTROL",
        srcs = ["drivers/thermal/lenovo/lenovo_thermal_control.c"],
    )
