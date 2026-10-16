/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __GH_DEVICES_H
#define __GH_DEVICES_H

#include <linux/errno.h>

#include "gh_common.h"

#define GH_DEV_RSC_TYPE_IOMEM		1
#define GH_DEV_RSC_TYPE_IRQ		2
#define GH_DEV_RSC_TYPE_IOMMU_ENDPOINT	3
#define GH_DEV_RSC_TYPE_MSI_ENDPOINT	4
#define GH_DEV_RSC_TYPE_PCIE_FUNCTION	5

#define GH_DEV_HANDLE_INVALID		U32_MAX

/* Flags */
#define GH_DEV_ACCEPT_RESET		BIT(0)
#define GH_DEV_ACCEPT_BIND_TO_BUS	BIT(1)
#define GH_DEV_LEND_RESET		BIT(0)
#define GH_DEV_RELEASE_RESET		BIT(0)
#define GH_DEV_RECLAIM_RESET		BIT(0)
#define GH_DEV_GET_RESOURCES_PA		BIT(0)

/**
 * struct gh_dev_iomem_rsc_desc - A struct describing the iomemory
 * region corresponding to a device.
 * @size: Size of the iomemory region.
 * @base_addr: Base address of the iomemory region.
 */
struct gh_dev_iomem_rsc_desc {
	u32 size;
	u64 base_addr;
};

/**
 * struct gh_dev_irq_rsc_desc - A struct describing the irq
 * corresponding to a device.
 * @irq: Interrupt number.
 */
struct gh_dev_irq_rsc_desc {
	u32 irq;
};

/**
 * struct gh_dev_iommu_rsc_desc - A struct describing the iommu
 * resource corresponding to a device.
 * @iommu_hdl: Handle of the IOMMU. For fetching this you might need to
 *             use gh_dev_find_handle_*() for IOMMU itself.
 * @endpt_id_base: Endpoint ID base of the device.
 * @endpt_id_count: Endpoint ID count of the device.
 */
struct gh_dev_iommu_rsc_desc {
	u32 iommu_hdl;
	u32 endpt_id_base;
	u32 endpt_id_count;
};

/**
 * struct gh_dev_msi_rsc_desc - A struct describing the MSI
 * resource corresponding to a device.
 * @rtr_hdl: Handle of the MSI router. For fetching this you might need to
 *             use gh_dev_find_handle_*() for MSI router itself.
 * @endpt_id_base: Endpoint ID base of the device.
 * @endpt_id_count: Endpoint ID count of the device.
 */
struct gh_dev_msi_rsc_desc {
	u32 rtr_hdl;
	u32 endpt_id_base;
	u32 endpt_id_count;
};

/**
 * struct gh_dev_pcie_rsc_desc - A struct describing the PCIe
 * resource corresponding to a device.
 * @responder_id: Responder ID of the device.
 * @rc_hdl: Handle of the PCIe root complex. For fetching this you might need to
 *             use gh_dev_find_handle_*() for PCIe root complex itself.
 */
struct gh_dev_pcie_rsc_desc {
	u16 responder_id;
	u32 rc_hdl;
};

/**
 * struct gh_dev_resources - A struct describing all types of resources
 * corresponding to a device.
 *
 * This struct will be allocated within and returned by
 * gh_dev_get_resources(). Freeing up the memory allocated within the
 * struct is the responsibility of the caller. The caller should call
 * gh_dev_free_resources() explicitly to free up the memory.
 */
struct gh_dev_resources {
	int n_iomem;
	struct gh_dev_iomem_rsc_desc *iomem_rsc_desc;
	int n_irq;
	struct gh_dev_irq_rsc_desc *irq_rsc_desc;
	int n_iommu;
	struct gh_dev_iommu_rsc_desc *iommu_rsc_desc;
	int n_msi;
	struct gh_dev_msi_rsc_desc *msi_rsc_desc;
	int n_pcie;
	struct gh_dev_pcie_rsc_desc *pcie_rsc_desc;
};

gh_dev_handle_t gh_device_find_handle_by_iomem(struct gh_dev_iomem_rsc_desc *iomem_rsc_desc);
gh_dev_handle_t gh_device_find_handle_by_irq(struct gh_dev_irq_rsc_desc *irq_rsc_desc);
gh_dev_handle_t gh_device_find_handle_by_iommu(struct gh_dev_iommu_rsc_desc *iommu_rsc_desc);
gh_dev_handle_t gh_device_find_handle_by_msi(struct gh_dev_msi_rsc_desc *msi_rsc_desc);
gh_dev_handle_t gh_device_find_handle_by_pcie(struct gh_dev_pcie_rsc_desc *pcie_rsc_desc);
struct gh_dev_resources *gh_device_get_resources(gh_dev_handle_t dev_hdl, u8 flags);
void gh_device_free_resources(struct gh_dev_resources *rsc);
int gh_device_accept(gh_dev_handle_t dev_hdl, u8 flags, gh_dev_handle_t bus_hdl);
int gh_device_lend(gh_dev_handle_t dev_hdl, gh_vmid_t vmid, u8 flags);
int gh_device_release(gh_dev_handle_t dev_hdl, u8 flags);
int gh_device_reclaim(gh_dev_handle_t dev_hdl, u8 flags);
int gh_device_bus_lockdown(gh_dev_handle_t dev_hdl);
int gh_device_bus_unlock(gh_dev_handle_t dev_hdl);

#endif
