def register_libraries(registry):
    """Register qcom-smmu-v2-v3-dispatcher EL2 hypervisor libraries."""
    registry.register(
        name = "qcom-smmu-v2-v3-dispatcher-hyp-lib",
        srcs = [
            "drivers/iommu/arm/arm-smmu-v3/pkvm/arm-smmu-v3-module.h",
            "drivers/iommu/arm/arm-smmu-v3/pkvm/arm_smmu_v3.h",
            "drivers/iommu/arm/arm-smmu-v3/pkvm/arm-smmuv2-defs.h",
            "drivers/iommu/arm/arm-smmu-v3/pkvm/smmuv2_nesting.h",
            "drivers/iommu/arm/arm-smmu-v3/pkvm/smmuv2_nesting.c",
            "drivers/iommu/arm/arm-smmu-v3/pkvm/qcom_smmu_dispatcher.h",
            "drivers/iommu/arm/arm-smmu-v3/pkvm/qcom_smmu_dispatcher.c",
            "drivers/iommu/arm/arm-smmu-v3/pkvm/arm-smmu-v3-common-lib.c",
            "drivers/iommu/arm/arm-smmu-v3/pkvm/io-pgtable-arm-common.c",
            "drivers/iommu/arm/arm-smmu-v3/pkvm/io-pgtable-arm.c",
            "drivers/iommu/arm/arm-smmu-v3/pkvm/arm-smmu-v3-nested.c",
            "drivers/iommu/arm/arm-smmu-v3/pkvm/arm_smmu_v3_nested.h",
            "drivers/iommu/arm/arm-smmu-v3/pkvm/arm-smmu-v3.h",
        ],
        config = "CONFIG_QCOM_SMMU_V2_V3_DISPATCHER",
        pkvm_el2 = True,
    )
