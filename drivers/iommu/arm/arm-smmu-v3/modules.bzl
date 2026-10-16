def register_modules(registry):
    registry.register(
        name = "drivers/iommu/arm/arm-smmu-v3/arm_smmu_v3",
        out = "arm_smmu_v3.ko",
        config = "CONFIG_ARM_SMMU_V3",
        srcs = [
            # do not sort
            "drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.c",
            "drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3-common-lib.c",
            "drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3-common.c",
            "drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.h",
            "drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3-qcom-pm.c",
        ],
        conditional_srcs = {
            "CONFIG_ARM_SMMU_V3_QCOM_VIRTIO": {
                True: [
                    # do not sort
                    "drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3-qcom-virtio.c",
                ],
            },
        },
        deps = [
            # do not sort
            "drivers/iommu/virtio-iommu",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    """Register qcom-smmu-v2-v3-dispatcher EL1 host module that depends on EL2 hyp library."""
    registry.register(
        name = "drivers/iommu/arm/arm-smmu-v3",
        out = "qcom_smmu_v2_v3_dispatcher.ko",
        config = "CONFIG_QCOM_SMMU_V2_V3_DISPATCHER",
        srcs = [
            # do not sort
            "drivers/iommu/arm/arm-smmu-v3/arm-smmuv2-defs.h",
            "drivers/iommu/arm/arm-smmu/arm-smmu.h",
            "drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3.h",
            "drivers/iommu/arm/arm-smmu-v3/arm-smmu-qcom-pm.c",
            "drivers/iommu/arm/arm-smmu-v3/qcom_smmu_dispatcher.c",
            "drivers/iommu/arm/arm-smmu-v3/smmuv2_nesting.h",
            "drivers/iommu/arm/arm-smmu-v3/smmuv2_nesting.c",
            "drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3-kvm-nested.c",
            "drivers/iommu/arm/arm-smmu-v3/arm_smmu_v3_nested.h",
            "drivers/iommu/arm/arm-smmu-v3/arm-smmu-v3-common-lib.c",
        ],
        lib_deps = [
            "qcom-smmu-v2-v3-dispatcher-hyp-lib",
        ],
    )
