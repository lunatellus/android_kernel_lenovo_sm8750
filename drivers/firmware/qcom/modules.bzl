load(":drivers/firmware/qcom/pkvm-secure-memory-manager/modules.bzl", register_secure_memory_manager = "register_modules")
load(":drivers/firmware/qcom/pkvm-smc-filter/modules.bzl", register_pkvm_smc_filter = "register_modules")
load(":drivers/firmware/qcom/si_core/modules.bzl", register_si_core = "register_modules")

def register_modules(registry):
    register_si_core(registry)
    register_pkvm_smc_filter(registry)
    register_secure_memory_manager(registry)

    registry.register(
        name = "drivers/firmware/qcom/qcom-scm",
        out = "qcom-scm.ko",
        config = "CONFIG_QCOM_SCM",
        srcs = [
            # do not sort
            "drivers/firmware/qcom/qcom_scm-legacy.c",
            "drivers/firmware/qcom/qcom_scm-smc.c",
            "drivers/firmware/qcom/qcom_scm.c",
            "drivers/firmware/qcom/qcom_tzmem.h",
            "drivers/firmware/qcom/qcom_scm.h",
            "drivers/firmware/qcom/qcom_scm_trace.h",
            "drivers/firmware/qcom/qtee_shmbridge_internal.h",
            "drivers/firmware/qcom/lcp-ppddr-internal.h",
        ],
        hdrs = [
            "include/linux/firmware/qcom/qcom_scm.h",
        ],
        includes = ["include/linux"],
        conditional_srcs = {
            "CONFIG_QTEE_SHM_BRIDGE": {
                True: [
                    # do not sort
                    "drivers/firmware/qcom/qtee_shmbridge.c",
                ],
            },
            "CONFIG_MSM_HAB": {
                True: [
                    # do not sort
                    "drivers/firmware/qcom/qcom_scm_hab.c",
                ],
            },
            "CONFIG_QCOM_TZMEM": {
                True: [
                    # do not sort
                    "drivers/firmware/qcom/qcom_tzmem.c",
                ],
            },
            "CONFIG_QCOM_TZMEM_FFA": {
                True: [
                    # do not sort
                    "drivers/firmware/qcom/qcom_tzmem_ffa.c",
                ],
            },
            "CONFIG_QCOM_SCM_SMCI": {
                False: [
                    # do not sort
                    "drivers/firmware/qcom/qcom_scm-scm.c",
                ],
            },
        },
        deps = [
            # do not sort
            "drivers/firmware/arm_ffa_transport",
            "drivers/firmware/arm_ffa",
            "drivers/soc/qcom/hab/msm_hab",
        ],
    )

    registry.register(
        name = "drivers/firmware/qcom/qcom_scm_smci",
        out = "qcom_scm_smci.ko",
        config = "CONFIG_QCOM_SCM_SMCI",
        srcs = [
            # do not sort
            "drivers/firmware/qcom/qcom_scm-smci.c",
            "drivers/firmware/qcom/qcom_scm_smcinvoke.h",
            "drivers/firmware/qcom/qcom_scm.h",
        ],
        conditional_srcs = {
            "CONFIG_QCOM_SCM_SMCI": {
                True: [
                    # do not sort
                    "drivers/firmware/qcom/qcom_scm-smcinvoke.c",
                ],
            },
        },
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
            "drivers/firmware/qcom/si_core/si_core_module",
        ],
    )
