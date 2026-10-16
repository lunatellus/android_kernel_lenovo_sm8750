def register_modules(registry):
    registry.register(
        name = "drivers/eom/pcie/pci_phy_access",
        out = "pci_phy_access.ko",
        config = "CONFIG_PCI_MSM_EOM",
        srcs = [
            # do not sort
            "drivers/eom/pcie/pci_phy_access.c",
        ],
        includes = ["include/linux"],
        deps = [
            # do not sort
            "drivers/eom/eom_driver",
            "drivers/pci/controller/pci-msm-drv",
        ],
    )
