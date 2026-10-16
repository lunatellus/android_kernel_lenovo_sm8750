// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */

#include <linux/delay.h>
#include <linux/eom_ioctl.h>
#include <linux/module.h>
#include <linux/phy_core.h>
#include <linux/slab.h>

#include "buffer_manager.h"
#include "eom_driver.h"
#include "pcie_eom_hamoa_phy_reg.h"

#if IS_ENABLED(CONFIG_PCI_MSM_EOM)

/**
 * hamoa_read_phy_reg - Read from appropriate PHY based on lane number
 * @phy: EOM PHY device
 * @lanenum: Lane number (0-based)
 * @reg_offset: Register offset (should be pre-adjusted for PHY B lanes)
 * @val: Pointer to store read value
 *
 * This wrapper determines which PHY (A or B) to use based on lane number
 * and routes the read operation accordingly. For bifurcated configurations,
 * lanes 0-1 use PHY A and lanes 2-3 use PHY B.
 * The caller should pre-adjust the offset for PHY B lanes.
 *
 * Return: 0 on success, negative error code on failure
 */
static int hamoa_read_phy_reg(struct eom_phy_device *phy, u32 reg_offset,
			    u32 lanenum, u32 *val)
{
	/* Check if PHY B exists and If use it for lanes 2-3*/
	if (phy_has_phy_b(phy) && lanenum >= HAMOA_PHY_B_LANE_START)
		return phy_b_read(phy, reg_offset, val);

	/* Use PHY-A for lanes 0-1 if PHY-B exists, else use it for all lanes */
	return phy_read(phy, reg_offset, val);
}

/**
 * hamoa_write_phy_reg - Write to appropriate PHY based on lane number
 * @phy: EOM PHY device
 * @lanenum: Lane number (0-based)
 * @reg_offset: Register offset (should be pre-adjusted for PHY B lanes)
 * @val: Value to write
 *
 * This wrapper determines which PHY (A or B) to use based on lane number
 * and routes the write operation accordingly. For bifurcated configurations,
 * lanes 0-1 use PHY A and lanes 2-3 use PHY B.
 * The caller should pre-adjust the offset for PHY B lanes.
 *
 * Return: 0 on success, negative error code on failure
 */
static int hamoa_write_phy_reg(struct eom_phy_device *phy, u32 reg_offset,
			     u32 lanenum, u32 val)
{
	/* Check if PHY B exists and If use it for lanes 2-3 */
	if (phy_has_phy_b(phy) && lanenum >= HAMOA_PHY_B_LANE_START)
		return phy_b_write(phy, reg_offset, val);

	/* Use PHY-A for lanes 0-1 if PHY-B exists, else use it for all lanes */
	return phy_write(phy, reg_offset, val);
}

/**
 * hamoa_phy_get_reg_offset - Calculate register offset with PHY B adjustment
 * @phy: EOM PHY device
 * @base_reg: Base register address
 * @lanenum: Lane number (0-based)
 *
 * This helper calculates the register offset for a given lane.
 * For PHY B lanes (2-3), it adjusts the lane number before calculating offset.
 *
 * Return: Adjusted register offset
 */
static inline u32 hamoa_phy_get_reg_offset(struct eom_phy_device *phy,
					   u32 base_reg, u32 lanenum)
{
	u32 adjusted_lanenum = lanenum;

	/* Adjust lane number for PHY B lanes */
	if (phy_has_phy_b(phy) && lanenum >= HAMOA_PHY_B_LANE_START)
		adjusted_lanenum = lanenum - HAMOA_PHY_B_LANE_START;

	return HAMOA_PHY_REG_ADDR(base_reg, adjusted_lanenum);
}

/**
 * struct eom_reg_seq - Single register write entry in an EOM init sequence
 * @offset:   Register offset from PHY base (before lane adjustment)
 * @value:    Value to write to the register
 * @delay_ns: Nanosecond delay to insert after the write (0 = no delay)
 *
 * Used to describe a flat, ordered table of register writes that make up
 * an EOM initialisation sequence.  The lane-specific address adjustment is
 * applied at run-time by hamoa_eom_write_seq(), so the offsets stored here
 * are always the lane-0 base offsets defined in pcie_eom_hamoa_phy_reg.h.
 */
struct eom_reg_seq {
	u32 offset;
	u32 value;
	u32 delay_ns;
};

/*
 * PCIe6 (Gen4x4) EOM initialization sequence
 *
 * Each row maps to one hamoa_write_phy_reg() call followed by an optional ndelay().
 * The delay_ns field captures the ndelay(EOM_REG_WRITE_DELAY).
 */
static const struct eom_reg_seq hamoa_pcie6_eom_init_seq_pre[] = {
	{ PCIE_PHY_QSERDES_TX0_RESET_GEN_MUXES,		0x03, 0 },
	{ PCIE_PHY_QSERDES_RX0_CDR_RESET_OVERRIDE,	0x0A, 0 },
};

static const struct eom_reg_seq hamoa_pcie6_eom_init_seq_post[] = {
	{ PCIE_PHY_QSERDES_RX0_EOM_CTRL2,		0x28, 0 },
	{ PCIE_PHY_QSERDES_RX0_AUX_CONTROL,		0x40, 0 },
	{ PCIE_PHY_QSERDES_RX0_RCLK_AUXDATA_SEL,	0xFC, 0 },
	{ PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL2,		0x80, 0 },
	{ PCIE_PHY_QSERDES_RX0_RX_MARG_VERTICAL_CTRL,	0x02, 0 },
	{ PCIE_PHY_QSERDES_RX0_AUXDATA_TB,		0x80, EOM_REG_WRITE_DELAY },
	{ PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL4,		0x33, 0 },
	{ PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3,		0x4C, EOM_REG_WRITE_DELAY },
	{ PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3,		0x48, 0 },
};

/*
 * PCIe6 (Gen4x4) per-sample register write sequence
 *
 * These are the fixed-value writes performed for every eye sample point in
 * msm_pcie_eom_process_eye_sample().
 */
static const struct eom_reg_seq hamoa_pcie6_eom_sample_seq[] = {
	{ PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL4,    0x33, EOM_REG_WRITE_DELAY },
	{ PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3,    0x4C, EOM_REG_WRITE_DELAY },
	{ PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL3,    0x48, 0 },
	/* EOM Comp clear sequence */
	{ PCIE_PHY_QSERDES_RX0_RCLK_AUXDATA_SEL,    0xFC, EOM_REG_WRITE_DELAY },
	{ PCIE_PHY_QSERDES_RX0_RCLK_AUXDATA_SEL,    0xF4, 0 },
};

/**
 * hamoa_eom_write_seq - Execute a register write sequence
 * @phy:         EOM PHY device structure
 * @lanenum:     Lane number (used to compute the lane-adjusted register address)
 * @seq:         Pointer to the first entry of the register sequence table
 * @count:       Number of entries in @seq (use ARRAY_SIZE())
 *
 * Iterates over @seq and for each entry:
 *   1. Computes the lane-adjusted address as offset + lanenum * lane_stride.
 *   2. Writes @entry.value to that address via hamoa_write_phy_reg().
 *   3. If @entry.delay_ns is non-zero, inserts an ndelay() after the write.
 *
 * On the first write failure the function returns immediately with the
 * negative error code from hamoa_write_phy_reg(), logging the failing offset and
 * sequence index for easy debugging.
 *
 * Return: 0 on success, negative error code on failure.
 */
static int hamoa_eom_write_seq(struct eom_phy_device *phy, u32 lanenum,
				const struct eom_reg_seq *seq, size_t count)
{
	u32 rc_idx = phy->index;
	u32 reg_addr;
	size_t i;
	int ret;

	for (i = 0; i < count; i++) {
		reg_addr = hamoa_phy_get_reg_offset(phy, seq[i].offset, lanenum);
		ret = hamoa_write_phy_reg(phy, reg_addr, lanenum, seq[i].value);
		if (ret < 0) {
			pr_err("RC%d: Failed to write reg offset=0x%x val=0x%x at seq[%zu]\n",
			       rc_idx, seq[i].offset, seq[i].value, i);
			return ret;
		}
		if (seq[i].delay_ns)
			ndelay(seq[i].delay_ns);
	}

	return 0;
}

static int msm_pcie_eom_init(struct eom_phy_device *phy, struct eom_lane *lane,
			     u32 is_positive_seq)
{
	u32 lanenum = lane->lane_num;
	u32 rc_idx = phy->index;
	u32 eom_ctrl1_val;
	u32 reg_addr;
	int ret = 0;

	if (atomic_read(&lane->eom_seq_stop))
		return 0;

	pr_info("RC%d EOM Initializing lanes (Seq: %s)\n", rc_idx,
		is_positive_seq ? "Upper/Positive" : "Lower/Negative");

	if (rc_idx >= HAMOA_PHY_MAX_RC_INSTANCES) {
		pr_err("Invalid RC index %d, max supported: %d\n",
			rc_idx, HAMOA_PHY_MAX_RC_INSTANCES - 1);
		return -EINVAL;
	}

	ret = hamoa_eom_write_seq(phy, lanenum, hamoa_pcie6_eom_init_seq_pre,
				  ARRAY_SIZE(hamoa_pcie6_eom_init_seq_pre));
	if (ret < 0) {
		pr_err("RC%d: EOM init sequence failed for lane %d\n", rc_idx, lanenum);
		return ret;
	}

	/* RX_REG_EOM_CTRL1: 0x98 for Upper (Positive), 0xD8 for Lower (Negative) */
	eom_ctrl1_val = is_positive_seq ? 0x98 : 0xD8;

	reg_addr = hamoa_phy_get_reg_offset(phy, PCIE_PHY_QSERDES_RX0_EOM_CTRL1, lanenum);
	ret = hamoa_write_phy_reg(phy, reg_addr, lanenum, eom_ctrl1_val);
	if (ret < 0) {
		pr_err("RC%d: Failed to write EOM_CTRL1\n", rc_idx);
		return ret;
	}

	ret = hamoa_eom_write_seq(phy, lanenum, hamoa_pcie6_eom_init_seq_post,
				  ARRAY_SIZE(hamoa_pcie6_eom_init_seq_post));
	if (ret < 0) {
		pr_err("RC%d: EOM init sequence failed for lane %d\n", rc_idx, lanenum);
		return ret;
	}

	pr_info("RC%d EOM Initialization completed for lane %d\n", rc_idx, lanenum);
	return ret;
}

static int msm_pcie_eom_process_eye_sample(struct eom_lane *lane, struct eom_phy_device *phy,
					   u32 lanenum, u32 xcoord, u32 ycoord,
					   u32 dtime, u32 is_positive_seq)
{
	u32 temp_err_low, temp_err_high;
	u32 rc_idx = phy->index;
	struct eom_entry entry;
	u32 xtmp, ytmp;
	u32 errorcntr;
	ssize_t wret;
	u32 reg_addr;
	int ret;

	if (atomic_read(&lane->eom_seq_stop))
		return 0;

	if (!lane->buffer) {
		pr_err("RC%d Lane:%d EOM buffer not initialized\n", rc_idx, lanenum);
		return -EINVAL;
	}

	ytmp = (ycoord | 0x80);
	reg_addr = hamoa_phy_get_reg_offset(phy, PCIE_PHY_QSERDES_RX0_AUXDATA_TB, lanenum);
	ret = hamoa_write_phy_reg(phy, reg_addr, lanenum, ytmp);
	if (ret < 0) {
		pr_err("RC%d: Failed to write AUXDATA_TB\n", rc_idx);
		return ret;
	}

	xtmp = (xcoord | 0x40);
	reg_addr = hamoa_phy_get_reg_offset(phy, PCIE_PHY_QSERDES_RX0_AUX_CONTROL, lanenum);
	ret = hamoa_write_phy_reg(phy, reg_addr, lanenum, xtmp);
	if (ret < 0) {
		pr_err("RC%d: Failed to write AUX_CONTROL\n", rc_idx);
		return ret;
	}

	ndelay(EOM_REG_WRITE_DELAY);

	ret = hamoa_eom_write_seq(phy, lanenum, hamoa_pcie6_eom_sample_seq,
				  ARRAY_SIZE(hamoa_pcie6_eom_sample_seq));
	if (ret < 0) {
		pr_err("RC%d: Failed to execute sample sequence for lane %d\n", rc_idx, lanenum);
		return ret;
	}

	msleep(dtime);

	/* Read Errors */
	reg_addr = hamoa_phy_get_reg_offset(phy, PCIE_PHY_QSERDES_RX0_IA_ERROR_COUNTER_LOW,
					    lanenum);
	ret = hamoa_read_phy_reg(phy, reg_addr, lanenum, &temp_err_low);
	if (ret < 0) {
		pr_err("RC%d: Failed to read ERROR_COUNTER_LOW\n", rc_idx);
		return ret;
	}

	reg_addr = hamoa_phy_get_reg_offset(phy, PCIE_PHY_QSERDES_RX0_IA_ERROR_COUNTER_HIGH,
					    lanenum);
	ret = hamoa_read_phy_reg(phy, reg_addr, lanenum, &temp_err_high);
	if (ret < 0) {
		pr_err("RC%d: Failed to read ERROR_COUNTER_HIGH\n", rc_idx);
		return ret;
	}

	errorcntr = (temp_err_low & 0xFF) | ((temp_err_high & 0xFF) << 8);

	if (errorcntr == 0xFFFF) {
		pr_warn("RC%d Lane:%d Error counter saturated at x=%d y=%d, consider increasing dwell time\n",
				rc_idx, lanenum, xcoord, ycoord);
	}

	/* Store Entry: Apply sign to ycoord based on sequence */
	entry.x = (xcoord > 31) ? xcoord - 64 : xcoord;
	entry.y = (is_positive_seq ? 1 : -1) * ycoord;
	entry.error_count = errorcntr;

	wret = eom_buffer_write(lane->buffer, (char *)&entry, sizeof(entry));
	if (wret < 0) {
		pr_err("RC%d Lane:%d Failed to write EOM buffer x=%d y=%d: %zd\n",
				rc_idx, lanenum, entry.x, entry.y, wret);
		return (int)wret;
	}

	reg_addr = hamoa_phy_get_reg_offset(phy, PCIE_PHY_QSERDES_RX0_RX_MARG_CTRL4, lanenum);
	ret = hamoa_write_phy_reg(phy, reg_addr, lanenum, 0x23);
	if (ret < 0) {
		pr_err("RC%d: Failed to write MARG_CTRL_4\n", rc_idx);
		return ret;
	}

	return 0;
}

static int msm_pcie_eom_eye_seq(struct eom_lane *lane, u32 is_positive_seq, u32 lanenum)
{
	struct eom_phy_device *phy = lane->phy_dev;
	u32 ycoord, xcoord;
	int ret = 0;
	u32 dtime;

	if (atomic_read(&lane->eom_seq_stop))
		return 0;

	/* Convert microseconds to milliseconds, ensure minimum 1ms */
	dtime = lane->dwell_time_us / 1000;
	if (dtime == 0)
		dtime = 1;

	/* ycoord 0..128, xcoord 0..64 */
	ycoord = 0;
	while (ycoord < MAX_EYE_HEIGHT) {
		xcoord = 0;
		while (xcoord < MAX_EYE_WIDTH) {
			ret = msm_pcie_eom_process_eye_sample(lane, phy, lanenum, xcoord,
							      ycoord, dtime,
							      is_positive_seq);
			if (ret < 0)
				return ret;

			xcoord++;
			if (atomic_read(&lane->eom_seq_stop))
				break;
		}
		ycoord++;
		if (atomic_read(&lane->eom_seq_stop))
			break;
	}

	return 0;
}

/**
 * phy_pcie_eom_sequence - Hamoa PCIe PHY EOM sequence implementation.
 * Strong symbol that overrides the weak fallback in eom_driver.c when
 * CONFIG_ARCH_X1E80100 is enabled.
 * Runs both Upper (Positive) and Lower (Negative) eye scans.
 */
int phy_pcie_eom_sequence(struct eom_lane *lane)
{
	struct eom_phy_device *phy = lane->phy_dev;
	u32 rc_idx = phy->index;
	int ret = 0;

	if (!lane || !lane->phy_dev || !lane->seq) {
		pr_err("Invalid lane or phy_dev pointer or seq pointer\n");
		return -EINVAL;
	}

	pr_info("Running Hamoa PCIe EOM for %s instance %u lane %d\n",
		lane->seq->name, rc_idx, lane->lane_num);

	/* 1. Run Positive/Upper Sequence (EOM_CTRL1 = 0x98) */
	pr_info("Initializing Phy and running EOM for Upper/Positive sequence\n");
	ret = msm_pcie_eom_init(phy, lane, POSITIVE_SEQUENCE);
	if (ret < 0) {
		pr_err("PHY RC%d EOM initialization failed: %d\n", rc_idx, ret);
		return ret;
	}

	ret = msm_pcie_eom_eye_seq(lane, 1, lane->lane_num);
	if (ret < 0) {
		pr_err("PHY RC%d EOM eye sequence failed: %d\n", rc_idx, ret);
		return ret;
	}

	if (atomic_read(&lane->eom_seq_stop))
		return 0;

	/* Short delay between passes */
	ndelay(1000);

	/* 2. Run Negative/Lower Sequence (EOM_CTRL1 = 0xD8) */
	pr_info("Initializing Phy and running EOM for Lower/Negative sequence\n");
	ret = msm_pcie_eom_init(phy, lane, NEGATIVE_SEQUENCE);
	if (ret < 0) {
		pr_err("PHY RC%d EOM initialization failed: %d\n", rc_idx, ret);
		return ret;
	}

	ret = msm_pcie_eom_eye_seq(lane, 0, lane->lane_num);
	if (ret < 0) {
		pr_err("PHY RC%d EOM eye sequence failed: %d\n", rc_idx, ret);
		return ret;
	}

	pr_info("PHY RC%d EOM eye sequence completed for lane %d\n", rc_idx, lane->lane_num);

	return 0;
}
#endif /* CONFIG_PCI_MSM_EOM */
