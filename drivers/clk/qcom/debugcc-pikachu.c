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
	.ctl_reg = 0x62048,
	.status_reg = 0x6204C,
	.xo_div4_cbcr = 0x62008,
};

static const char *const apss_cc_debug_mux_parent_names[] = {
	"measure_only_apcs_l3_post_acd_clk",
	"measure_only_apcs_l3_pre_acd_clk",
	"measure_only_apcs_silver_post_acd_clk",
	"measure_only_apcs_silver_pre_acd_clk",
};

static int apss_cc_debug_mux_sels[] = {
	0x41,		/* measure_only_apcs_l3_post_acd_clk */
	0x46,		/* measure_only_apcs_l3_pre_acd_clk */
	0x21,		/* measure_only_apcs_silver_post_acd_clk */
	0x44,		/* measure_only_apcs_silver_pre_acd_clk */
};

static int apss_cc_debug_mux_pre_divs[] = {
	0x4,		/* measure_only_apcs_l3_post_acd_clk */
	0x10,		/* measure_only_apcs_l3_pre_acd_clk */
	0x4,		/* measure_only_apcs_silver_post_acd_clk */
	0x10,		/* measure_only_apcs_silver_pre_acd_clk */
};

static struct clk_debug_mux apss_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x18,
	.post_div_offset = 0x18,
	.cbcr_offset = 0x0,
	.src_sel_mask = 0x7F0,
	.src_sel_shift = 4,
	.post_div_mask = 0x7800,
	.post_div_shift = 11,
	.post_div_val = 1,
	.mux_sels = apss_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(apss_cc_debug_mux_sels),
	.pre_div_vals = apss_cc_debug_mux_pre_divs,
	.hw.init = &(const struct clk_init_data){
		.name = "apss_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = apss_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(apss_cc_debug_mux_parent_names),
	},
};

static const char *const cam_cc_debug_mux_parent_names[] = {
	"cam_cc_bps_ahb_clk",
	"cam_cc_bps_clk",
	"cam_cc_bps_fast_ahb_clk",
	"cam_cc_camnoc_ahb_clk",
	"cam_cc_camnoc_axi_nrt_clk",
	"cam_cc_camnoc_axi_rt_clk",
	"cam_cc_camnoc_dcd_xo_clk",
	"cam_cc_camnoc_xo_clk",
	"cam_cc_cci_0_clk",
	"cam_cc_cci_1_clk",
	"cam_cc_core_ahb_clk",
	"cam_cc_cpas_ahb_clk",
	"cam_cc_cpas_bps_clk",
	"cam_cc_cpas_fast_ahb_clk",
	"cam_cc_cpas_ife_0_clk",
	"cam_cc_cpas_ife_lite_0_clk",
	"cam_cc_cpas_ipe_nps_clk",
	"cam_cc_csi2phytimer_clk",
	"cam_cc_csi4phytimer_clk",
	"cam_cc_csid_clk",
	"cam_cc_csid_csiphy_rx_clk",
	"cam_cc_csiphy2_clk",
	"cam_cc_csiphy4_clk",
	"cam_cc_drv_ahb_clk",
	"cam_cc_drv_xo_clk",
	"cam_cc_icp_ahb_clk",
	"cam_cc_icp_clk",
	"cam_cc_ife_0_clk",
	"cam_cc_ife_0_fast_ahb_clk",
	"cam_cc_ife_lite_0_ahb_clk",
	"cam_cc_ife_lite_0_clk",
	"cam_cc_ife_lite_0_cphy_rx_clk",
	"cam_cc_ife_lite_0_csid_clk",
	"cam_cc_ipe_nps_ahb_clk",
	"cam_cc_ipe_nps_clk",
	"cam_cc_ipe_nps_fast_ahb_clk",
	"cam_cc_ipe_pps_clk",
	"cam_cc_ipe_pps_fast_ahb_clk",
	"cam_cc_jpeg_clk",
	"cam_cc_mclk0_clk",
	"cam_cc_mclk1_clk",
	"cam_cc_mclk2_clk",
	"cam_cc_mclk4_clk",
	"cam_cc_qdss_debug_clk",
	"cam_cc_qdss_debug_xo_clk",
	"measure_only_cam_cc_gdsc_clk",
	"measure_only_cam_cc_sleep_clk",
};

static int cam_cc_debug_mux_sels[] = {
	0x17,		/* cam_cc_bps_ahb_clk */
	0x18,		/* cam_cc_bps_clk */
	0x16,		/* cam_cc_bps_fast_ahb_clk */
	0x78,		/* cam_cc_camnoc_ahb_clk */
	0x57,		/* cam_cc_camnoc_axi_nrt_clk */
	0x49,		/* cam_cc_camnoc_axi_rt_clk */
	0x4A,		/* cam_cc_camnoc_dcd_xo_clk */
	0x60,		/* cam_cc_camnoc_xo_clk */
	0x44,		/* cam_cc_cci_0_clk */
	0x45,		/* cam_cc_cci_1_clk */
	0x4D,		/* cam_cc_core_ahb_clk */
	0x46,		/* cam_cc_cpas_ahb_clk */
	0x19,		/* cam_cc_cpas_bps_clk */
	0x47,		/* cam_cc_cpas_fast_ahb_clk */
	0x25,		/* cam_cc_cpas_ife_0_clk */
	0x34,		/* cam_cc_cpas_ife_lite_0_clk */
	0x1B,		/* cam_cc_cpas_ipe_nps_clk */
	0xE,		/* cam_cc_csi2phytimer_clk */
	0x10,		/* cam_cc_csi4phytimer_clk */
	0x48,		/* cam_cc_csid_clk */
	0xB,		/* cam_cc_csid_csiphy_rx_clk */
	0xF,		/* cam_cc_csiphy2_clk */
	0x11,		/* cam_cc_csiphy4_clk */
	0x79,		/* cam_cc_drv_ahb_clk */
	0x74,		/* cam_cc_drv_xo_clk */
	0x43,		/* cam_cc_icp_ahb_clk */
	0x42,		/* cam_cc_icp_clk */
	0x24,		/* cam_cc_ife_0_clk */
	0x28,		/* cam_cc_ife_0_fast_ahb_clk */
	0x37,		/* cam_cc_ife_lite_0_ahb_clk */
	0x33,		/* cam_cc_ife_lite_0_clk */
	0x36,		/* cam_cc_ife_lite_0_cphy_rx_clk */
	0x35,		/* cam_cc_ife_lite_0_csid_clk */
	0x1E,		/* cam_cc_ipe_nps_ahb_clk */
	0x1A,		/* cam_cc_ipe_nps_clk */
	0x1F,		/* cam_cc_ipe_nps_fast_ahb_clk */
	0x1C,		/* cam_cc_ipe_pps_clk */
	0x20,		/* cam_cc_ipe_pps_fast_ahb_clk */
	0x40,		/* cam_cc_jpeg_clk */
	0x1,		/* cam_cc_mclk0_clk */
	0x2,		/* cam_cc_mclk1_clk */
	0x3,		/* cam_cc_mclk2_clk */
	0x4,		/* cam_cc_mclk4_clk */
	0x4B,		/* cam_cc_qdss_debug_clk */
	0x4C,		/* cam_cc_qdss_debug_xo_clk */
	0x4E,		/* measure_only_cam_cc_gdsc_clk */
	0x4F,		/* measure_only_cam_cc_sleep_clk */
};

static struct clk_debug_mux cam_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x16000,
	.post_div_offset = 0x1408C,
	.cbcr_offset = 0x14090,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 4,
	.mux_sels = cam_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(cam_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "cam_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = cam_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(cam_cc_debug_mux_parent_names),
	},
};

static const char *const eva_cc_debug_mux_parent_names[] = {
	"eva_cc_mvs0_clk",
	"eva_cc_mvs0_freerun_clk",
	"eva_cc_mvs0_shift_clk",
	"eva_cc_mvs0c_clk",
	"eva_cc_mvs0c_freerun_clk",
	"eva_cc_mvs0c_shift_clk",
	"measure_only_eva_cc_ahb_clk",
	"measure_only_eva_cc_sleep_clk",
	"measure_only_eva_cc_xo_clk",
};

static int eva_cc_debug_mux_sels[] = {
	0x4,		/* eva_cc_mvs0_clk */
	0x5,		/* eva_cc_mvs0_freerun_clk */
	0xA,		/* eva_cc_mvs0_shift_clk */
	0x1,		/* eva_cc_mvs0c_clk */
	0x2,		/* eva_cc_mvs0c_freerun_clk */
	0xB,		/* eva_cc_mvs0c_shift_clk */
	0x8,		/* measure_only_eva_cc_ahb_clk */
	0xC,		/* measure_only_eva_cc_sleep_clk */
	0x9,		/* measure_only_eva_cc_xo_clk */
};

static struct clk_debug_mux eva_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x9A4C,
	.post_div_offset = 0x80A8,
	.cbcr_offset = 0x80AC,
	.src_sel_mask = 0x3F,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 3,
	.mux_sels = eva_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(eva_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "eva_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = eva_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(eva_cc_debug_mux_parent_names),
	},
};

static const char *const gcc_debug_mux_parent_names[] = {
	"apss_cc_debug_mux",
	"cam_cc_debug_mux",
	"eva_cc_debug_mux",
	"gcc_aggre_noc_pcie_axi_clk",
	"gcc_aggre_ufs_phy_axi_clk",
	"gcc_aggre_usb3_prim_axi_clk",
	"gcc_boot_rom_ahb_clk",
	"gcc_camera_cti_clk",
	"gcc_camera_hf_axi_clk",
	"gcc_camera_sf_axi_clk",
	"gcc_camera_tsctr_clk",
	"gcc_cfg_noc_pcie_anoc_ahb_clk",
	"gcc_cfg_noc_usb3_prim_axi_clk",
	"gcc_cnoc_pcie_sf_axi_clk",
	"gcc_ddrss_pcie_sf_qtb_clk",
	"gcc_disp_0_hf_axi_clk",
	"gcc_disp_sf_axi_clk",
	"gcc_disp_tsctr_clk",
	"gcc_eva_axi0_clk",
	"gcc_eva_axi0c_clk",
	"gcc_gp10_clk",
	"gcc_gp11_clk",
	"gcc_gp1_clk",
	"gcc_gp2_clk",
	"gcc_gp3_clk",
	"gcc_gp4_clk",
	"gcc_gp5_clk",
	"gcc_gp6_clk",
	"gcc_gp7_clk",
	"gcc_gp8_clk",
	"gcc_gp9_clk",
	"gcc_gpu_gpll0_clk_src",
	"gcc_gpu_gpll0_div_clk_src",
	"gcc_gpu_memnoc_gfx_clk",
	"gcc_pcie_0_aux_clk",
	"gcc_pcie_0_cfg_ahb_clk",
	"gcc_pcie_0_mstr_axi_clk",
	"gcc_pcie_0_phy_rchng_clk",
	"gcc_pcie_0_pipe_clk",
	"gcc_pcie_0_pipe_div2_clk",
	"gcc_pcie_0_slv_axi_clk",
	"gcc_pcie_0_slv_q2a_axi_clk",
	"gcc_pcie_1_aux_clk",
	"gcc_pcie_1_cfg_ahb_clk",
	"gcc_pcie_1_mstr_axi_clk",
	"gcc_pcie_1_phy_rchng_clk",
	"gcc_pcie_1_pipe_clk",
	"gcc_pcie_1_pipe_div2_clk",
	"gcc_pcie_1_slv_axi_clk",
	"gcc_pcie_1_slv_q2a_axi_clk",
	"gcc_pdm2_clk",
	"gcc_pdm_ahb_clk",
	"gcc_pdm_xo4_clk",
	"gcc_pwm0_xo512_clk",
	"gcc_pwm1_xo512_clk",
	"gcc_pwm2_xo512_clk",
	"gcc_pwm3_xo512_clk",
	"gcc_pwm4_xo512_clk",
	"gcc_pwm5_xo512_clk",
	"gcc_pwm6_xo512_clk",
	"gcc_pwm7_xo512_clk",
	"gcc_pwm8_xo512_clk",
	"gcc_pwm9_xo512_clk",
	"gcc_qmip_camera_icp_ahb_clk",
	"gcc_qmip_camera_nrt_ahb_clk",
	"gcc_qmip_camera_rt_ahb_clk",
	"gcc_qmip_disp_ahb_clk",
	"gcc_qmip_gpu_ahb_clk",
	"gcc_qmip_pcie_ahb_clk",
	"gcc_qmip_video_cv_cpu_ahb_clk",
	"gcc_qmip_video_cvp_ahb_clk",
	"gcc_qmip_video_v_cpu_ahb_clk",
	"gcc_qmip_video_vcodec_ahb_clk",
	"gcc_qupv3_wrap0_core_2x_clk",
	"gcc_qupv3_wrap0_core_clk",
	"gcc_qupv3_wrap0_s0_clk",
	"gcc_qupv3_wrap0_s1_clk",
	"gcc_qupv3_wrap0_s2_clk",
	"gcc_qupv3_wrap0_s3_clk",
	"gcc_qupv3_wrap0_s4_clk",
	"gcc_qupv3_wrap0_s5_clk",
	"gcc_qupv3_wrap1_core_2x_clk",
	"gcc_qupv3_wrap1_core_clk",
	"gcc_qupv3_wrap1_s0_clk",
	"gcc_qupv3_wrap1_s1_clk",
	"gcc_qupv3_wrap1_s2_clk",
	"gcc_qupv3_wrap1_s3_clk",
	"gcc_qupv3_wrap1_s4_clk",
	"gcc_qupv3_wrap1_s5_clk",
	"gcc_qupv3_wrap_0_m_ahb_clk",
	"gcc_qupv3_wrap_0_s_ahb_clk",
	"gcc_qupv3_wrap_1_m_ahb_clk",
	"gcc_qupv3_wrap_1_s_ahb_clk",
	"gcc_sdcc1_ahb_clk",
	"gcc_sdcc1_apps_clk",
	"gcc_sdcc1_ice_core_clk",
	"gcc_sdcc2_ahb_clk",
	"gcc_sdcc2_apps_clk",
	"gcc_ufs_phy_ahb_clk",
	"gcc_ufs_phy_axi_clk",
	"gcc_ufs_phy_ice_core_clk",
	"gcc_ufs_phy_phy_aux_clk",
	"gcc_ufs_phy_rx_symbol_0_clk",
	"gcc_ufs_phy_tx_symbol_0_clk",
	"gcc_ufs_phy_unipro_core_clk",
	"gcc_usb30_prim_master_clk",
	"gcc_usb30_prim_master_cloned_clk",
	"gcc_usb30_prim_mock_utmi_clk",
	"gcc_usb30_prim_sleep_clk",
	"gcc_usb3_prim_phy_aux_clk",
	"gcc_usb3_prim_phy_com_aux_clk",
	"gcc_usb3_prim_phy_pipe_clk",
	"gcc_video_axi0_clk",
	"gcc_video_axi1_clk",
	"gpu_cc_debug_mux",
	"mc_cc_debug_mux",
	"measure_only_cnoc_clk",
	"measure_only_gcc_camera_ahb_clk",
	"measure_only_gcc_camera_xo_clk",
	"measure_only_gcc_disp_0_ahb_clk",
	"measure_only_gcc_disp_0_xo_clk",
	"measure_only_gcc_eva_ahb_clk",
	"measure_only_gcc_eva_xo_clk",
	"measure_only_gcc_gpu_cfg_ahb_clk",
	"measure_only_gcc_pcie_rscc_cfg_ahb_clk",
	"measure_only_gcc_pcie_rscc_xo_clk",
	"measure_only_gcc_video_ahb_clk",
	"measure_only_gcc_video_xo_clk",
	"measure_only_ipa_2x_clk",
	"measure_only_memnoc_clk",
	"measure_only_pcie_0_pipe_clk",
	"measure_only_pcie_1_pipe_clk",
	"measure_only_snoc_clk",
	"measure_only_ufs_phy_rx_symbol_0_clk",
	"measure_only_ufs_phy_tx_symbol_0_clk",
	"measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk",
	"video_cc_debug_mux",
};

static int gcc_debug_mux_sels[] = {
	0x173,		/* apss_cc_debug_mux */
	0xA8,		/* cam_cc_debug_mux */
	0x1E6,		/* eva_cc_debug_mux */
	0x6C,		/* gcc_aggre_noc_pcie_axi_clk */
	0x76,		/* gcc_aggre_ufs_phy_axi_clk */
	0x6D,		/* gcc_aggre_usb3_prim_axi_clk */
	0x11F,		/* gcc_boot_rom_ahb_clk */
	0xA9,		/* gcc_camera_cti_clk */
	0xA3,		/* gcc_camera_hf_axi_clk */
	0xA4,		/* gcc_camera_sf_axi_clk */
	0xAA,		/* gcc_camera_tsctr_clk */
	0x58,		/* gcc_cfg_noc_pcie_anoc_ahb_clk */
	0x38,		/* gcc_cfg_noc_usb3_prim_axi_clk */
	0x30,		/* gcc_cnoc_pcie_sf_axi_clk */
	0x13D,		/* gcc_ddrss_pcie_sf_qtb_clk */
	0xAD,		/* gcc_disp_0_hf_axi_clk */
	0xB1,		/* gcc_disp_sf_axi_clk */
	0xB2,		/* gcc_disp_tsctr_clk */
	0x1E3,		/* gcc_eva_axi0_clk */
	0x1E4,		/* gcc_eva_axi0c_clk */
	0x190,		/* gcc_gp10_clk */
	0x191,		/* gcc_gp11_clk */
	0x187,		/* gcc_gp1_clk */
	0x188,		/* gcc_gp2_clk */
	0x189,		/* gcc_gp3_clk */
	0x18A,		/* gcc_gp4_clk */
	0x18B,		/* gcc_gp5_clk */
	0x18C,		/* gcc_gp6_clk */
	0x18D,		/* gcc_gp7_clk */
	0x18E,		/* gcc_gp8_clk */
	0x18F,		/* gcc_gp9_clk */
	0x1C7,		/* gcc_gpu_gpll0_clk_src */
	0x1C8,		/* gcc_gpu_gpll0_div_clk_src */
	0x1C5,		/* gcc_gpu_memnoc_gfx_clk */
	0x197,		/* gcc_pcie_0_aux_clk */
	0x196,		/* gcc_pcie_0_cfg_ahb_clk */
	0x195,		/* gcc_pcie_0_mstr_axi_clk */
	0x19A,		/* gcc_pcie_0_phy_rchng_clk */
	0x198,		/* gcc_pcie_0_pipe_clk */
	0x199,		/* gcc_pcie_0_pipe_div2_clk */
	0x194,		/* gcc_pcie_0_slv_axi_clk */
	0x193,		/* gcc_pcie_0_slv_q2a_axi_clk */
	0x1A1,		/* gcc_pcie_1_aux_clk */
	0x1A0,		/* gcc_pcie_1_cfg_ahb_clk */
	0x19F,		/* gcc_pcie_1_mstr_axi_clk */
	0x1A4,		/* gcc_pcie_1_phy_rchng_clk */
	0x1A2,		/* gcc_pcie_1_pipe_clk */
	0x1A3,		/* gcc_pcie_1_pipe_div2_clk */
	0x19E,		/* gcc_pcie_1_slv_axi_clk */
	0x19D,		/* gcc_pcie_1_slv_q2a_axi_clk */
	0x104,		/* gcc_pdm2_clk */
	0x102,		/* gcc_pdm_ahb_clk */
	0x103,		/* gcc_pdm_xo4_clk */
	0x105,		/* gcc_pwm0_xo512_clk */
	0x106,		/* gcc_pwm1_xo512_clk */
	0x107,		/* gcc_pwm2_xo512_clk */
	0x108,		/* gcc_pwm3_xo512_clk */
	0x109,		/* gcc_pwm4_xo512_clk */
	0x10A,		/* gcc_pwm5_xo512_clk */
	0x10B,		/* gcc_pwm6_xo512_clk */
	0x10C,		/* gcc_pwm7_xo512_clk */
	0x10D,		/* gcc_pwm8_xo512_clk */
	0x10E,		/* gcc_pwm9_xo512_clk */
	0xA7,		/* gcc_qmip_camera_icp_ahb_clk */
	0xA1,		/* gcc_qmip_camera_nrt_ahb_clk */
	0xA2,		/* gcc_qmip_camera_rt_ahb_clk */
	0xAC,		/* gcc_qmip_disp_ahb_clk */
	0x1C2,		/* gcc_qmip_gpu_ahb_clk */
	0x192,		/* gcc_qmip_pcie_ahb_clk */
	0xB7,		/* gcc_qmip_video_cv_cpu_ahb_clk */
	0xB4,		/* gcc_qmip_video_cvp_ahb_clk */
	0xB6,		/* gcc_qmip_video_v_cpu_ahb_clk */
	0xB5,		/* gcc_qmip_video_vcodec_ahb_clk */
	0xFB,		/* gcc_qupv3_wrap0_core_2x_clk */
	0xFA,		/* gcc_qupv3_wrap0_core_clk */
	0xFC,		/* gcc_qupv3_wrap0_s0_clk */
	0xFD,		/* gcc_qupv3_wrap0_s1_clk */
	0xFE,		/* gcc_qupv3_wrap0_s2_clk */
	0xFF,		/* gcc_qupv3_wrap0_s3_clk */
	0x100,		/* gcc_qupv3_wrap0_s4_clk */
	0x101,		/* gcc_qupv3_wrap0_s5_clk */
	0xF1,		/* gcc_qupv3_wrap1_core_2x_clk */
	0xF0,		/* gcc_qupv3_wrap1_core_clk */
	0xF2,		/* gcc_qupv3_wrap1_s0_clk */
	0xF3,		/* gcc_qupv3_wrap1_s1_clk */
	0xF4,		/* gcc_qupv3_wrap1_s2_clk */
	0xF5,		/* gcc_qupv3_wrap1_s3_clk */
	0xF6,		/* gcc_qupv3_wrap1_s4_clk */
	0xF7,		/* gcc_qupv3_wrap1_s5_clk */
	0xF8,		/* gcc_qupv3_wrap_0_m_ahb_clk */
	0xF9,		/* gcc_qupv3_wrap_0_s_ahb_clk */
	0xEE,		/* gcc_qupv3_wrap_1_m_ahb_clk */
	0xEF,		/* gcc_qupv3_wrap_1_s_ahb_clk */
	0x1E9,		/* gcc_sdcc1_ahb_clk */
	0x1EA,		/* gcc_sdcc1_apps_clk */
	0x1EB,		/* gcc_sdcc1_ice_core_clk */
	0x1F0,		/* gcc_sdcc2_ahb_clk */
	0x1EF,		/* gcc_sdcc2_apps_clk */
	0x1F4,		/* gcc_ufs_phy_ahb_clk */
	0x1F3,		/* gcc_ufs_phy_axi_clk */
	0x1FA,		/* gcc_ufs_phy_ice_core_clk */
	0x1FB,		/* gcc_ufs_phy_phy_aux_clk */
	0x1F6,		/* gcc_ufs_phy_rx_symbol_0_clk */
	0x1F5,		/* gcc_ufs_phy_tx_symbol_0_clk */
	0x1F9,		/* gcc_ufs_phy_unipro_core_clk */
	0xDF,		/* gcc_usb30_prim_master_clk */
	0xE9,		/* gcc_usb30_prim_master_cloned_clk */
	0xE1,		/* gcc_usb30_prim_mock_utmi_clk */
	0xE0,		/* gcc_usb30_prim_sleep_clk */
	0xE2,		/* gcc_usb3_prim_phy_aux_clk */
	0xE3,		/* gcc_usb3_prim_phy_com_aux_clk */
	0xE4,		/* gcc_usb3_prim_phy_pipe_clk */
	0xB8,		/* gcc_video_axi0_clk */
	0xB9,		/* gcc_video_axi1_clk */
	0x1C4,		/* gpu_cc_debug_mux */
	0x148,		/* mc_cc_debug_mux */
	0x2D,		/* measure_only_cnoc_clk */
	0xA0,		/* measure_only_gcc_camera_ahb_clk */
	0xA6,		/* measure_only_gcc_camera_xo_clk */
	0xAB,		/* measure_only_gcc_disp_0_ahb_clk */
	0xAE,		/* measure_only_gcc_disp_0_xo_clk */
	0x1E2,		/* measure_only_gcc_eva_ahb_clk */
	0x1E5,		/* measure_only_gcc_eva_xo_clk */
	0x1C1,		/* measure_only_gcc_gpu_cfg_ahb_clk */
	0x1D2,		/* measure_only_gcc_pcie_rscc_cfg_ahb_clk */
	0x1D3,		/* measure_only_gcc_pcie_rscc_xo_clk */
	0xB3,		/* measure_only_gcc_video_ahb_clk */
	0xBA,		/* measure_only_gcc_video_xo_clk */
	0x1B8,		/* measure_only_ipa_2x_clk */
	0x141,		/* measure_only_memnoc_clk */
	0x19B,		/* measure_only_pcie_0_pipe_clk */
	0x1A5,		/* measure_only_pcie_1_pipe_clk */
	0xB,		/* measure_only_snoc_clk */
	0x1F8,		/* measure_only_ufs_phy_rx_symbol_0_clk */
	0x1F7,		/* measure_only_ufs_phy_tx_symbol_0_clk */
	0xEA,		/* measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk */
	0xBB,		/* video_cc_debug_mux */
};

static struct clk_debug_mux gcc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x62024,
	.post_div_offset = 0x62000,
	.cbcr_offset = 0x62004,
	.src_sel_mask = 0x3FF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 2,
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
	"gpu_cc_ahb_clk",
	"gpu_cc_crc_ahb_clk",
	"gpu_cc_cx_accu_shift_clk",
	"gpu_cc_cx_ff_clk",
	"gpu_cc_cx_gmu_clk",
	"gpu_cc_cxo_clk",
	"gpu_cc_freq_measure_clk",
	"gpu_cc_hub_aon_clk",
	"gpu_cc_hub_cx_int_clk",
	"gpu_cc_memnoc_gfx_clk",
	"gpu_cc_mnd1x_0_gfx3d_clk",
	"gpu_cc_mnd1x_1_gfx3d_clk",
	"measure_only_gpu_cc_cb_clk",
	"measure_only_gpu_cc_cxo_aon_clk",
	"measure_only_gpu_cc_sleep_clk",
	"measure_only_gpu_cc_demet_clk",
};

static int gpu_cc_debug_mux_sels[] = {
	0x15,		/* gpu_cc_ahb_clk */
	0x16,		/* gpu_cc_crc_ahb_clk */
	0x2F,		/* gpu_cc_cx_accu_shift_clk */
	0x1F,		/* gpu_cc_cx_ff_clk */
	0x1C,		/* gpu_cc_cx_gmu_clk */
	0x1D,		/* gpu_cc_cxo_clk */
	0xB,		/* gpu_cc_freq_measure_clk */
	0x2C,		/* gpu_cc_hub_aon_clk */
	0x1E,		/* gpu_cc_hub_cx_int_clk */
	0x20,		/* gpu_cc_memnoc_gfx_clk */
	0x27,		/* gpu_cc_mnd1x_0_gfx3d_clk */
	0x28,		/* gpu_cc_mnd1x_1_gfx3d_clk */
	0x2B,		/* measure_only_gpu_cc_cb_clk */
	0xA,		/* measure_only_gpu_cc_cxo_aon_clk */
	0x1A,		/* measure_only_gpu_cc_sleep_clk */
	0xC,		/* measure_only_gpu_cc_demet_clk */
};

static struct clk_debug_mux gpu_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x9564,
	.post_div_offset = 0x928C,
	.cbcr_offset = 0x9290,
	.src_sel_mask = 0xFF,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
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

static const char *const video_cc_debug_mux_parent_names[] = {
	"measure_only_video_cc_ahb_clk",
	"measure_only_video_cc_sleep_clk",
	"measure_only_video_cc_ts_xo_clk",
	"measure_only_video_cc_xo_clk",
	"video_cc_mvs0_clk",
	"video_cc_mvs0_freerun_clk",
	"video_cc_mvs0_shift_clk",
	"video_cc_mvs0_vpp0_clk",
	"video_cc_mvs0_vpp0_freerun_clk",
	"video_cc_mvs0_vpp1_clk",
	"video_cc_mvs0_vpp1_freerun_clk",
	"video_cc_mvs0b_clk",
	"video_cc_mvs0b_freerun_clk",
	"video_cc_mvs0c_clk",
	"video_cc_mvs0c_freerun_clk",
	"video_cc_mvs0c_shift_clk",
};

static int video_cc_debug_mux_sels[] = {
	0x14,		/* measure_only_video_cc_ahb_clk */
	0x19,		/* measure_only_video_cc_sleep_clk */
	0x16,		/* measure_only_video_cc_ts_xo_clk */
	0x15,		/* measure_only_video_cc_xo_clk */
	0x7,		/* video_cc_mvs0_clk */
	0x8,		/* video_cc_mvs0_freerun_clk */
	0x17,		/* video_cc_mvs0_shift_clk */
	0xE,		/* video_cc_mvs0_vpp0_clk */
	0xF,		/* video_cc_mvs0_vpp0_freerun_clk */
	0xB,		/* video_cc_mvs0_vpp1_clk */
	0xC,		/* video_cc_mvs0_vpp1_freerun_clk */
	0x4,		/* video_cc_mvs0b_clk */
	0x5,		/* video_cc_mvs0b_freerun_clk */
	0x11,		/* video_cc_mvs0c_clk */
	0x12,		/* video_cc_mvs0c_freerun_clk */
	0x18,		/* video_cc_mvs0c_shift_clk */
};

static struct clk_debug_mux video_cc_debug_mux = {
	.priv = &debug_mux_priv,
	.debug_offset = 0x9A4C,
	.post_div_offset = 0x8180,
	.cbcr_offset = 0x8184,
	.src_sel_mask = 0x3F,
	.src_sel_shift = 0,
	.post_div_mask = 0xF,
	.post_div_shift = 0,
	.post_div_val = 3,
	.mux_sels = video_cc_debug_mux_sels,
	.num_mux_sels = ARRAY_SIZE(video_cc_debug_mux_sels),
	.hw.init = &(const struct clk_init_data){
		.name = "video_cc_debug_mux",
		.ops = &clk_debug_mux_ops,
		.parent_names = video_cc_debug_mux_parent_names,
		.num_parents = ARRAY_SIZE(video_cc_debug_mux_parent_names),
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
	{ .mux = &video_cc_debug_mux, .regmap_name = "qcom,videocc" },
	{ .mux = &gpu_cc_debug_mux, .regmap_name = "qcom,gpucc" },
	{ .mux = &eva_cc_debug_mux, .regmap_name = "qcom,evacc" },
	{ .mux = &cam_cc_debug_mux, .regmap_name = "qcom,camcc" },
	{ .mux = &apss_cc_debug_mux, .regmap_name = "qcom,apsscc" },
	{ .mux = &gcc_debug_mux, .regmap_name = "qcom,gcc" },
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

static struct clk_dummy measure_only_cam_cc_gdsc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_cam_cc_gdsc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_cam_cc_sleep_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_cam_cc_sleep_clk",
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

static struct clk_dummy measure_only_eva_cc_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_eva_cc_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_eva_cc_sleep_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_eva_cc_sleep_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_eva_cc_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_eva_cc_xo_clk",
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

static struct clk_dummy measure_only_gcc_disp_0_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_disp_0_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_disp_0_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_disp_0_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_eva_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_eva_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_eva_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_eva_xo_clk",
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

static struct clk_dummy measure_only_gcc_pcie_rscc_cfg_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_pcie_rscc_cfg_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gcc_pcie_rscc_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gcc_pcie_rscc_xo_clk",
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

static struct clk_dummy measure_only_gpu_cc_cb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gpu_cc_cb_clk",
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

static struct clk_dummy measure_only_gpu_cc_demet_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gpu_cc_demet_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_gpu_cc_sleep_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_gpu_cc_sleep_clk",
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

static struct clk_dummy measure_only_mccc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_mccc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_memnoc_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_memnoc_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pcie_0_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_pcie_0_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_pcie_1_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_pcie_1_pipe_clk",
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

static struct clk_dummy measure_only_ufs_phy_rx_symbol_0_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_ufs_phy_rx_symbol_0_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_ufs_phy_tx_symbol_0_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_ufs_phy_tx_symbol_0_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_video_cc_ahb_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_video_cc_ahb_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_video_cc_sleep_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_video_cc_sleep_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_video_cc_ts_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_video_cc_ts_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_dummy measure_only_video_cc_xo_clk = {
	.rrate = 1000,
	.hw.init = &(const struct clk_init_data){
		.name = "measure_only_video_cc_xo_clk",
		.ops = &clk_dummy_ops,
	},
};

static struct clk_hw *debugcc_pikachu_hws[] = {
	&measure_only_apcs_l3_post_acd_clk.hw,
	&measure_only_apcs_l3_pre_acd_clk.hw,
	&measure_only_apcs_silver_post_acd_clk.hw,
	&measure_only_apcs_silver_pre_acd_clk.hw,
	&measure_only_cam_cc_gdsc_clk.hw,
	&measure_only_cam_cc_sleep_clk.hw,
	&measure_only_cnoc_clk.hw,
	&measure_only_eva_cc_ahb_clk.hw,
	&measure_only_eva_cc_sleep_clk.hw,
	&measure_only_eva_cc_xo_clk.hw,
	&measure_only_gcc_camera_ahb_clk.hw,
	&measure_only_gcc_camera_xo_clk.hw,
	&measure_only_gcc_disp_0_ahb_clk.hw,
	&measure_only_gcc_disp_0_xo_clk.hw,
	&measure_only_gcc_eva_ahb_clk.hw,
	&measure_only_gcc_eva_xo_clk.hw,
	&measure_only_gcc_gpu_cfg_ahb_clk.hw,
	&measure_only_gcc_pcie_rscc_cfg_ahb_clk.hw,
	&measure_only_gcc_pcie_rscc_xo_clk.hw,
	&measure_only_gcc_video_ahb_clk.hw,
	&measure_only_gcc_video_xo_clk.hw,
	&measure_only_gpu_cc_cb_clk.hw,
	&measure_only_gpu_cc_cxo_aon_clk.hw,
	&measure_only_gpu_cc_demet_clk.hw,
	&measure_only_gpu_cc_sleep_clk.hw,
	&measure_only_ipa_2x_clk.hw,
	&measure_only_mccc_clk.hw,
	&measure_only_memnoc_clk.hw,
	&measure_only_pcie_0_pipe_clk.hw,
	&measure_only_pcie_1_pipe_clk.hw,
	&measure_only_snoc_clk.hw,
	&measure_only_ufs_phy_rx_symbol_0_clk.hw,
	&measure_only_ufs_phy_tx_symbol_0_clk.hw,
	&measure_only_usb3_phy_wrapper_gcc_usb30_pipe_clk.hw,
	&measure_only_video_cc_ahb_clk.hw,
	&measure_only_video_cc_sleep_clk.hw,
	&measure_only_video_cc_ts_xo_clk.hw,
	&measure_only_video_cc_xo_clk.hw,
};

static const struct of_device_id clk_debug_match_table[] = {
	{ .compatible = "qcom,pikachu-debugcc" },
	{ }
};

static int clk_debug_pikachu_probe(struct platform_device *pdev)
{
	struct clk *clk;
	int ret = 0, i;

	BUILD_BUG_ON(ARRAY_SIZE(apss_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(apss_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(cam_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(cam_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(eva_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(eva_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gcc_debug_mux_parent_names) != ARRAY_SIZE(gcc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(gpu_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(gpu_cc_debug_mux_sels));
	BUILD_BUG_ON(ARRAY_SIZE(video_cc_debug_mux_parent_names) !=
		ARRAY_SIZE(video_cc_debug_mux_sels));

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

	for (i = 0; i < ARRAY_SIZE(debugcc_pikachu_hws); i++) {
		clk = devm_clk_register(&pdev->dev, debugcc_pikachu_hws[i]);
		if (IS_ERR(clk)) {
			dev_err(&pdev->dev, "Unable to register %s, err:(%ld)\n",
				qcom_clk_hw_get_name(debugcc_pikachu_hws[i]),
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
	.probe = clk_debug_pikachu_probe,
	.driver = {
		.name = "pikachu-debugcc",
		.of_match_table = clk_debug_match_table,
	},
};

static int __init clk_debug_pikachu_init(void)
{
	return platform_driver_register(&clk_debug_driver);
}
fs_initcall(clk_debug_pikachu_init);

MODULE_DESCRIPTION("QTI DEBUG CC PIKACHU Driver");
MODULE_LICENSE("GPL");
