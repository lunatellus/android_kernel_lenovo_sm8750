// SPDX-License-Identifier: GPL-2.0-only
/*
 * STM Passthrough driver for Hypervisor Guest Virtual Machine (GVM)
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This driver enables Linux STM framework sources (ftrace, console, etc.)
 * running inside a GVM to forward trace data to the host PVM's CoreSight
 * STM hardware through a memory-mapped stimulus port passthrough region.
 *
 * Architecture:
 *
 *   GVM (this driver runs here):
 *     ftrace/console -> stm_core -> stm_p_ost -> stm_passthrough
 *                                                      |
 *                                              MMIO writes to
 *                                           STM stimulus port
 *                                        (hypervisor passthrough)
 *                                                      |
 *   PVM (hardware owner):                              v
 *     STM HW -> ATB funnel -> TMC-ETR -> .bin file
 *
 * Setup:
 *   PVM must grant the GVM access to the STM stimulus port region:
 *     pass loc mem:<phys_base>,<size>,rw=<phys_base>
 *
 *   GVM device tree must describe the accessible region:
 *     stm_passthrough: stm-passthrough@<phys_base> {
 *         compatible = "qcom,stm-passthrough";
 *         reg = <<phys_base> <size>>;
 *     };
 *
 * Channel mapping:
 *   Each software channel (one per CPU for ftrace) is mapped to a unique
 *   physical offset within the stimulus port:
 *     hw_addr = stimulus_base + (channel * STM_BYTES_PER_CHANNEL) + pkt_type_offset
 *
 *   This ensures per-CPU trace streams land on distinct STP channels in the
 *   hardware ATB stream, allowing the decoder to reconstruct individual frames.
 *
 * Stimulus port packet type encoding (ARM CoreSight STM Architecture):
 *   Each channel occupies a 256-byte window in the stimulus port address space.
 *   Within the window, the byte offset selects the STPv2 packet type. The base
 *   types below have the maximum set of flags present; flags are cleared (via
 *   bitwise AND-NOT) to remove the corresponding feature from the packet:
 *
 *     STM_PKT_TYPE_DATA = 0x98  (D8, Guaranteed, Marked, Timestamped)
 *     STM_PKT_TYPE_FLAG = 0xE8  (FLAG, Guaranteed, Timestamped)
 *     STM_PKT_TYPE_TRIG = 0xF8  (TRIG, Guaranteed, Timestamped)
 *
 *   STM_FLAG_TIMESTAMPED (BIT(3)) and STM_FLAG_MARKED (BIT(4)) are cleared
 *   from the base type when the corresponding feature is *not* requested.
 *   Guaranteed delivery (BIT(7)) is always present in the base types above.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stm.h>
#include <uapi/linux/coresight-stm.h>
#include <uapi/linux/stm.h>

#define DRIVER_NAME "stm-passthrough"

/*
 * STM stimulus port packet type encoding.
 *
 * The low byte of the write address within a channel's window tells the STM
 * hardware what type of STPv2 packet to generate. These values come from the
 * ARM CoreSight STM architecture specification (IHI0029).
 *
 * Each base type below represents the packet with the maximum set of optional
 * features enabled. Individual features (timestamp, marked) are cleared from
 * the base offset when they are NOT requested by the caller:
 *   offset = base_type & ~flags_to_clear
 *
 * Bit positions within the channel window offset:
 *   BIT(3) - Timestamped (cleared = no timestamp in generated STP packet)
 *   BIT(4) - Marked      (cleared = no mark, for data packets)
 *   BIT(7) - Guaranteed  (always set in base types below = guaranteed delivery)
 */
enum stm_pkt_type {
	STM_PKT_TYPE_DATA = 0x98,	/* D8 + Guaranteed + Marked + Timestamped */
	STM_PKT_TYPE_FLAG = 0xE8,	/* FLAG + Guaranteed + Timestamped */
	STM_PKT_TYPE_TRIG = 0xF8,	/* TRIG + Guaranteed + Timestamped */
};

/*
 * Number of bytes each channel occupies in the stimulus port address space.
 * Derived from the CoreSight STM architecture: each port (master/channel pair)
 * has a 256-byte window that encodes packet type and flags in the offset.
 */
#define STM_BYTES_PER_CHANNEL	256U

/*
 * Compute the write address offset within a channel's 256-byte window.
 *
 * @type:  base packet type (e.g. STM_PKT_TYPE_DATA = 0x98)
 * @flags: bitmask of STM_FLAG_* bits to CLEAR from the base type
 *         (i.e. features that are NOT wanted in this particular packet)
 *
 * The base packet types encode the fully-featured version. To suppress a
 * feature, clear its bit: e.g. to omit the timestamp from a DATA packet,
 * pass flags = STM_FLAG_TIMESTAMPED so that BIT(3) is cleared from 0x98.
 */
#define STM_CHANNEL_OFFSET(type, flags)	((type) & ~(flags))

/**
 * struct stm_passthrough - per-device driver state
 * @dev:       backing platform device
 * @base:      ioremap'd virtual address of the stimulus port base
 * @phys_base: physical address (for diagnostics only)
 * @stm:       stm_data instance registered with the STM core
 */
struct stm_passthrough {
	struct device	*dev;
	void __iomem	*base;
	resource_size_t	 phys_base;
	struct stm_data	 stm;
};

/* Retrieve the driver state from an embedded stm_data pointer. */
static inline struct stm_passthrough *
to_stm_passthrough(struct stm_data *stm)
{
	return container_of(stm, struct stm_passthrough, stm);
}

/*
 * stm_passthrough_write - issue a single MMIO write to the stimulus port
 *
 * The STM hardware determines the STPv2 packet type from the write address.
 * The written value is the packet payload. Write width determines whether the
 * hardware generates a D8/D16/D32/D64 STP data packet.
 *
 * If @data is NULL (valid for FLAG packets where no payload is required), a
 * zero byte is written to trigger the hardware packet generation.
 * If @data is not naturally aligned for the write width, it is copied to a
 * local aligned buffer before the write.
 */
static void stm_passthrough_write(void __iomem *addr, const void *data,
				  u32 size)
{
	static const u64 zero;
	u8 aligned_buf[8];

	if (!data)
		data = &zero;

	if ((unsigned long)data & (size - 1)) {
		memcpy(aligned_buf, data, size);
		data = aligned_buf;
	}

	switch (size) {
#ifdef CONFIG_64BIT
	case 8:
		writeq_relaxed(*(const u64 *)data, addr);
		break;
#endif
	case 4:
		writel_relaxed(*(const u32 *)data, addr);
		break;
	case 2:
		writew_relaxed(*(const u16 *)data, addr);
		break;
	case 1:
		writeb_relaxed(*(const u8 *)data, addr);
		break;
	default:
		break;
	}
}

/**
 * stm_passthrough_packet - STM core callback: deliver one STPv2 packet
 * @stm_data:  pointer to the embedded stm_data (use to_stm_passthrough())
 * @master:    STP master number (unused; single master derived from DT reg)
 * @channel:   STP channel number (0 .. sw_nchannels-1); one per CPU for ftrace
 * @packet:    STP packet type: STP_PACKET_DATA or STP_PACKET_FLAG
 * @flags:     STP packet flags: STP_PACKET_MARKED, STP_PACKET_TIMESTAMPED
 * @size:      payload size in bytes (power-of-two, 0 for FLAG)
 * @payload:   pointer to payload bytes (may be NULL for FLAG packets)
 *
 * Translates the generic STM core request into a physical MMIO write to the
 * STM stimulus port. The write address encodes:
 *   - Which channel: base + channel * STM_BYTES_PER_CHANNEL
 *   - What packet type and flags: low byte offset within the channel window
 *
 * The CoreSight STM stimulus port uses "clear to enable" semantics for the
 * optional packet features (timestamp, marked): the base type offsets below
 * represent the fully-featured packet, and bits are cleared to suppress
 * unwanted features. Guaranteed delivery (BIT(7)) is always present.
 *
 * Returns the number of payload bytes consumed, or negative on error.
 */
static ssize_t notrace
stm_passthrough_packet(struct stm_data *stm_data, unsigned int master,
		       unsigned int channel, unsigned int packet,
		       unsigned int flags, unsigned int size,
		       const unsigned char *payload)
{
	struct stm_passthrough *drv = to_stm_passthrough(stm_data);
	void __iomem *ch_addr;
	unsigned int stm_flags;

	if (unlikely(!drv->base))
		return -ENXIO;

	if (unlikely(channel >= drv->stm.sw_nchannels))
		return -EINVAL;

	/*
	 * Map the software channel to its hardware stimulus port address.
	 *
	 * Each channel occupies STM_BYTES_PER_CHANNEL bytes in the stimulus
	 * window. CPU-0 gets offset 0, CPU-1 gets offset 256, etc. This means
	 * each CPU's trace stream lands on a distinct STP channel number in
	 * the hardware ATB output, allowing per-channel reconstruction in QTF.
	 */
	ch_addr = drv->base + ((resource_size_t)channel * STM_BYTES_PER_CHANNEL);

	/*
	 * Build the set of flag bits to CLEAR from the base packet type offset.
	 *
	 * The base packet type offsets (0x98 for DATA, 0xE8 for FLAG) represent
	 * the fully-featured packet. Each STM_FLAG_* bit added to stm_flags will
	 * be cleared from the base offset via AND-NOT, suppressing that feature
	 * in the generated STP packet.
	 *
	 * This matches the Qualcomm CoreSight STM driver convention where the
	 * flag bits track which features the caller requests to SUPPRESS.
	 */
	stm_flags = (flags & STP_PACKET_TIMESTAMPED) ? STM_FLAG_TIMESTAMPED : 0;

	/* Hardware can only write power-of-two sizes. */
	size = size ? rounddown_pow_of_two(size) : size;

	switch (packet) {
	case STP_PACKET_FLAG:
		ch_addr += STM_CHANNEL_OFFSET(STM_PKT_TYPE_FLAG, stm_flags);
		/*
		 * stm_core passes size=0 for FLAG packets. The hardware still
		 * requires a write to the stimulus port address to generate the
		 * STP FLAG packet; stm_passthrough_write handles NULL payload
		 * safely by writing a zero byte. Return 0 per the stm_data API
		 * contract (no payload bytes were consumed).
		 */
		stm_passthrough_write(ch_addr, payload, 1);
		return 0;

	case STP_PACKET_DATA:
		stm_flags |= (flags & STP_PACKET_MARKED) ? STM_FLAG_MARKED : 0;
		ch_addr += STM_CHANNEL_OFFSET(STM_PKT_TYPE_DATA, stm_flags);
		stm_passthrough_write(ch_addr, payload, size);
		break;

	default:
		return -EOPNOTSUPP;
	}

	return size;
}

/*
 * stm_passthrough_link / stm_passthrough_unlink
 *
 * Called by stm_core when a source (e.g. stm_ftrace) is linked to or unlinked
 * from this STM device. No per-channel hardware setup is needed for a
 * passthrough device; the channel is active as soon as it is written to.
 */
static int stm_passthrough_link(struct stm_data *stm_data,
				unsigned int master, unsigned int channel)
{
	return 0;
}

static void stm_passthrough_unlink(struct stm_data *stm_data,
				   unsigned int master, unsigned int channel)
{
}

static int stm_passthrough_probe(struct platform_device *pdev)
{
	struct stm_passthrough *drv;
	struct resource *res;
	unsigned long nchannels;
	int ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->dev = &pdev->dev;

	/*
	 * The stimulus port physical address and size come from the DT "reg"
	 * property. This region must have been granted to the GVM by the PVM
	 * via hypervisor memory passthrough configuration.
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no memory resource in DT\n");
		return -ENODEV;
	}

	drv->phys_base = res->start;

	drv->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(drv->base)) {
		dev_err(&pdev->dev,
			"ioremap failed for stimulus port 0x%llx\n",
			(u64)res->start);
		return PTR_ERR(drv->base);
	}

	/*
	 * The number of software channels exposed to stm_core is derived
	 * from the mapped region size. Each channel consumes STM_BYTES_PER_CHANNEL
	 * bytes of stimulus address space. Cap at STP_CHANNEL_MAX.
	 */
	nchannels = resource_size(res) / STM_BYTES_PER_CHANNEL;
	if (!nchannels) {
		dev_err(&pdev->dev,
			"stimulus region 0x%llx too small (min %u bytes)\n",
			(u64)resource_size(res), STM_BYTES_PER_CHANNEL);
		return -EINVAL;
	}
	nchannels = min_t(unsigned long, nchannels, STP_CHANNEL_MAX);

	/*
	 * Register with the Linux STM core. stm_core will allocate master/channel
	 * pairs from [sw_start..sw_end] x [0..sw_nchannels-1] and call our
	 * packet() callback for each trace write.
	 *
	 * We expose a single master (sw_start == sw_end == 0). All channels
	 * within that master are available to sources like stm_ftrace.
	 */
	drv->stm.name        = DRIVER_NAME;
	drv->stm.sw_start    = 0;
	drv->stm.sw_end      = 0;
	drv->stm.sw_nchannels = (unsigned int)nchannels;
	drv->stm.packet      = stm_passthrough_packet;
	drv->stm.link        = stm_passthrough_link;
	drv->stm.unlink      = stm_passthrough_unlink;

	ret = stm_register_device(&pdev->dev, &drv->stm, THIS_MODULE);
	if (ret) {
		dev_err(&pdev->dev, "stm_register_device failed: %d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, drv);

	dev_info(&pdev->dev,
		 "STM passthrough ready: phys=0x%llx size=0x%llx channels=%lu\n",
		 (u64)drv->phys_base, (u64)resource_size(res), nchannels);

	return 0;
}

/*
 * stm_passthrough_remove - platform driver remove callback
 *
 * Returns void as required by the kernel 6.11+ platform_driver API.
 * The devm_ allocations (ioremap, kzalloc) are released automatically.
 */
static void stm_passthrough_remove(struct platform_device *pdev)
{
	struct stm_passthrough *drv = platform_get_drvdata(pdev);

	stm_unregister_device(&drv->stm);
}

static const struct of_device_id stm_passthrough_of_match[] = {
	{ .compatible = "qcom,stm-passthrough" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, stm_passthrough_of_match);

static struct platform_driver stm_passthrough_driver = {
	.probe  = stm_passthrough_probe,
	.remove = stm_passthrough_remove,
	.driver = {
		.name           = DRIVER_NAME,
		.of_match_table = stm_passthrough_of_match,
	},
};

module_platform_driver(stm_passthrough_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("STM passthrough driver for GVM-to-PVM trace forwarding");
