def register_modules(registry):
    registry.register(
        name = "drivers/net/mdio_fe/emac-mdio-fe",
        out = "emac-mdio-fe.ko",
        config = "CONFIG_EMAC_MDIO_FE",
        srcs = [
            # do not sort
            "drivers/net/mdio_fe/emac_mdio_fe.c",
        ],
        hdrs = [
            "include/linux/emac_mdio_fe.h",
        ],
        includes = ["include/linux"],
        deps = [
            # do not sort
        ],
    )
