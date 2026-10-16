def register_modules(registry):
    registry.register(
        name = "drivers/cpuidle/governors/qcom_lpm",
        out = "qcom_lpm.ko",
        config = "CONFIG_CPU_IDLE_GOV_QCOM_LPM",
        srcs = [
            # do not sort
            "drivers/cpuidle/governors/qcom-cluster-lpm.c",
            "drivers/cpuidle/governors/qcom-lpm-sysfs.c",
            "drivers/cpuidle/governors/qcom-lpm.c",
            "drivers/cpuidle/governors/qcom-lpm.h",
            "drivers/cpuidle/governors/trace-cluster-lpm.h",
            "drivers/cpuidle/governors/trace-qcom-lpm.h",
        ],
        deps = [
            # do not sort
            "kernel/sched/walt/sched-walt",
            "drivers/soc/qcom/socinfo",
            "drivers/soc/qcom/smem",
        ],
    )

    registry.register(
        name = "drivers/cpuidle/governors/qcom_simple_lpm",
        out = "qcom_simple_lpm.ko",
        config = "CONFIG_CPU_IDLE_SIMPLE_GOV_QCOM_LPM",
        srcs = [
            # do not sort
            "drivers/cpuidle/governors/qcom-simple-cluster-lpm.c",
            "drivers/cpuidle/governors/qcom-simple-lpm-sysfs.c",
            "drivers/cpuidle/governors/qcom-simple-lpm.c",
            "drivers/cpuidle/governors/qcom-simple-lpm.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/socinfo",
            "drivers/soc/qcom/smem",
        ],
    )
