def register_modules(registry):
    registry.register(
        name = "drivers/net/phy/qcom/qca808x",
        out = "qca808x.ko",
        config = "CONFIG_QCA808X_PHY",
        srcs = [
            # do not sort
            "drivers/net/phy/qcom/qca808x.c",
            "drivers/net/phy/qcom/qcom.h",
            "drivers/net/phy/qcom/qcom-phy-lib.c",
        ],
    )
