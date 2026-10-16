load(":drivers/firmware/qcom/pkvm-secure-memory-manager/hyp/libraries.bzl", register_secure_memory_manager_libs = "register_libraries")
load(":drivers/firmware/qcom/pkvm-smc-filter/hyp/libraries.bzl", register_pkvm_smc_filter_libs = "register_libraries")
load(":drivers/iommu/arm/arm-smmu-v3/pkvm/libraries.bzl", register_qcom_v2_v3_dispatcher_libs = "register_libraries")
load(":drivers/tty/serial/pkvm-geni/hyp/libraries.bzl", register_pkvm_geni_hyp_libs = "register_libraries")

def register_libraries(registry):
    register_pkvm_geni_hyp_libs(registry)
    register_pkvm_smc_filter_libs(registry)
    register_secure_memory_manager_libs(registry)
    register_qcom_v2_v3_dispatcher_libs(registry)
