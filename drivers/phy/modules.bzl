def register_modules(registry):
    registry.register(
        name = "drivers/phy/phy-nxp-ptn3222",
        out = "phy-nxp-ptn3222.ko",
        config = "CONFIG_PHY_NXP_PTN3222",
        srcs = [
            # do not sort
            "drivers/phy/phy-nxp-ptn3222.c",
        ],
    )
