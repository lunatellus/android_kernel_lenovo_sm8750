// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/clk-provider.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,x1p42100-av1ecc.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "clk-regmap-mux.h"
#include "common.h"
#include "gdsc.h"
#include "reset.h"
#include "vdd-level.h"

static DEFINE_VDD_REGULATORS(vdd_mm, VDD_NOMINAL + 1, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mxc, VDD_NOMINAL + 1, 1, vdd_corner);

static struct clk_vdd_class *av1e_cc_x1p42100_regulators[] = {
	&vdd_mm,
	&vdd_mxc,
};

enum {
	P_BI_TCXO,
	P_AV1E_CC_PLL0_OUT_MAIN,
	P_SLEEP_CLK,
};

static const struct pll_vco lucid_ole_vco[] = {
	{ 249600000, 2300000000, 0 },
};

/* 600.0 MHz Configuration */
static const struct alpha_pll_config av1e_cc_pll0_config = {
	.l = 0x1f,
	.cal_l = 0x44,
	.cal_l_ringosc = 0x44,
	.alpha = 0x4000,
	.config_ctl_val = 0x20485699,
	.config_ctl_hi_val = 0x00182261,
	.config_ctl_hi1_val = 0x82aa299c,
	.test_ctl_val = 0x00000000,
	.test_ctl_hi_val = 0x00000003,
	.test_ctl_hi1_val = 0x00009000,
	.test_ctl_hi2_val = 0x00000034,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000005,
};

static struct clk_alpha_pll av1e_cc_pll0 = {
	.offset = 0x0,
	.vco_table = lucid_ole_vco,
	.num_vco = ARRAY_SIZE(lucid_ole_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_OLE],
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "av1e_cc_pll0",
			.parent_data = &(const struct clk_parent_data) {
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_lucid_ole_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mm,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_LOWER_D1] = 615000000,
				[VDD_LOW] = 1100000000,
				[VDD_LOW_L1] = 1600000000,
				[VDD_NOMINAL] = 2000000000,
				[VDD_HIGH_L1] = 2300000000},
		},
	},
};

static const struct parent_map av1e_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_AV1E_CC_PLL0_OUT_MAIN, 1 },
};

static const struct clk_parent_data av1e_cc_parent_data_0[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &av1e_cc_pll0.clkr.hw },
};

static const struct parent_map av1e_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
};

static const struct clk_parent_data av1e_cc_parent_data_1[] = {
	{ .fw_name = "bi_tcxo" },
};

static const struct parent_map av1e_cc_parent_map_2[] = {
	{ P_SLEEP_CLK, 0 },
};

static const struct clk_parent_data av1e_cc_parent_data_2[] = {
	{ .fw_name = "sleep_clk" },
};

static const struct freq_tbl ftbl_av1e_cc_av1e_core_clk_src[] = {
	F(300000000, P_AV1E_CC_PLL0_OUT_MAIN, 2, 0, 0),
	F(400000000, P_AV1E_CC_PLL0_OUT_MAIN, 2, 0, 0),
	F(600000000, P_AV1E_CC_PLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 av1e_cc_av1e_core_clk_src = {
	.cmd_rcgr = 0x4018,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = av1e_cc_parent_map_0,
	.freq_tbl = ftbl_av1e_cc_av1e_core_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "av1e_cc_av1e_core_clk_src",
		.parent_data = av1e_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(av1e_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_classes = av1e_cc_x1p42100_regulators,
		.num_vdd_classes = ARRAY_SIZE(av1e_cc_x1p42100_regulators),
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 300000000,
			[VDD_LOW] = 400000000,
			[VDD_NOMINAL] = 600000000},
	},
};

static const struct freq_tbl ftbl_av1e_cc_av1e_xo_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 av1e_cc_av1e_xo_clk_src = {
	.cmd_rcgr = 0x601c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = av1e_cc_parent_map_1,
	.freq_tbl = ftbl_av1e_cc_av1e_xo_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "av1e_cc_av1e_xo_clk_src",
		.parent_data = av1e_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(av1e_cc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 19200000},
	},
};

static const struct freq_tbl ftbl_av1e_cc_sleep_clk_src[] = {
	F(32000, P_SLEEP_CLK, 1, 0, 0),
	{ }
};

static struct clk_rcg2 av1e_cc_sleep_clk_src = {
	.cmd_rcgr = 0x603c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = av1e_cc_parent_map_2,
	.freq_tbl = ftbl_av1e_cc_sleep_clk_src,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "av1e_cc_sleep_clk_src",
		.parent_data = av1e_cc_parent_data_2,
		.num_parents = ARRAY_SIZE(av1e_cc_parent_data_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_class = &vdd_mm,
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER] = 32000},
	},
};

static struct clk_regmap_div av1e_cc_av1e_noc_ahb_div_clk_src = {
	.reg = 0x5028,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "av1e_cc_av1e_noc_ahb_div_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&av1e_cc_av1e_core_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_branch av1e_cc_av1e_core_axi_clk = {
	.halt_reg = 0x403c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x403c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "av1e_cc_av1e_core_axi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&av1e_cc_av1e_core_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch av1e_cc_av1e_core_clk = {
	.halt_reg = 0x4030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4030,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "av1e_cc_av1e_core_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&av1e_cc_av1e_core_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch av1e_cc_av1e_gdsc_noc_ahb_clk = {
	.halt_reg = 0x502c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x502c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "av1e_cc_av1e_gdsc_noc_ahb_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&av1e_cc_av1e_noc_ahb_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch av1e_cc_av1e_noc_core_axi_clk = {
	.halt_reg = 0x5004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x5004,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "av1e_cc_av1e_noc_core_axi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&av1e_cc_av1e_core_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct gdsc av1e_cc_core_gdsc = {
	.gdscr = 0x4004,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0xf,
	.pd = {
		.name = "av1e_cc_core_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE,
	.supply = "vdd_mm_mxc_voter",
};

static struct clk_regmap *av1e_cc_x1p42100_clocks[] = {
	[AV1E_CC_AV1E_CORE_AXI_CLK] = &av1e_cc_av1e_core_axi_clk.clkr,
	[AV1E_CC_AV1E_CORE_CLK] = &av1e_cc_av1e_core_clk.clkr,
	[AV1E_CC_AV1E_CORE_CLK_SRC] = &av1e_cc_av1e_core_clk_src.clkr,
	[AV1E_CC_AV1E_GDSC_NOC_AHB_CLK] = &av1e_cc_av1e_gdsc_noc_ahb_clk.clkr,
	[AV1E_CC_AV1E_NOC_AHB_DIV_CLK_SRC] = &av1e_cc_av1e_noc_ahb_div_clk_src.clkr,
	[AV1E_CC_AV1E_NOC_CORE_AXI_CLK] = &av1e_cc_av1e_noc_core_axi_clk.clkr,
	[AV1E_CC_AV1E_XO_CLK_SRC] = &av1e_cc_av1e_xo_clk_src.clkr,
	[AV1E_CC_PLL0] = &av1e_cc_pll0.clkr,
	[AV1E_CC_SLEEP_CLK_SRC] = &av1e_cc_sleep_clk_src.clkr,
};

static struct gdsc *av1e_cc_x1p42100_gdscs[] = {
	[AV1E_CC_CORE_GDSC] = &av1e_cc_core_gdsc,
};

static const struct qcom_reset_map av1e_cc_x1p42100_resets[] = {
	[AV1E_CC_AV1E_CC_XO_CLK_ARES] = { 0x6034, 2 },
	[AV1E_CC_AV1E_CORE_AXI_CLK_ARES] = { 0x403c, 2 },
	[AV1E_CC_AV1E_CORE_CLK_ARES] = { 0x4030, 2 },
	[AV1E_CC_AV1E_GDSC_NOC_AHB_CLK_ARES] = { 0x502c, 2 },
	[AV1E_CC_AV1E_NOC_CORE_AXI_CLK_ARES] = { 0x5004, 2 },
	[AV1E_CC_AV1E_NOC_XO_CLK_ARES] = { 0x6038, 2 },
	[AV1E_CC_CORE_BCR] = { 0x4000 },
	[AV1E_CC_NOC_BCR] = { 0x5000 },
};

static const struct regmap_config av1e_cc_x1p42100_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xa01c,
	.fast_io = true,
};

static struct qcom_cc_desc av1e_cc_x1p42100_desc = {
	.config = &av1e_cc_x1p42100_regmap_config,
	.clks = av1e_cc_x1p42100_clocks,
	.num_clks = ARRAY_SIZE(av1e_cc_x1p42100_clocks),
	.resets = av1e_cc_x1p42100_resets,
	.num_resets = ARRAY_SIZE(av1e_cc_x1p42100_resets),
	.clk_regulators = av1e_cc_x1p42100_regulators,
	.num_clk_regulators = ARRAY_SIZE(av1e_cc_x1p42100_regulators),
	.gdscs = av1e_cc_x1p42100_gdscs,
	.num_gdscs = ARRAY_SIZE(av1e_cc_x1p42100_gdscs),
};

static const struct of_device_id av1e_cc_x1p42100_match_table[] = {
	{ .compatible = "qcom,x1p42100-av1ecc" },
	{ }
};
MODULE_DEVICE_TABLE(of, av1e_cc_x1p42100_match_table);

static int av1e_cc_x1p42100_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	regmap = qcom_cc_map(pdev, &av1e_cc_x1p42100_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = qcom_cc_runtime_init(pdev, &av1e_cc_x1p42100_desc);
	if (ret)
		return ret;

	ret = pm_runtime_resume_and_get(&pdev->dev);
	if (ret)
		return ret;

	clk_lucid_ole_pll_configure(&av1e_cc_pll0, regmap, &av1e_cc_pll0_config);

	/*
	 * Keep clocks always enabled:
	 *	av1e_cc_ahb_clk
	 *	av1e_cc_av1e_cc_xo_clk
	 *	av1e_cc_av1e_noc_ahb_clk
	 *	av1e_cc_sleep_clk
	 */
	regmap_update_bits(regmap, 0x6000, BIT(0), BIT(0));
	regmap_update_bits(regmap, 0x6034, BIT(0), BIT(0));
	regmap_update_bits(regmap, 0x5030, BIT(0), BIT(0));
	regmap_update_bits(regmap, 0x6054, BIT(0), BIT(0));

	ret = qcom_cc_really_probe(&pdev->dev, &av1e_cc_x1p42100_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register AV1E CC clocks ret=%d\n", ret);
		return ret;
	}

	pm_runtime_put_sync(&pdev->dev);
	dev_info(&pdev->dev, "Registered AV1E CC clocks\n");

	return ret;
}

static void av1e_cc_x1p42100_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &av1e_cc_x1p42100_desc);
}

static const struct dev_pm_ops av1e_cc_x1p42100_pm_ops = {
	SET_RUNTIME_PM_OPS(qcom_cc_runtime_suspend, qcom_cc_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
};

static struct platform_driver av1e_cc_x1p42100_driver = {
	.probe = av1e_cc_x1p42100_probe,
	.driver = {
		.name = "av1ecc-x1p42100",
		.of_match_table = av1e_cc_x1p42100_match_table,
		.sync_state = av1e_cc_x1p42100_sync_state,
		.pm = &av1e_cc_x1p42100_pm_ops,
	},
};

module_platform_driver(av1e_cc_x1p42100_driver);

MODULE_DESCRIPTION("QTI AV1ECC X1P42100 Driver");
MODULE_LICENSE("GPL");
