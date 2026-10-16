def register_modules(registry):
    registry.register(
        name = "drivers/pwm/pwm-qcom",
        out = "pwm-qcom.ko",
        config = "CONFIG_PWM_QCOM",
        srcs = [
            # do not sort
            "drivers/pwm/pwm-qcom.c",
        ],
        deps = [
            # do not sort
            "drivers/clk/qcom/clk-qcom",
            "drivers/clk/qcom/gcc-pikachu",
        ],
    )
