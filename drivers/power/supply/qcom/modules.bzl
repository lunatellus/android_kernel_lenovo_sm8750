def register_modules(registry):
    registry.register(
        name = "drivers/power/supply/qcom/qpnp-smb5-main",
        out = "qpnp-smb5-main.ko",
        config = "CONFIG_QPNP_SMB5",
        srcs = [
            # do not sort
            "drivers/power/supply/qcom/storm-watch.h",
            "drivers/power/supply/qcom/battery.h",
            "drivers/power/supply/qcom/smb5-lib.h",
            "drivers/power/supply/qcom/smb5-iio.h",
            "drivers/power/supply/qcom/smb5-reg.h",
            "drivers/power/supply/qcom/battery-profile-loader.h",
            "drivers/power/supply/qcom/step-chg-jeita.h",
            "drivers/power/supply/qcom/schgm-flash.h",
            "drivers/power/supply/qcom/storm-watch.c",
            "drivers/power/supply/qcom/step-chg-jeita.c",
            "drivers/power/supply/qcom/battery.c",
            "drivers/power/supply/qcom/smb5-lib.c",
            "drivers/power/supply/qcom/pmic-voter.c",
            "drivers/power/supply/qcom/battery-profile-loader.c",
            "drivers/power/supply/qcom/smb5-iio.c",
            "drivers/power/supply/qcom/smbx-get-chan.c",
            "drivers/power/supply/qcom/qpnp-smb5.c",
            "drivers/power/supply/qcom/schgm-flash.c",
        ],
        deps = [
            # do not sort
            "drivers/pinctrl/qcom/pinctrl-spmi-gpio",
            "drivers/spmi/spmi-pmic-arb",
        ],
    )

    registry.register(
        name = "drivers/power/supply/qcom/qcom-qpnp-qg",
        out = "qcom-qpnp-qg.ko",
        config = "CONFIG_QPNP_QG",
        srcs = [
            # do not sort
            "include/uapi/linux/qg.h",
            "include/uapi/linux/qg-profile.h",
            "drivers/power/supply/qcom/smb5-iio.h",
            "drivers/power/supply/qcom/schgm-flash.h",
            "drivers/power/supply/qcom/step-chg-jeita.h",
            "drivers/power/supply/qcom/fg-alg.h",
            "drivers/power/supply/qcom/qg-core.h",
            "drivers/power/supply/qcom/qg-iio.h",
            "drivers/power/supply/qcom/qg-reg.h",
            "drivers/power/supply/qcom/qg-soc.h",
            "drivers/power/supply/qcom/qg-battery-profile.h",
            "drivers/power/supply/qcom/qg-defs.h",
            "drivers/power/supply/qcom/qg-profile-lib.h",
            "drivers/power/supply/qcom/qg-sdam.h",
            "drivers/power/supply/qcom/qg-util.h",
            "drivers/power/supply/qcom/battery-profile-loader.h",
            "drivers/power/supply/qcom/qpnp-qg.c",
            "drivers/power/supply/qcom/battery-profile-loader.c",
            "drivers/power/supply/qcom/pmic-voter.c",
            "drivers/power/supply/qcom/qg-util.c",
            "drivers/power/supply/qcom/qg-soc.c",
            "drivers/power/supply/qcom/qg-sdam.c",
            "drivers/power/supply/qcom/qg-battery-profile.c",
            "drivers/power/supply/qcom/qg-profile-lib.c",
            "drivers/power/supply/qcom/fg-alg.c",
        ],
        deps = [
            # do not sort
            "drivers/pinctrl/qcom/pinctrl-spmi-gpio",
            "drivers/spmi/spmi-pmic-arb",
        ],
    )

    registry.register(
        name = "drivers/power/supply/qcom/qcom-smb1398-charger",
        out = "qcom-smb1398-charger.ko",
        config = "CONFIG_SMB1398_CHARGER",
        srcs = [
            # do not sort
            "drivers/power/supply/qcom/smb1398-charger.c",
            "drivers/power/supply/qcom/pmic-voter.c",
        ],
        deps = [
            # do not sort
        ],
    )

    registry.register(
        name = "drivers/power/supply/qcom/qcom-smb1355-charger",
        out = "qcom-smb1355-charger.ko",
        config = "CONFIG_SMB1355_SLAVE_CHARGER",
        srcs = [
            # do not sort
            "drivers/power/supply/qcom/smb1355-charger.c",
            "drivers/power/supply/qcom/pmic-voter.c",
        ],
        deps = [
            # do not sort
        ],
    )

    registry.register(
        name = "drivers/power/supply/qcom/qpnp-smblite-main",
        out = "qpnp-smblite-main.ko",
        config = "CONFIG_QPNP_SMBLITE",
        srcs = [
            # do not sort
            "drivers/power/supply/qcom/smb5-iio.h",
            "drivers/power/supply/qcom/storm-watch.h",
            "drivers/power/supply/qcom/schgm-flashlite.h",
            "drivers/power/supply/qcom/smblite-lib.h",
            "drivers/power/supply/qcom/smblite-reg.h",
            "drivers/power/supply/qcom/smblite-remote-bms.h",
            "drivers/power/supply/qcom/battery.h",
            "drivers/power/supply/qcom/step-chg-jeita.h",
            "drivers/power/supply/qcom/battery-profile-loader.h",
            "include/linux/soc/qcom/slate_events_bridge_intf.h",
            "drivers/power/supply/qcom/step-chg-jeita.c",
            "drivers/power/supply/qcom/battery.c",
            "drivers/power/supply/qcom/qpnp-smblite.c",
            "drivers/power/supply/qcom/smblite-lib.c",
            "drivers/power/supply/qcom/pmic-voter.c",
            "drivers/power/supply/qcom/storm-watch.c",
            "drivers/power/supply/qcom/battery-profile-loader.c",
            "drivers/power/supply/qcom/schgm-flashlite.c",
            "drivers/power/supply/qcom/smblite-iio.c",
            "drivers/power/supply/qcom/smbx-get-chan.c",
            "drivers/power/supply/qcom/smblite-remote-bms.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/slate_events_bridge",
        ],
    )

    registry.register(
        name = "drivers/power/supply/qcom/qti-qbg-main",
        out = "qti-qbg-main.ko",
        config = "CONFIG_QTI_QBG",
        srcs = [
            # do not sort
            "drivers/power/supply/qcom/qbg-core.h",
            "drivers/power/supply/qcom/qbg-iio.h",
            "drivers/power/supply/qcom/qbg-sdam.h",
            "drivers/power/supply/qcom/qbg-battery-profile.h",
            "drivers/power/supply/qcom/battery-profile-loader.h",
            "drivers/power/supply/qcom/qti-qbg.c",
            "drivers/power/supply/qcom/qbg-sdam.c",
            "drivers/power/supply/qcom/qbg-battery-profile.c",
            "drivers/power/supply/qcom/battery-profile-loader.c",
        ],
        deps = [
            # do not sort
        ],
    )
