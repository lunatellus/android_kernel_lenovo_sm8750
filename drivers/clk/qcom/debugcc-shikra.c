// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) "clk: %s: " fmt, __func__

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "clk-debug.h"
#include "common.h"

static struct measure_clk_data debug_mux_priv = {
	.ctl_reg = 0x62038,
	.status_reg = 0x6203C,
	.xo_div4_cbcr = 0x28008,
};

static const char *const apcs_debug_mux_parent_names[] = {
	"measure_only_apcs_gold_post_acd_clk",
	"measure_only_apcs_gold_pre_acd_clk",
	"measure_only_apcs_l3_post_acd_clk",
	"measure_only_apcs_l3_pre_acd_clk",
	"measure_only_apcs_silver_post_acd_clk",
	"measure_only_apcs_silver_pre_acd_clk",
};

static int apcs_debug_mux_sels[] = {
	0x25,		/* measure_only_apcs_gold_post_acd_clk */
	0x45,		/* measure_only_apcs_gold_pre_acd_clk */
	0x41,		/* measure_only_apcs_l3_post_acd_clk */
	0x46,		/* measure_only_apcs_l3_pre_acd_clk */
	0x21,		/* measure_only_apcs_silver_post_acd_clk */
	0x44,		/* measure_only_apcs_silver_pre_acd_clk */
};

static int apcs_debug_mux_pre_divs[] = {
	0x8,		/* measure_only_apcs_gold_post_acd_clk */
	0x10,		/* measure_only_apcs_gold_pre_acd_clk */
	0x4,		/* measure_only_apcs_l3_post_acd_clk */
	0x10,		/* measure_only_apcs_l3_pre_acd_clk */
	0x4,		/* measure_only_apcs_silver_post_acd_clk */
	0x10,		/* measure_only_apcs_silver_pre_acd_clk */
};

static struct clk_debug_mux apcs_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x0,
	.post_div_offset = 0x0,
	.cbcr_offset = 0x0,
	.src_sel_mask = 0x7F0,
	.src_sel_shift = 4,
	.post_div_mask = 0x7800,
	.post_div_shift = 11,
	.post_div_val = 1,
	.mux_sels = apcs_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(apcs_debug_mux_sels),
	.pre_div_vals = apcs_debug_mux_pre_divs,
	.hw.init = &(const struct clk_init_data){
		.name = "apcs_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = apcs_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(apcs_debug_mux_parent_names),
	},
};

static const char *const audio_core_cc_debug_mux_parent_names[] = {
	"audio_core_cc_aif_if0_ebit_clk",
	"audio_core_cc_aif_if0_ibit_clk",
	"audio_core_cc_aif_if1_ebit_clk",
	"audio_core_cc_aif_if1_ibit_clk",
	"audio_core_cc_aif_if2_ebit_clk",
	"audio_core_cc_aif_if2_ibit_clk",
	"audio_core_cc_aif_if3_ebit_clk",
	"audio_core_cc_aif_if3_ibit_clk",
	"audio_core_cc_aud_dma_clk",
	"audio_core_cc_aud_dma_mem_clk",
	"audio_core_cc_bus_clk",
	"audio_core_cc_ext_mclka_out_clk",
	"audio_core_cc_ext_mclkb_out_clk",
	"audio_core_cc_im_sleep_clk",
	"audio_core_cc_lpaif_pcmoe_clk",
	"audio_core_cc_rx_mclk_2x_clk",
	"audio_core_cc_rx_mclk_clk",
	"audio_core_cc_sampling_clk",
	"audio_core_cc_tx_mclk_2x_clk",
	"audio_core_cc_tx_mclk_clk",
	"measure_only_audio_core_va2cc_debug_clk"
};

static int audio_core_cc_debug_mux_sels[] = {
	0x5,		/* audio_core_cc_aif_if0_ebit_clk */
	0x4,		/* audio_core_cc_aif_if0_ibit_clk */
	0x7,		/* audio_core_cc_aif_if1_ebit_clk */
	0x6,		/* audio_core_cc_aif_if1_ibit_clk */
	0x9,		/* audio_core_cc_aif_if2_ebit_clk */
	0x8,		/* audio_core_cc_aif_if2_ibit_clk */
	0xB,		/* audio_core_cc_aif_if3_ebit_clk */
	0xA,		/* audio_core_cc_aif_if3_ibit_clk */
	0x2,		/* audio_core_cc_aud_dma_clk */
	0x3,		/* audio_core_cc_aud_dma_mem_clk */
	0x1,		/* audio_core_cc_bus_clk */
	0xC,		/* audio_core_cc_ext_mclka_out_clk */
	0xD,		/* audio_core_cc_ext_mclkb_out_clk */
	0x13,		/* audio_core_cc_im_sleep_clk */
	0x12,		/* audio_core_cc_lpaif_pcmoe_clk */
	0xF,		/* audio_core_cc_rx_mclk_2x_clk */
	0x11,		/* audio_core_cc_rx_mclk_clk */
	0x0,		/* audio_core_cc_sampling_clk */
	0xE,		/* audio_core_cc_tx_mclk_2x_clk */
	0x10,		/* audio_core_cc_tx_mclk_clk */
	0x14,		/* measure_only_audio_core_va2cc_debug_clk */
};

static struct clk_debug_mux audio_core_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x3040,
	.post_div_offset = U32_MAX,
	.cbcr_offset = 0x12D0,
	.src_sel_mask = 0x1F,
	.src_sel_shift = 0,
	.post_div_mask = 0x1,
	.post_div_shift = 0,
	.post_div_val = 1,
	.mux_sels = audio_core_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(audio_core_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "audio_core_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = audio_core_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(audio_core_cc_debug_mux_parent_names),
	},
};

static const char *const disp_cc_debug_mux_parent_names[] = {
	"disp_cc_mdss_ahb_clk",
	"disp_cc_mdss_byte0_clk",
	"disp_cc_mdss_byte0_intf_clk",
	"disp_cc_mdss_esc0_clk",
	"disp_cc_mdss_mdp_clk",
	"disp_cc_mdss_mdp_lut_clk",
	"disp_cc_mdss_non_gdsc_ahb_clk",
	"disp_cc_mdss_pclk0_clk",
	"disp_cc_mdss_vsync_clk",
	"measure_only_disp_cc_sleep_clk",
	"measure_only_disp_cc_xo_clk",
};

static int disp_cc_debug_mux_sels[] = {
	0x1A,		/* disp_cc_mdss_ahb_clk */
	0x11,		/* disp_cc_mdss_byte0_clk */
	0x12,		/* disp_cc_mdss_byte0_intf_clk */
	0x13,		/* disp_cc_mdss_esc0_clk */
	0xE,		/* disp_cc_mdss_mdp_clk */
	0xF,		/* disp_cc_mdss_mdp_lut_clk */
	0x1B,		/* disp_cc_mdss_non_gdsc_ahb_clk */
	0xD,		/* disp_cc_mdss_pclk0_clk */
	0x10,		/* disp_cc_mdss_vsync_clk */
	0x24,		/* measure_only_disp_cc_sleep_clk */
	0x23,		/* measure_only_disp_cc_xo_clk */
};

static struct clk_debug_mux disp_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x7000,
	.post_div_offset = 0x5008,
	.cbcr_offset = 0x500C,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0x3,
	.post_div_shift = 0,
	.post_div_val = 4,
	.mux_sels = disp_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(disp_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "disp_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = disp_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(disp_cc_debug_mux_parent_names),
	},
};

static const char *const gcc_debug_mux_parent_names[] = {
	"apcs_debug_mux",
	"audio_core_cc_debug_mux",
	"disp_cc_debug_mux",
	"gcc_ahb2phy_csi_clk",
	"gcc_ahb2phy_usb_clk",
	"gcc_boot_rom_ahb_clk",
	"gcc_cam_throttle_nrt_clk",
	"gcc_cam_throttle_rt_clk",
	"gcc_camss_axi_clk",
	"gcc_camss_camnoc_atb_clk",
	"gcc_camss_camnoc_dragonlink_atb_clk",
	"gcc_camss_camnoc_nts_xo_clk",
	"gcc_camss_cci_0_clk",
	"gcc_camss_cphy_0_clk",
	"gcc_camss_cphy_1_clk",
	"gcc_camss_csi0phytimer_clk",
	"gcc_camss_csi1phytimer_clk",
	"gcc_camss_mclk0_clk",
	"gcc_camss_mclk1_clk",
	"gcc_camss_mclk2_clk",
	"gcc_camss_mclk3_clk",
	"gcc_camss_nrt_axi_clk",
	"gcc_camss_ope_ahb_clk",
	"gcc_camss_ope_clk",
	"gcc_camss_rt_axi_clk",
	"gcc_camss_tfe_0_clk",
	"gcc_camss_tfe_0_cphy_rx_clk",
	"gcc_camss_tfe_0_csid_clk",
	"gcc_camss_tfe_1_clk",
	"gcc_camss_tfe_1_cphy_rx_clk",
	"gcc_camss_tfe_1_csid_clk",
	"gcc_camss_top_ahb_clk",
	"gcc_cfg_noc_usb2_prim_axi_clk",
	"gcc_cfg_noc_usb3_prim_axi_clk",
	"gcc_ddrss_gpu_axi_clk",
	"gcc_ddrss_memnoc_pcie_sf_clk",
	"gcc_disp_gpll0_div_clk_src",
	"gcc_disp_hf_axi_clk",
	"gcc_disp_throttle_core_clk",
	"gcc_emac0_ahb_clk",
	"gcc_emac0_axi_clk",
	"gcc_emac0_axi_sys_noc_clk",
	"gcc_emac0_cc_sgmiiphy_rx_clk",
	"gcc_emac0_cc_sgmiiphy_rx_clk_src",
	"gcc_emac0_cc_sgmiiphy_tx_clk",
	"gcc_emac0_cc_sgmiiphy_tx_clk_src",
	"gcc_emac0_phy_aux_clk",
	"gcc_emac0_ptp_clk",
	"gcc_emac0_rgmii_clk",
	"gcc_emac1_ahb_clk",
	"gcc_emac1_axi_clk",
	"gcc_emac1_axi_sys_noc_clk",
	"gcc_emac1_cc_sgmiiphy_rx_clk",
	"gcc_emac1_cc_sgmiiphy_rx_clk_src",
	"gcc_emac1_cc_sgmiiphy_tx_clk",
	"gcc_emac1_cc_sgmiiphy_tx_clk_src",
	"gcc_emac1_phy_aux_clk",
	"gcc_emac1_ptp_clk",
	"gcc_emac1_rgmii_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_gpu_gpll0_clk_src",
	"gcc_gpu_gpll0_div_clk_src",
	"gcc_gpu_memnoc_gfx_clk",
	"gcc_gpu_snoc_dvm_gfx_clk",
	"gcc_gpu_throttle_core_clk",
	"gcc_lpass_config_clk",
	"gcc_lpass_core_axim_clk",
	"gcc_pcie_aux_clk",
	"gcc_pcie_aux_clk_src",
	"gcc_pcie_cfg_ahb_clk",
	"gcc_pcie_mstr_axi_clk",
	"gcc_pcie_pipe_clk",
	"gcc_pcie_pipe_clk_src",
	"gcc_pcie_rchng_phy_clk",
	"gcc_pcie_sleep_clk",
	"gcc_pcie_slv_axi_clk",
	"gcc_pcie_slv_q2a_axi_clk",
	"gcc_pcie_tbu_clk",
	"gcc_pcie_throttle_core_clk",
	"gcc_pcie_throttle_xo_clk",
	"gcc_pcie_tile_axi_sys_noc_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_pdm_xo4_clk",
	"gcc_pwm0_xo512_clk",
	"gcc_qmip_camera_nrt_ahb_clk",
	"gcc_qmip_camera_rt_ahb_clk",
	"gcc_qmip_disp_ahb_clk",
	"gcc_qmip_gpu_cfg_ahb_clk",
	"gcc_qmip_pcie_cfg_ahb_clk",
	"gcc_qmip_video_vcodec_ahb_clk",
	"gcc_qupv3_wrap0_core_2x_clk",
	"gcc_qupv3_wrap0_core_clk",
	"gcc_qupv3_wrap0_s0_clk",
	"gcc_qupv3_wrap0_s1_clk",
	"gcc_qupv3_wrap0_s2_clk",
	"gcc_qupv3_wrap0_s3_clk",
	"gcc_qupv3_wrap0_s4_clk",
	"gcc_qupv3_wrap0_s5_clk",
	"gcc_qupv3_wrap0_s6_clk",
	"gcc_qupv3_wrap0_s7_clk",
	"gcc_qupv3_wrap0_s8_clk",
	"gcc_qupv3_wrap0_s9_clk",
	"gcc_qupv3_wrap_0_m_ahb_clk",
	"gcc_qupv3_wrap_0_s_ahb_clk",
	"gcc_sdcc1_ahb_clk",
	"gcc_sdcc1_apps_clk",
	"gcc_sdcc1_ice_core_clk",
	"gcc_sdcc2_ahb_clk",
	"gcc_sdcc2_apps_clk",
	"gcc_sys_noc_turing_axi_clk",
	"gcc_sys_noc_usb2_prim_axi_clk",
	"gcc_sys_noc_usb3_prim_axi_clk",
	"gcc_tscss_ahb_clk",
	"gcc_tscss_cntr_clk",
	"gcc_tscss_etu_clk",
	"gcc_usb20_master_clk",
	"gcc_usb20_mock_utmi_clk",
	"gcc_usb20_sleep_clk",
	"gcc_usb30_prim_master_clk",
	"gcc_usb30_prim_mock_utmi_clk",
	"gcc_usb30_prim_sleep_clk",
	"gcc_usb3_prim_phy_com_aux_clk",
	"gcc_usb3_prim_phy_pipe_clk",
	"gcc_usb3_prim_phy_pipe_clk_src",
	"gcc_vcodec0_axi_clk",
	"gcc_venus_ahb_clk",
	"gcc_venus_ctl_axi_clk",
	"gcc_video_axi0_clk",
	"gcc_video_throttle_core_clk",
	"gcc_video_vcodec0_sys_clk",
	"gcc_video_venus_ctl_clk",
	"gpu_cc_debug_mux",
	"mc_cc_debug_mux",
	"measure_only_cnoc_clk",
	"measure_only_emac0_sgmiiphy_rclk",
	"measure_only_emac0_sgmiiphy_tclk",
	"measure_only_emac1_sgmiiphy_rclk",
	"measure_only_emac1_sgmiiphy_tclk",
	"measure_only_gcc_camera_ahb_clk",
	"measure_only_gcc_camera_xo_clk",
	"measure_only_gcc_cpuss_gnoc_clk",
	"measure_only_gcc_disp_ahb_clk",
	"measure_only_gcc_disp_xo_clk",
	"measure_only_gcc_gpu_cfg_ahb_clk",
	"measure_only_gcc_sys_noc_cpuss_ahb_clk",
	"measure_only_gcc_video_ahb_clk",
	"measure_only_gcc_video_xo_clk",
	"measure_only_hwkm_ahb_clk",
	"measure_only_ipa_2x_clk",
	"measure_only_pka_ahb_clk",
	"measure_only_pka_core_clk",
	"measure_only_qpic_clk",
	"measure_only_qpic_ahb_clk",
	"measure_only_pcie_pipe_clk",
	"measure_only_snoc_clk",
	"measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk",
};

static int gcc_debug_mux_sels[] = {
	0xDB,		/* apcs_debug_mux */
	0x12D,		/* lpass_gcc_debug_mux */
	0x60,		/* disp_cc_debug_mux */
	0x86,		/* gcc_ahb2phy_csi_clk */
	0x87,		/* gcc_ahb2phy_usb_clk */
	0x9D,		/* gcc_boot_rom_ahb_clk */
	0x6A,		/* gcc_cam_throttle_nrt_clk */
	0x69,		/* gcc_cam_throttle_rt_clk */
	0x15B,		/* gcc_camss_axi_clk */
	0x15D,		/* gcc_camss_camnoc_atb_clk */
	0x162,		/* gcc_camss_camnoc_dragonlink_atb_clk */
	0x15E,		/* gcc_camss_camnoc_nts_xo_clk */
	0x159,		/* gcc_camss_cci_0_clk */
	0x150,		/* gcc_camss_cphy_0_clk */
	0x151,		/* gcc_camss_cphy_1_clk */
	0x144,		/* gcc_camss_csi0phytimer_clk */
	0x145,		/* gcc_camss_csi1phytimer_clk */
	0x146,		/* gcc_camss_mclk0_clk */
	0x147,		/* gcc_camss_mclk1_clk */
	0x148,		/* gcc_camss_mclk2_clk */
	0x149,		/* gcc_camss_mclk3_clk */
	0x15F,		/* gcc_camss_nrt_axi_clk */
	0x158,		/* gcc_camss_ope_ahb_clk */
	0x156,		/* gcc_camss_ope_clk */
	0x161,		/* gcc_camss_rt_axi_clk */
	0x14A,		/* gcc_camss_tfe_0_clk */
	0x14E,		/* gcc_camss_tfe_0_cphy_rx_clk */
	0x152,		/* gcc_camss_tfe_0_csid_clk */
	0x14C,		/* gcc_camss_tfe_1_clk */
	0x14F,		/* gcc_camss_tfe_1_cphy_rx_clk */
	0x154,		/* gcc_camss_tfe_1_csid_clk */
	0x15A,		/* gcc_camss_top_ahb_clk */
	0x43,		/* gcc_cfg_noc_usb2_prim_axi_clk */
	0x36,		/* gcc_cfg_noc_usb3_prim_axi_clk */
	0xB5,		/* gcc_ddrss_gpu_axi_clk */
	0xC7,		/* gcc_ddrss_memnoc_pcie_sf_clk */
	0x65,		/* gcc_disp_gpll0_div_clk_src */
	0x5B,		/* gcc_disp_hf_axi_clk */
	0x67,		/* gcc_disp_throttle_core_clk */
	0x18B,		/* gcc_emac0_ahb_clk */
	0x18C,		/* gcc_emac0_axi_clk */
	0x1C,		/* gcc_emac1_axi_sys_noc_clk */
	0x192,		/* gcc_emac0_cc_sgmiiphy_rx_clk */
	0x193,		/* gcc_emac0_cc_sgmiiphy_rx_clk_src */
	0x190,		/* gcc_emac0_cc_sgmiiphy_tx_clk */
	0x191,		/* gcc_emac0_cc_sgmiiphy_tx_clk_src */
	0x18D,		/* gcc_emac0_phy_aux_clk */
	0x18E,		/* gcc_emac0_ptp_clk */
	0x18F,		/* gcc_emac0_rgmii_clk */
	0x198,		/* gcc_emac1_ahb_clk */
	0x199,		/* gcc_emac1_axi_clk */
	0x1D,		/* gcc_emac1_axi_sys_noc_clk */
	0x19F,		/* gcc_emac1_cc_sgmiiphy_rx_clk */
	0x1A0,		/* gcc_emac1_cc_sgmiiphy_rx_clk_src */
	0x19D,		/* gcc_emac1_cc_sgmiiphy_tx_clk */
	0x19E,		/* gcc_emac1_cc_sgmiiphy_tx_clk_src */
	0x19A,		/* gcc_emac1_phy_aux_clk */
	0x19B,		/* gcc_emac1_ptp_clk */
	0x19C,		/* gcc_emac1_rgmii_clk */
	0xE6,		/* gcc_gp1_clk */
	0xE7,		/* gcc_gp2_clk */
	0xE8,		/* gcc_gp3_clk */
	0x11F,		/* gcc_gpu_gpll0_clk_src */
	0x120,		/* gcc_gpu_gpll0_div_clk_src */
	0x11C,		/* gcc_gpu_memnoc_gfx_clk */
	0x11E,		/* gcc_gpu_snoc_dvm_gfx_clk */
	0x123,		/* gcc_gpu_throttle_core_clk */
	0x12B,		/* gcc_lpass_config_clk */
	0x12A,		/* gcc_lpass_core_axim_clk */
	0x1AA,		/* gcc_pcie_aux_clk */
	0x1AE,		/* gcc_pcie_aux_clk_src */
	0x1A5,		/* gcc_pcie_cfg_ahb_clk */
	0x1A8,		/* gcc_pcie_mstr_axi_clk */
	0x1AC,		/* gcc_pcie_pipe_clk */
	0x1AD,		/* gcc_pcie_pipe_clk_src */
	0x1A9,		/* gcc_pcie_rchng_phy_clk */
	0x1AB,		/* gcc_pcie_sleep_clk */
	0x1A7,		/* gcc_pcie_slv_axi_clk */
	0x1A6,		/* gcc_pcie_slv_q2a_axi_clk */
	0x1B4,		/* gcc_pcie_tbu_clk */
	0x1B3,		/* gcc_pcie_throttle_core_clk */
	0x1B2,		/* gcc_pcie_throttle_xo_clk */
	0x32,		/* gcc_pcie_tile_axi_sys_noc_clk */
	0x9A,		/* gcc_pdm2_clk */
	0x98,		/* gcc_pdm_ahb_clk */
	0x99,		/* gcc_pdm_xo4_clk */
	0x9B,		/* gcc_pwm0_xo512_clk */
	0x58,		/* gcc_qmip_camera_nrt_ahb_clk */
	0x66,		/* gcc_qmip_camera_rt_ahb_clk */
	0x59,		/* gcc_qmip_disp_ahb_clk */
	0x121,		/* gcc_qmip_gpu_cfg_ahb_clk */
	0x1B1,		/* gcc_qmip_pcie_cfg_ahb_clk */
	0x57,		/* gcc_qmip_video_vcodec_ahb_clk */
	0x8D,		/* gcc_qupv3_wrap0_core_2x_clk */
	0x8C,		/* gcc_qupv3_wrap0_core_clk */
	0x8E,		/* gcc_qupv3_wrap0_s0_clk */
	0x8F,		/* gcc_qupv3_wrap0_s1_clk */
	0x90,		/* gcc_qupv3_wrap0_s2_clk */
	0x91,		/* gcc_qupv3_wrap0_s3_clk */
	0x92,		/* gcc_qupv3_wrap0_s4_clk */
	0x93,		/* gcc_qupv3_wrap0_s5_clk */
	0x94,		/* gcc_qupv3_wrap0_s6_clk */
	0x95,		/* gcc_qupv3_wrap0_s7_clk */
	0x96,		/* gcc_qupv3_wrap0_s8_clk */
	0x97,		/* gcc_qupv3_wrap0_s9_clk */
	0x8A,		/* gcc_qupv3_wrap_0_m_ahb_clk */
	0x8B,		/* gcc_qupv3_wrap_0_s_ahb_clk */
	0x127,		/* gcc_sdcc1_ahb_clk */
	0x126,		/* gcc_sdcc1_apps_clk */
	0x128,		/* gcc_sdcc1_ice_core_clk */
	0x89,		/* gcc_sdcc2_ahb_clk */
	0x88,		/* gcc_sdcc2_apps_clk */
	0x20,		/* gcc_sys_noc_turing_axi_clk */
	0x1E,		/* gcc_sys_noc_usb2_prim_axi_clk */
	0x18,		/* gcc_sys_noc_usb3_prim_axi_clk */
	0x18A,		/* gcc_tscss_ahb_clk */
	0x189,		/* gcc_tscss_cntr_clk */
	0x188,		/* gcc_tscss_etu_clk */
	0x1B6,		/* gcc_usb20_master_clk */
	0x1B8,		/* gcc_usb20_mock_utmi_clk */
	0x1B7,		/* gcc_usb20_sleep_clk */
	0x7E,		/* gcc_usb30_prim_master_clk */
	0x80,		/* gcc_usb30_prim_mock_utmi_clk */
	0x7F,		/* gcc_usb30_prim_sleep_clk */
	0x81,		/* gcc_usb3_prim_phy_com_aux_clk */
	0x82,		/* gcc_usb3_prim_phy_pipe_clk */
	0x83,		/* gcc_usb3_prim_phy_pipe_clk_src */
	0x168,		/* gcc_vcodec0_axi_clk */
	0x169,		/* gcc_venus_ahb_clk */
	0x167,		/* gcc_venus_ctl_axi_clk */
	0x5A,		/* gcc_video_axi0_clk */
	0x68,		/* gcc_video_throttle_core_clk */
	0x165,		/* gcc_video_vcodec0_sys_clk */
	0x163,		/* gcc_video_venus_ctl_clk */
	0x11B,		/* gpu_cc_debug_mux */
	0xC6,		/* mc_cc_debug_mux or ddrss_gcc_debug_clk */
	0x33,		/* measure_only_cnoc_clk */
	0x195,		/* measure_only_emac0_sgmiiphy_rclk */
	0x194,		/* measure_only_emac0_sgmiiphy_tclk */
	0x1A2,		/* measure_only_emac1_sgmiiphy_rclk */
	0x1A1,		/* measure_only_emac1_sgmiiphy_tclk */
	0x55,		/* measure_only_gcc_camera_ahb_clk */
	0x5D,		/* measure_only_gcc_camera_xo_clk */
	0xD6,		/* measure_only_gcc_cpuss_gnoc_clk */
	0x56,		/* measure_only_gcc_disp_ahb_clk */
	0x5E,		/* measure_only_gcc_disp_xo_clk */
	0x119,		/* measure_only_gcc_gpu_cfg_ahb_clk */
	0x9,		/* measure_only_gcc_sys_noc_cpuss_ahb_clk */
	0x54,		/* measure_only_gcc_video_ahb_clk */
	0x5C,		/* measure_only_gcc_video_xo_clk */
	0xD2,		/* measure_only_hwkm_ahb_clk */
	0xF9,		/* measure_only_ipa_2x_clk */
	0x1AF,		/* measure_only_pcie_pipe_clk */
	0xD4,		/* measure_only_pka_ahb_clk */
	0xD3,		/* measure_only_pka_core_clk */
	0xCC,		/* measure_only_qpic_clk */
	0xCE,		/* measure_only_qpic_ahb_clk */
	0x7,		/* measure_only_snoc_clk */
	0x84,		/* measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk */
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x62000,
	.post_div_offset = 0x30000,
	.cbcr_offset = 0x30004,
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 1,
	.mux_sels = gcc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(gcc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "gcc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gcc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gcc_debug_mux_parent_names),
	},
};

static const char *const gpu_cc_debug_mux_parent_names[] = {
	"gpu_cc_crc_ahb_clk",
	"gpu_cc_cx_gfx3d_clk",
	"gpu_cc_cx_gfx3d_slv_clk",
	"gpu_cc_cx_gmu_clk",
	"gpu_cc_cx_snoc_dvm_clk",
	"gpu_cc_cxo_clk",
	"gpu_cc_gx_gfx3d_clk",
	"gpu_cc_sleep_clk",
	"measure_only_gpu_cc_ahb_clk",
	"measure_only_gpu_cc_cxo_aon_clk",
	"measure_only_gpu_cc_gx_cxo_clk",
};

static int gpu_cc_debug_mux_sels[] = {
	0x11,		/* gpu_cc_crc_ahb_clk */
	0x1A,		/* gpu_cc_cx_gfx3d_clk */
	0x1B,		/* gpu_cc_cx_gfx3d_slv_clk */
	0x18,		/* gpu_cc_cx_gmu_clk */
	0x15,		/* gpu_cc_cx_snoc_dvm_clk */
	0x19,		/* gpu_cc_cxo_clk */
	0xB,		/* gpu_cc_gx_gfx3d_clk */
	0x16,		/* gpu_cc_sleep_clk */
	0x10,		/* measure_only_gpu_cc_ahb_clk */
	0xA,		/* measure_only_gpu_cc_cxo_aon_clk */
	0xE,		/* measure_only_gpu_cc_gx_cxo_clk */
};

static struct clk_debug_mux gpu_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x1568,
	.post_div_offset = 0x10FC,
	.cbcr_offset = 0x1100,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0x3,
	.post_div_shift = 0,
	.post_div_val = 2,
	.mux_sels = gpu_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(gpu_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "gpu_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = gpu_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(gpu_cc_debug_mux_parent_names),
	},
};

static const char *const mc_cc_debug_mux_parent_names[] = {
	"measure_only_mccc_clk",
};

static struct clk_debug_mux mc_cc_debug_mux = {
	.period_offset = 0x50,
	.hw.init = &(struct clk_init_data){
		.name = "mc_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = mc_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(mc_cc_debug_mux_parent_names),
	},
};

static struct mux_regmap_names mux_list[] = {
	{ .mux = &mc_cc_debug_mux, .regmap_name = "qcom,mccc" },
	{ .mux = &gpu_cc_debug_mux, .regmap_name = "qcom,gpucc" },
	{ .mux = &disp_cc_debug_mux, .regmap_name = "qcom,dispcc" },
	{ .mux = &apcs_debug_mux, .regmap_name = "qcom,cpucc" },
	{ .mux = &audio_core_cc_debug_mux, .regmap_name = "qcom,audiocorecc" },
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
};

static struct clk_dummy measure_only_apcs_gold_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_gold_post_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_gold_pre_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_gold_pre_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_l3_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_l3_post_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_l3_pre_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_l3_pre_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_silver_post_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_silver_post_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_apcs_silver_pre_acd_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_apcs_silver_pre_acd_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_audio_core_va2cc_debug_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_audio_core_va2cc_debug_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_cnoc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_cnoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_disp_cc_sleep_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_disp_cc_sleep_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_disp_cc_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_disp_cc_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_emac0_sgmiiphy_rclk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_emac0_sgmiiphy_rclk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_emac0_sgmiiphy_tclk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_emac0_sgmiiphy_tclk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_emac1_sgmiiphy_rclk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_emac1_sgmiiphy_rclk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_emac1_sgmiiphy_tclk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_emac1_sgmiiphy_tclk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_camera_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_camera_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_camera_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_camera_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_cpuss_gnoc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_cpuss_gnoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_disp_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_disp_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_disp_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_disp_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_gpu_cfg_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_gpu_cfg_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_sys_noc_cpuss_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_sys_noc_cpuss_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_video_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_video_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_video_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_video_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gpu_cc_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gpu_cc_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gpu_cc_cxo_aon_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gpu_cc_cxo_aon_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gpu_cc_gx_cxo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gpu_cc_gx_cxo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_hwkm_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_hwkm_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_ipa_2x_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_ipa_2x_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pcie_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_pcie_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_qpic_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_qpic_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_qpic_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_qpic_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_mccc_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_mccc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pka_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_pka_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pka_core_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_pka_core_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_snoc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_snoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(struct clk_init_data){
		.name = "measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_hw *debugcc_shikra_hws[] = {
	&measure_only_apcs_gold_post_acd_clk.hw,
	&measure_only_apcs_gold_pre_acd_clk.hw,
	&measure_only_apcs_l3_post_acd_clk.hw,
	&measure_only_apcs_l3_pre_acd_clk.hw,
	&measure_only_apcs_silver_post_acd_clk.hw,
	&measure_only_apcs_silver_pre_acd_clk.hw,
	&measure_only_audio_core_va2cc_debug_clk.hw,
	&measure_only_cnoc_clk.hw,
	&measure_only_disp_cc_sleep_clk.hw,
	&measure_only_disp_cc_xo_clk.hw,
	&measure_only_emac0_sgmiiphy_rclk.hw,
	&measure_only_emac0_sgmiiphy_tclk.hw,
	&measure_only_emac1_sgmiiphy_rclk.hw,
	&measure_only_emac1_sgmiiphy_tclk.hw,
	&measure_only_gcc_camera_ahb_clk.hw,
	&measure_only_gcc_camera_xo_clk.hw,
	&measure_only_gcc_cpuss_gnoc_clk.hw,
	&measure_only_gcc_disp_ahb_clk.hw,
	&measure_only_gcc_disp_xo_clk.hw,
	&measure_only_gcc_gpu_cfg_ahb_clk.hw,
	&measure_only_gcc_sys_noc_cpuss_ahb_clk.hw,
	&measure_only_gcc_video_ahb_clk.hw,
	&measure_only_gcc_video_xo_clk.hw,
	&measure_only_gpu_cc_ahb_clk.hw,
	&measure_only_gpu_cc_cxo_aon_clk.hw,
	&measure_only_gpu_cc_gx_cxo_clk.hw,
	&measure_only_hwkm_ahb_clk.hw,
	&measure_only_ipa_2x_clk.hw,
	&measure_only_mccc_clk.hw,
	&measure_only_pcie_pipe_clk.hw,
	&measure_only_pka_ahb_clk.hw,
	&measure_only_pka_core_clk.hw,
	&measure_only_qpic_clk.hw,
	&measure_only_qpic_ahb_clk.hw,
	&measure_only_snoc_clk.hw,
	&measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,shikra-debugcc" },
	{ }
};

static int clk_debug_shikra_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret = 0, i;

	BUILD_BUG_ON(ARRAY_SIZE(apcs_debug_mux_parent_names) != ARRAY_SIZE(apcs_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(audio_core_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(audio_core_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(disp_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(disp_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gcc_debug_mux_parent_names) != ARRAY_SIZE(gcc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gpu_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(gpu_cc_debug_mux_sels));

	clk = devm_clk_get(&pdev->dev, "xo_clk_src");
	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Unable to get xo clock\n");
		return PTR_ERR(clk);
	}

	debug_mux_priv.cxo = clk;

	for (i = 0; i < ARRAY_SIZE(mux_list); i++) {
		if (IS_ERR_OR_NULL(mux_list[i].mux->regmap)) {
			ret = map_debug_bases(pdev, mux_list[i].regmap_name,
					      mux_list[i].mux);
			if (ret == -EBADR)
				continue;
			else if (ret)
				return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(debugcc_shikra_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_shikra_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%ld)\n",
				qcom_clk_hw_get_name(debugcc_shikra_hws[i]),
				PTR_ERR(clk));
			return PTR_ERR(clk);
		}
	}

	for (i = 0; i < ARRAY_SIZE(mux_list); i++) {
		ret = devm_clk_register_debug_mux(&pdev->dev, mux_list[i].mux);
		if (ret) {
			dev_err(&pdev->dev, "Unable to register mux clk %s, err:(%d)\n",
				qcom_clk_hw_get_name(&mux_list[i].mux->hw),
				ret);
			return ret;
		}
	}

	ret = clk_debug_measure_register(&gcc_debug_mux.hw);
	if (ret) {
		dev_err(&pdev->dev, "Could not register Measure clocks\n");
		return ret;
	}

	dev_info(&pdev->dev, "Registered debug measure clocks\n");

	return ret;
}

static struct platform_driver clk_debug_driver = {
	.probe = clk_debug_shikra_probe,
	.driver = {
		.name = "shikra-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_shikra_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_shikra_init);

MODULE_DESCRIPTION("QTI DEBUG CC SHIKRA Driver");
MODULE_LICENSE("GPL");
