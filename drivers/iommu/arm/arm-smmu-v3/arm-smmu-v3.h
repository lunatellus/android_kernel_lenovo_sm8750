/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * IOMMU API for ARM architected SMMUv3 implementations.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _ARM_SMMU_V3_H
#define _ARM_SMMU_V3_H

#include <linux/delay.h>
#include <linux/iommu.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/sizes.h>
#include <linux/virtio-iommu.h>
#include <linux/platform_device.h>

struct arm_smmu_device;

#include <asm/arm-smmu-v3-common.h>

#define Q_IDX(llq, p)			((p) & ((1 << (llq)->max_n_shift) - 1))
#define Q_WRP(llq, p)			((p) & (1 << (llq)->max_n_shift))
#define Q_ENT(q, p)			((q)->base +			\
					 Q_IDX(&((q)->llq), p) *	\
					 (q)->ent_dwords)

/* Ensure DMA allocations are naturally aligned */
#ifdef CONFIG_CMA_ALIGNMENT
#define Q_MAX_SZ_SHIFT			(PAGE_SHIFT + CONFIG_CMA_ALIGNMENT)
#else
#define Q_MAX_SZ_SHIFT			(PAGE_SHIFT + MAX_PAGE_ORDER)
#endif

#define CMDQ_PROD_OWNED_FLAG		Q_OVERFLOW_FLAG

/* High-level queue structures */
#define ARM_SMMU_POLL_TIMEOUT_US	1000000 /* 1s! */
#define ARM_SMMU_POLL_SPIN_COUNT	10

#define MSI_IOVA_BASE			0x8000000
#define MSI_IOVA_LENGTH			0x100000

struct arm_smmu_ll_queue {
	union {
		u64			val;
		struct {
			u32		prod;
			u32		cons;
		};
		struct {
			atomic_t	prod;
			atomic_t	cons;
		} atomic;
		u8			__pad[SMP_CACHE_BYTES];
	} ____cacheline_aligned_in_smp;
	u32				max_n_shift;
};

struct arm_smmu_queue {
	struct arm_smmu_ll_queue	llq;
	int				irq; /* Wired interrupt */

	__le64				*base;
	dma_addr_t			base_dma;
	u64				q_base;

	size_t				ent_dwords;

	u32 __iomem			*prod_reg;
	u32 __iomem			*cons_reg;
};

struct arm_smmu_queue_poll {
	ktime_t				timeout;
	unsigned int			delay;
	unsigned int			spin_cnt;
	bool				wfe;
};

struct arm_smmu_cmdq {
	struct arm_smmu_queue		q;
	atomic_long_t			*valid_map;
	atomic_t			owner_prod;
	atomic_t			lock;
	bool				(*supports_cmd)(struct arm_smmu_cmdq_ent *ent);
};

static inline bool arm_smmu_cmdq_supports_cmd(struct arm_smmu_cmdq *cmdq,
					      struct arm_smmu_cmdq_ent *ent)
{
	return cmdq->supports_cmd ? cmdq->supports_cmd(ent) : true;
}

struct arm_smmu_evtq {
	struct arm_smmu_queue		q;
	struct iopf_queue		*iopf;
	u32				max_stalls;
};

struct arm_smmu_priq {
	struct arm_smmu_queue		q;
};

/* High-level stream table and context descriptor structures */
struct arm_smmu_ctx_desc {
	u16				asid;
};

struct arm_smmu_ctx_desc_cfg {
	union {
		struct {
			struct arm_smmu_cd *table;
			unsigned int num_ents;
		} linear;
		struct {
			struct arm_smmu_cdtab_l1 *l1tab;
			struct arm_smmu_cdtab_l2 **l2ptrs;
			unsigned int num_l1_ents;
		} l2;
	};
	dma_addr_t			cdtab_dma;
	unsigned int			used_ssids;
	u8				in_ste;
	u8				s1fmt;
	/* log2 of the maximum number of CDs supported by this table */
	u8				s1cdmax;
};

static inline bool
arm_smmu_cdtab_allocated(struct arm_smmu_ctx_desc_cfg *cfg)
{
	return cfg->linear.table || cfg->l2.l1tab;
}

/* True if the cd table has SSIDS > 0 in use. */
static inline bool arm_smmu_ssids_in_use(struct arm_smmu_ctx_desc_cfg *cd_table)
{
	return cd_table->used_ssids;
}

struct arm_smmu_s2_cfg {
	u16				vmid;
};

struct arm_smmu_domain;
struct arm_smmu_master;

struct arm_smmu_impl_ops {
	int (*device_reset)(struct arm_smmu_device *smmu);
	void (*device_remove)(struct arm_smmu_device *smmu);
	int (*init_structures)(struct arm_smmu_device *smmu);
	struct arm_smmu_cmdq *(*get_secondary_cmdq)(
		struct arm_smmu_device *smmu, struct arm_smmu_cmdq_ent *ent);
	int (*probe_device)(struct arm_smmu_device *smmu, struct device *dev);
	u32 (*read_idr)(struct arm_smmu_device *smmu, u32 offset);
	int (*install_ste)(struct arm_smmu_master *master, struct arm_smmu_domain *new,
			struct arm_smmu_domain *old);
	int (*cmdq_issue_cmdlist)(struct arm_smmu_device *smmu, u64 *cmds, int nr, bool sync);
	void (*sync_cd)(struct arm_smmu_master *master, int ssid, bool leaf);
	void (*tlb_inv_context)(struct arm_smmu_device *smmu, struct arm_smmu_domain *smmu_domain);
	void (*tlb_inv_range)(struct arm_smmu_device *smmu, struct arm_smmu_domain *smmu_domain,
				unsigned long iova, size_t size, size_t granule,
				bool leaf, u16 asid);
};

/*
 * Describes resources required for on/off power operation.
 * Separate reference count is provided for atomic/nonatomic
 * operations.
 * gdscs - on kernel 6.6, power domains are used instead. This
 * field can be removed once no legacy targets using it remain.
 */
struct arm_smmu_power_resources {
	struct device			*dev;

	struct clk			**clocks;
	int				num_clocks;

	struct regulator_bulk_data	*gdscs;
	int				num_gdscs;

	struct icc_path			*icc_path;

	/* Protects power_count */
	struct mutex			power_lock;
	int				power_count;

	int (*resume)(struct arm_smmu_power_resources *pwr);
	void (*suspend)(struct arm_smmu_power_resources *pwr);
};

int arm_smmu_power_on(struct arm_smmu_power_resources *pwr);
void arm_smmu_power_off(struct arm_smmu_device *smmu,
			struct arm_smmu_power_resources *pwr);
struct arm_smmu_power_resources *arm_smmu_init_power_resources(
			struct device *dev);

/* An SMMUv3 instance */
struct arm_smmu_device {
	struct device			*dev;
	struct device			*impl_dev;
	const struct arm_smmu_impl_ops	*impl_ops;

	void __iomem			*base;
	void __iomem			*page1;

	/* See arm-smmu-v3-common.h*/
	u32				features;

#define ARM_SMMU_OPT_SKIP_PREFETCH	(1 << 0)
#define ARM_SMMU_OPT_PAGE0_REGS_ONLY	(1 << 1)
#define ARM_SMMU_OPT_MSIPOLL		(1 << 2)
#define ARM_SMMU_OPT_CMDQ_FORCE_SYNC	(1 << 3)
#define ARM_SMMU_OPT_TEGRA241_CMDQV	(1 << 4)
#define ARM_SMMU_OPT_VIRTIO		(1 << 4)
	u32				options;

	struct arm_smmu_cmdq		cmdq;
	struct arm_smmu_evtq		evtq;
	struct arm_smmu_priq		priq;

	int				gerr_irq;
	int				combined_irq;

	unsigned long			ias; /* IPA */
	unsigned long			oas; /* PA */
	unsigned long			pgsize_bitmap;

#define ARM_SMMU_MAX_ASIDS		(1 << 16)
	unsigned int			asid_bits;

#define ARM_SMMU_MAX_VMIDS		(1 << 16)
	unsigned int			vmid_bits;
	struct ida			vmid_map;

	unsigned int			ssid_bits;
	unsigned int			sid_bits;

	struct arm_smmu_strtab_cfg	strtab_cfg;

	/* IOMMU core code handle */
	struct iommu_device		iommu;

	struct rb_root			streams;
	struct mutex			streams_mutex;

	struct arm_smmu_power_resources *pwr;
};

struct arm_smmu_stream {
	u32				id;
	struct arm_smmu_master		*master;
	struct rb_node			node;
};

/* SMMU private data for each master */
struct arm_smmu_master {
	struct arm_smmu_device		*smmu;
	struct device			*dev;
	struct arm_smmu_domain          *domain;
	struct arm_smmu_stream		*streams;
	/* Locked by the iommu core using the group mutex */
	struct arm_smmu_ctx_desc_cfg	cd_table;
	unsigned int			num_streams;
	bool				ats_enabled : 1;
	bool				ste_ats_enabled : 1;
	bool				stall_enabled;
	bool				sva_enabled;
	bool				iopf_enabled;
	unsigned int			ssid_bits;
};

/* SMMU private data for an IOMMU domain */
enum arm_smmu_domain_stage {
	ARM_SMMU_DOMAIN_S1 = 0,
	ARM_SMMU_DOMAIN_S2,
};

/*
 * VIRTIO_IOMMU_ATTACH_TABLE associates @id with a set of sids. @refs is the
 * number of stream-ids in this set.
 * These fields are protected by @lock, which must be held when issueing virtio_iommu
 * commands using @id.
 * @id
 * IDs less than ARM_SMMU_MIN_VIRTIO_ID are not valid.
 * The arm-smmu-qcom-virtio cbs will no-op any calls made with an invalid id.
 * This can occur during arm_smmu_attach_dev(), as the CD is setup/invalidated
 * before to the STE is setup.
 */
struct arm_smmu_domain_virtio {
	spinlock_t	lock;
	u32		id;
	refcount_t	refs;
};

struct arm_smmu_domain {
	struct arm_smmu_device		*smmu;
	struct mutex			init_mutex; /* Protects smmu pointer */

	struct io_pgtable_ops		*pgtbl_ops;
	atomic_t			nr_ats_masters;

	enum arm_smmu_domain_stage	stage;
	union {
		struct arm_smmu_ctx_desc	cd;
		struct arm_smmu_s2_cfg		s2_cfg;
	};

	struct iommu_domain		domain;

	/* List of struct arm_smmu_master_domain */
	struct list_head		devices;
	spinlock_t			devices_lock;

	struct mmu_notifier		mmu_notifier;
	struct arm_smmu_domain_virtio	virtio;
};

/* The following are exposed for testing purposes. */
struct arm_smmu_entry_writer_ops;
struct arm_smmu_entry_writer {
	const struct arm_smmu_entry_writer_ops *ops;
	struct arm_smmu_master *master;
};

struct arm_smmu_entry_writer_ops {
	void (*get_used)(const __le64 *entry, __le64 *used);
	void (*sync)(struct arm_smmu_entry_writer *writer);
};

#if IS_ENABLED(CONFIG_KUNIT)
void arm_smmu_get_ste_used(const __le64 *ent, __le64 *used_bits);
void arm_smmu_write_entry(struct arm_smmu_entry_writer *writer, __le64 *cur,
			  const __le64 *target);
void arm_smmu_get_cd_used(const __le64 *ent, __le64 *used_bits);
void arm_smmu_make_abort_ste(struct arm_smmu_ste *target);
void arm_smmu_make_bypass_ste(struct arm_smmu_device *smmu,
			      struct arm_smmu_ste *target);
void arm_smmu_make_cdtable_ste(struct arm_smmu_ste *target,
			       struct arm_smmu_master *master, bool ats_enabled,
			       unsigned int s1dss);
void arm_smmu_make_s2_domain_ste(struct arm_smmu_ste *target,
				 struct arm_smmu_master *master,
				 struct arm_smmu_domain *smmu_domain,
				 bool ats_enabled);
void arm_smmu_make_sva_cd(struct arm_smmu_cd *target,
			  struct arm_smmu_master *master, struct mm_struct *mm,
			  u16 asid);
#endif

struct arm_smmu_master_domain {
	struct list_head devices_elm;
	struct arm_smmu_master *master;
	ioasid_t ssid;
};

static inline struct arm_smmu_domain *to_smmu_domain(struct iommu_domain *dom)
{
	return container_of(dom, struct arm_smmu_domain, domain);
}

extern struct xarray arm_smmu_asid_xa;
extern struct mutex arm_smmu_asid_lock;

struct arm_smmu_domain *arm_smmu_domain_alloc(void);

void arm_smmu_clear_cd(struct arm_smmu_master *master, ioasid_t ssid);
struct arm_smmu_cd *arm_smmu_get_cd_ptr(struct arm_smmu_master *master,
					u32 ssid);
void arm_smmu_make_s1_cd(struct arm_smmu_cd *target,
			 struct arm_smmu_master *master,
			 struct arm_smmu_domain *smmu_domain);
void arm_smmu_write_cd_entry(struct arm_smmu_master *master, int ssid,
			     struct arm_smmu_cd *cdptr,
			     const struct arm_smmu_cd *target);

int arm_smmu_set_pasid(struct arm_smmu_master *master,
		       struct arm_smmu_domain *smmu_domain, ioasid_t pasid,
		       struct arm_smmu_cd *cd);

int arm_smmu_write_reg_sync(struct arm_smmu_device *smmu, u32 val,
			    unsigned int reg_off, unsigned int ack_off);
int arm_smmu_update_gbpa(struct arm_smmu_device *smmu, u32 set, u32 clr);
int arm_smmu_device_disable(struct arm_smmu_device *smmu);
struct iommu_group *arm_smmu_device_group(struct device *dev);
int arm_smmu_of_xlate(struct device *dev, const struct of_phandle_args *args);
void arm_smmu_get_resv_regions(struct device *dev,
			       struct list_head *head);

struct platform_device;
int arm_smmu_fw_probe(struct platform_device *pdev,
		      struct arm_smmu_device *smmu);
int arm_smmu_device_hw_probe(struct arm_smmu_device *smmu);
int arm_smmu_init_one_queue(struct arm_smmu_device *smmu,
			    struct arm_smmu_queue *q,
			    void __iomem *page,
			    unsigned long prod_off,
			    unsigned long cons_off,
			    size_t dwords, const char *name);
int arm_smmu_init_strtab(struct arm_smmu_device *smmu);
void arm_smmu_write_strtab_l1_desc(struct arm_smmu_strtab_l1 *dst,
				   dma_addr_t l2ptr_dma);
void arm_smmu_write_strtab(struct arm_smmu_device *smmu);

void arm_smmu_probe_irq(struct platform_device *pdev,
			struct arm_smmu_device *smmu);
void arm_smmu_setup_unique_irqs(struct arm_smmu_device *smmu,
				irqreturn_t evtqirq(int irq, void *dev),
				irqreturn_t gerrorirq(int irq, void *dev),
				irqreturn_t priirq(int irq, void *dev));

int arm_smmu_register_iommu(struct arm_smmu_device *smmu,
			    struct iommu_ops *ops, phys_addr_t ioaddr);
void arm_smmu_unregister_iommu(struct arm_smmu_device *smmu);

void arm_smmu_tlb_inv_asid(struct arm_smmu_device *smmu, u16 asid);
void arm_smmu_tlb_inv_range_asid(unsigned long iova, size_t size, int asid,
				 size_t granule, bool leaf,
				 struct arm_smmu_domain *smmu_domain);
int arm_smmu_atc_inv_domain(struct arm_smmu_domain *smmu_domain,
			    unsigned long iova, size_t size);

int arm_smmu_device_dt_probe(struct platform_device *pdev,
			     struct arm_smmu_device *smmu);
int arm_smmu_device_hw_probe(struct arm_smmu_device *smmu);
int arm_smmu_init_structures(struct arm_smmu_device *smmu);
struct arm_smmu_ste *arm_smmu_get_step_for_sid(struct arm_smmu_device *smmu, u32 sid);
extern struct iommu_ops arm_smmu_ops;
struct arm_smmu_master *
arm_smmu_find_master(struct arm_smmu_device *smmu, u32 sid);
void __arm_smmu_cmdq_skip_err(struct arm_smmu_device *smmu,
			      struct arm_smmu_cmdq *cmdq);
int arm_smmu_init_one_queue(struct arm_smmu_device *smmu,
			    struct arm_smmu_queue *q, void __iomem *page,
			    unsigned long prod_off, unsigned long cons_off,
			    size_t dwords, const char *name);
int arm_smmu_cmdq_init(struct arm_smmu_device *smmu,
		       struct arm_smmu_cmdq *cmdq);

static inline u32 smmu_idr0_features(u32 reg)
{
	u32 features = 0;

	/* 2-level structures */
	if (FIELD_GET(IDR0_ST_LVL, reg) == IDR0_ST_LVL_2LVL)
		features |= ARM_SMMU_FEAT_2_LVL_STRTAB;

	if (reg & IDR0_CD2L)
		features |= ARM_SMMU_FEAT_2_LVL_CDTAB;

	/*
	 * Translation table endianness.
	 * We currently require the same endianness as the CPU, but this
	 * could be changed later by adding a new IO_PGTABLE_QUIRK.
	 */
	switch (FIELD_GET(IDR0_TTENDIAN, reg)) {
	case IDR0_TTENDIAN_MIXED:
		features |= ARM_SMMU_FEAT_TT_LE | ARM_SMMU_FEAT_TT_BE;
		break;
#ifdef __BIG_ENDIAN
	case IDR0_TTENDIAN_BE:
		features |= ARM_SMMU_FEAT_TT_BE;
		break;
#else
	case IDR0_TTENDIAN_LE:
		features |= ARM_SMMU_FEAT_TT_LE;
		break;
#endif
	}

	/* Boolean feature flags */
	if (IS_ENABLED(CONFIG_PCI_PRI) && reg & IDR0_PRI)
		features |= ARM_SMMU_FEAT_PRI;

	if (IS_ENABLED(CONFIG_PCI_ATS) && reg & IDR0_ATS)
		features |= ARM_SMMU_FEAT_ATS;

	if (reg & IDR0_SEV)
		features |= ARM_SMMU_FEAT_SEV;

	if (reg & IDR0_MSI)
		features |= ARM_SMMU_FEAT_MSI;

	if (reg & IDR0_HYP)
		features |= ARM_SMMU_FEAT_HYP;

	switch (FIELD_GET(IDR0_STALL_MODEL, reg)) {
	case IDR0_STALL_MODEL_FORCE:
		features |= ARM_SMMU_FEAT_STALL_FORCE;
		fallthrough;
	case IDR0_STALL_MODEL_STALL:
		features |= ARM_SMMU_FEAT_STALLS;
	}

	if (reg & IDR0_S1P)
		features |= ARM_SMMU_FEAT_TRANS_S1;

	if (reg & IDR0_S2P)
		features |= ARM_SMMU_FEAT_TRANS_S2;

	return features;
}

static inline u32 smmu_idr3_features(u32 reg)
{
	u32 features = 0;

	if (FIELD_GET(IDR3_RIL, reg))
		features |= ARM_SMMU_FEAT_RANGE_INV;

	return features;
}

static inline u32 smmu_idr5_to_oas(u32 reg)
{
	switch (FIELD_GET(IDR5_OAS, reg)) {
	case IDR5_OAS_32_BIT:
		return 32;
	case IDR5_OAS_36_BIT:
		return 36;
	case IDR5_OAS_40_BIT:
		return 40;
	case IDR5_OAS_42_BIT:
		return 42;
	case IDR5_OAS_44_BIT:
		return 44;
	case IDR5_OAS_48_BIT:
		return 48;
	case IDR5_OAS_52_BIT:
		return 52;
	}
	return 0;
}

static inline unsigned long smmu_idr5_to_pgsize(u32 reg)
{
	unsigned long pgsize_bitmap = 0;

	if (reg & IDR5_GRAN64K)
		pgsize_bitmap |= SZ_64K | SZ_512M;
	if (reg & IDR5_GRAN16K)
		pgsize_bitmap |= SZ_16K | SZ_32M;
	if (reg & IDR5_GRAN4K)
		pgsize_bitmap |= SZ_4K | SZ_2M | SZ_1G;
	return pgsize_bitmap;
}

/**
 * arm_smmu_tlb_inv_build - Create a range invalidation command
 * @cmd: Base command initialized with OPCODE (S1, S2..), vmid and asid.
 * @iova: Start IOVA to invalidate
 * @size: Size of range
 * @granule: Granule of invalidation
 * @pgsize_bitmap: Page size bit map of the page table.
 * @is_range: Use range invalidation commands.
 * @opaque: Pointer to pass to add_cmd
 * @add_cmd: Function to send/batch the invalidation command
 * @cmds: Incase of batching, it includes the pointer to the batch
 */
static inline void arm_smmu_tlb_inv_build(struct arm_smmu_cmdq_ent *cmd,
					  unsigned long iova, size_t size,
					  size_t granule, unsigned long pgsize_bitmap,
					  bool is_range, void *opaque,
					  void (*add_cmd)(void *_opaque,
							  struct arm_smmu_cmdq_batch *cmds,
							  struct arm_smmu_cmdq_ent *cmd),
					  struct arm_smmu_cmdq_batch *cmds)
{
	unsigned long end = iova + size, num_pages = 0, tg = 0;
	size_t inv_range = granule;

	if (is_range) {
		/* Get the leaf page size */
		tg = __ffs(pgsize_bitmap);

		num_pages = size >> tg;

		/* Convert page size of 12,14,16 (log2) to 1,2,3 */
		cmd->tlbi.tg = (tg - 10) / 2;

		/*
		 * Determine what level the granule is at. For non-leaf, both
		 * io-pgtable and SVA pass a nominal last-level granule because
		 * they don't know what level(s) actually apply, so ignore that
		 * and leave TTL=0. However for various errata reasons we still
		 * want to use a range command, so avoid the SVA corner case
		 * where both scale and num could be 0 as well.
		 */
		if (cmd->tlbi.leaf)
			cmd->tlbi.ttl = 4 - ((ilog2(granule) - 3) / (tg - 3));
		else if ((num_pages & CMDQ_TLBI_RANGE_NUM_MAX) == 1)
			num_pages++;
	}

	while (iova < end) {
		if (is_range) {
			/*
			 * On each iteration of the loop, the range is 5 bits
			 * worth of the aligned size remaining.
			 * The range in pages is:
			 *
			 * range = (num_pages & (0x1f << __ffs(num_pages)))
			 */
			unsigned long scale, num;

			/* Determine the power of 2 multiple number of pages */
			scale = __ffs(num_pages);
			cmd->tlbi.scale = scale;

			/* Determine how many chunks of 2^scale size we have */
			num = (num_pages >> scale) & CMDQ_TLBI_RANGE_NUM_MAX;
			cmd->tlbi.num = num - 1;

			/* range is num * 2^scale * pgsize */
			inv_range = num << (scale + tg);

			/* Clear out the lower order bits for the next iteration */
			num_pages -= num << scale;
		}

		cmd->tlbi.addr = iova;
		add_cmd(opaque, cmds, cmd);
		iova += inv_range;
	}
}

#ifdef CONFIG_ARM_SMMU_V3_SVA
bool arm_smmu_sva_supported(struct arm_smmu_device *smmu);
bool arm_smmu_master_sva_supported(struct arm_smmu_master *master);
bool arm_smmu_master_sva_enabled(struct arm_smmu_master *master);
int arm_smmu_master_enable_sva(struct arm_smmu_master *master);
int arm_smmu_master_disable_sva(struct arm_smmu_master *master);
bool arm_smmu_master_iopf_supported(struct arm_smmu_master *master);
void arm_smmu_sva_notifier_synchronize(void);
struct iommu_domain *arm_smmu_sva_domain_alloc(struct device *dev,
					       struct mm_struct *mm);
#else /* CONFIG_ARM_SMMU_V3_SVA */
static inline bool arm_smmu_sva_supported(struct arm_smmu_device *smmu)
{
	return false;
}

static inline bool arm_smmu_master_sva_supported(struct arm_smmu_master *master)
{
	return false;
}

static inline bool arm_smmu_master_sva_enabled(struct arm_smmu_master *master)
{
	return false;
}

static inline int arm_smmu_master_enable_sva(struct arm_smmu_master *master)
{
	return -ENODEV;
}

static inline int arm_smmu_master_disable_sva(struct arm_smmu_master *master)
{
	return -ENODEV;
}

static inline bool arm_smmu_master_iopf_supported(struct arm_smmu_master *master)
{
	return false;
}

static inline void arm_smmu_sva_notifier_synchronize(void) {}

#define arm_smmu_sva_domain_alloc NULL

#endif /* CONFIG_ARM_SMMU_V3_SVA */

#ifdef CONFIG_ARM_SMMU_V3_QCOM_VIRTIO
extern struct platform_driver arm_vsmmu_driver;
int arm_smmu_qcom_virtio_init(void);
void arm_smmu_qcom_virtio_exit(void);
#else /* CONFIG_ARM_SMMU_V3_QCOM_VIRTIO */
static inline int arm_smmu_qcom_virtio_init(void) {return 0; }
static inline void arm_smmu_qcom_virtio_exit(void) {}
#endif /* CONFIG_ARM_SMMU_V3_QCOM_VIRTIO */

#ifdef CONFIG_TEGRA241_CMDQV
struct arm_smmu_device *tegra241_cmdqv_probe(struct arm_smmu_device *smmu);
#else /* CONFIG_TEGRA241_CMDQV */
static inline struct arm_smmu_device *
tegra241_cmdqv_probe(struct arm_smmu_device *smmu)
{
	return ERR_PTR(-ENODEV);
}
#endif /* CONFIG_TEGRA241_CMDQV */

/* Queue functions shared with common and kernel drivers */
static inline u32 queue_space(struct arm_smmu_ll_queue *q)
{
	u32 prod, cons;

	prod = Q_IDX(q, q->prod);
	cons = Q_IDX(q, q->cons);

	if (Q_WRP(q, q->prod) == Q_WRP(q, q->cons))
		return (1 << q->max_n_shift) - (prod - cons);
	return cons - prod;
}

static inline bool queue_has_space(struct arm_smmu_ll_queue *q, u32 n)
{
	return queue_space(q) >= n;
}

static bool __maybe_unused queue_full(struct arm_smmu_ll_queue *q)
{
	return Q_IDX(q, q->prod) == Q_IDX(q, q->cons) &&
	       Q_WRP(q, q->prod) != Q_WRP(q, q->cons);
}

static bool __maybe_unused queue_empty(struct arm_smmu_ll_queue *q)
{
	return Q_IDX(q, q->prod) == Q_IDX(q, q->cons) &&
	       Q_WRP(q, q->prod) == Q_WRP(q, q->cons);
}

static bool __maybe_unused queue_consumed(struct arm_smmu_ll_queue *q, u32 prod)
{
	return ((Q_WRP(q, q->cons) == Q_WRP(q, prod)) &&
		(Q_IDX(q, q->cons) > Q_IDX(q, prod))) ||
	       ((Q_WRP(q, q->cons) != Q_WRP(q, prod)) &&
		(Q_IDX(q, q->cons) <= Q_IDX(q, prod)));
}

static void __maybe_unused queue_sync_cons_out(struct arm_smmu_queue *q)
{
	/*
	 * Ensure that all CPU accesses (reads and writes) to the queue
	 * are complete before we update the cons pointer.
	 */
	__iomb();
	writel_relaxed(q->llq.cons, q->cons_reg);
}

static void __maybe_unused queue_inc_cons(struct arm_smmu_ll_queue *q)
{
	u32 cons = (Q_WRP(q, q->cons) | Q_IDX(q, q->cons)) + 1;

	q->cons = Q_OVF(q->cons) | Q_WRP(q, cons) | Q_IDX(q, cons);
}

static void __maybe_unused queue_sync_cons_ovf(struct arm_smmu_queue *q)
{
	struct arm_smmu_ll_queue *llq = &q->llq;

	if (likely(Q_OVF(llq->prod) == Q_OVF(llq->cons)))
		return;

	llq->cons = Q_OVF(llq->prod) | Q_WRP(llq, llq->cons) |
		    Q_IDX(llq, llq->cons);
	queue_sync_cons_out(q);
}

static int __maybe_unused queue_sync_prod_in(struct arm_smmu_queue *q)
{
	u32 prod;
	int ret = 0;

	/*
	 * We can't use the _relaxed() variant here, as we must prevent
	 * speculative reads of the queue before we have determined that
	 * prod has indeed moved.
	 */
	prod = readl(q->prod_reg);

	if (Q_OVF(prod) != Q_OVF(q->llq.prod))
		ret = -EOVERFLOW;

	q->llq.prod = prod;
	return ret;
}

static u32 __maybe_unused queue_inc_prod_n(struct arm_smmu_ll_queue *q, int n)
{
	u32 prod = (Q_WRP(q, q->prod) | Q_IDX(q, q->prod)) + n;

	return Q_OVF(q->prod) | Q_WRP(q, prod) | Q_IDX(q, prod);
}

static void __maybe_unused queue_poll_init(struct arm_smmu_device *smmu,
					   struct arm_smmu_queue_poll *qp)
{
	qp->delay = 1;
	qp->spin_cnt = 0;
	qp->wfe = !!(smmu->features & ARM_SMMU_FEAT_SEV);
	qp->timeout = ktime_add_us(ktime_get(), ARM_SMMU_POLL_TIMEOUT_US);
}

static int __maybe_unused queue_poll(struct arm_smmu_queue_poll *qp)
{
	if (ktime_compare(ktime_get(), qp->timeout) > 0)
		return -ETIMEDOUT;

	if (qp->wfe) {
		wfe();
	} else if (++qp->spin_cnt < ARM_SMMU_POLL_SPIN_COUNT) {
		cpu_relax();
	} else {
		udelay(qp->delay);
		qp->delay *= 2;
		qp->spin_cnt = 0;
	}

	return 0;
}

static void __maybe_unused queue_write(__le64 *dst, u64 *src, size_t n_dwords)
{
	int i;

	for (i = 0; i < n_dwords; ++i)
		*dst++ = cpu_to_le64(*src++);
}

static void __maybe_unused queue_read(u64 *dst, __le64 *src, size_t n_dwords)
{
	int i;

	for (i = 0; i < n_dwords; ++i)
		*dst++ = le64_to_cpu(*src++);
}

static int __maybe_unused queue_remove_raw(struct arm_smmu_queue *q, u64 *ent)
{
	if (queue_empty(&q->llq))
		return -EAGAIN;

	queue_read(ent, Q_ENT(q, q->llq.cons), q->ent_dwords);
	queue_inc_cons(&q->llq);
	queue_sync_cons_out(q);
	return 0;
}

enum arm_smmu_msi_index {
	EVTQ_MSI_INDEX,
	GERROR_MSI_INDEX,
	PRIQ_MSI_INDEX,
	ARM_SMMU_MAX_MSIS,
};

int arm_smmu_cmdq_build_cmd(u64 *cmd, struct arm_smmu_cmdq_ent *ent);

#endif /* _ARM_SMMU_V3_H */
