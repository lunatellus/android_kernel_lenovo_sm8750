// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/limits.h>
#include <linux/module.h>
#include <linux/gunyah/gh_devices.h>

#include "gh_rm_drv_private.h"

#define GH_FILL_IOMEM_RSC_DESC(rsc_desc, _size, _base_addr) { \
	(rsc_desc).iomem.type = GH_DEV_RSC_TYPE_IOMEM; \
	(rsc_desc).iomem.size = cpu_to_le32(_size); \
	(rsc_desc).iomem.base_addr = cpu_to_le64(_base_addr); \
}

#define GH_FILL_IRQ_RSC_DESC(rsc_desc, _irq) { \
	(rsc_desc).irq.type = GH_DEV_RSC_TYPE_IRQ; \
	(rsc_desc).irq.irq = cpu_to_le32(_irq); \
}

#define GH_FILL_IOMMU_RSC_DESC(rsc_desc, _iommu_hdl, _endpt_id_base, _endpt_id_count) { \
	(rsc_desc).iommu.type = GH_DEV_RSC_TYPE_IOMMU_ENDPOINT; \
	(rsc_desc).iommu.iommu_hdl = cpu_to_le32(_iommu_hdl); \
	(rsc_desc).iommu.endpt_id_base = cpu_to_le32(_endpt_id_base); \
	(rsc_desc).iommu.endpt_id_count = cpu_to_le32(_endpt_id_count); \
}

#define GH_FILL_MSI_RSC_DESC(rsc_desc, _rtr_hdl, _endpt_id_base, _endpt_id_count) { \
	(rsc_desc).msi.type = GH_DEV_RSC_TYPE_MSI_ENDPOINT; \
	(rsc_desc).msi.rtr_hdl = cpu_to_le32(_rtr_hdl); \
	(rsc_desc).msi.endpt_id_base = cpu_to_le32(_endpt_id_base); \
	(rsc_desc).msi.endpt_id_count = cpu_to_le32(_endpt_id_count); \
}

#define GH_FILL_PCIE_RSC_DESC(rsc_desc, _rc_hdl, _responder_id) { \
	(rsc_desc).pcie.type = GH_DEV_RSC_TYPE_PCIE_FUNCTION; \
	(rsc_desc).pcie.rc_hdl = cpu_to_le32(_rc_hdl); \
	(rsc_desc).pcie.responder_id = cpu_to_le16(_responder_id); \
}

/**
 * gh_device_find_handle_by_iomem() - Find device handle that matches the
 * provided iomemory register region.
 * @iomem_rsc_desc: struct containing the size and base address of the
 *                  iomemory region.
 *
 * Returns: GH_DEV_HANDLE_INVALID if the RM call fails, device handle of
 *          the matching device otherwise.
 */
gh_dev_handle_t gh_device_find_handle_by_iomem(struct gh_dev_iomem_rsc_desc *iomem_rsc_desc)
{
	gh_dev_handle_t hdl = GH_DEV_HANDLE_INVALID;
	gh_dev_rsc_desc rsc_desc = {0};
	int ret;

	if (!iomem_rsc_desc)
		return GH_DEV_HANDLE_INVALID;

	GH_FILL_IOMEM_RSC_DESC(rsc_desc, iomem_rsc_desc->size, iomem_rsc_desc->base_addr);

	ret = gh_rm_device_find_handle(&rsc_desc, &hdl);
	if (ret) {
		pr_err("%s: Failed to get device handle by iomem ret=%d\n", __func__, ret);
		return GH_DEV_HANDLE_INVALID;
	}

	return hdl;
}
EXPORT_SYMBOL_GPL(gh_device_find_handle_by_iomem);

/**
 * gh_device_find_handle_by_irq() - Find device handle that matches the
 * provided irq number.
 * @irq_rsc_desc: struct containing the irq number.
 *
 * Returns: GH_DEV_HANDLE_INVALID if the RM RPC fails, device handle of
 *          the matching device otherwise.
 */
gh_dev_handle_t gh_device_find_handle_by_irq(struct gh_dev_irq_rsc_desc *irq_rsc_desc)
{
	gh_dev_handle_t hdl = GH_DEV_HANDLE_INVALID;
	gh_dev_rsc_desc rsc_desc = {0};
	int ret;

	if (!irq_rsc_desc)
		return GH_DEV_HANDLE_INVALID;

	GH_FILL_IRQ_RSC_DESC(rsc_desc, irq_rsc_desc->irq);

	ret = gh_rm_device_find_handle(&rsc_desc, &hdl);
	if (ret) {
		pr_err("%s: Failed to get device handle by irq ret=%d\n", __func__, ret);
		return GH_DEV_HANDLE_INVALID;
	}

	return hdl;
}
EXPORT_SYMBOL_GPL(gh_device_find_handle_by_irq);

/**
 * gh_device_find_handle_by_iommu() - Find device handle that matches the
 * provided iommu endpoint.
 * @iommu_rsc_desc: struct containing the IOMMU handle, endpoint id base
 *                  and endpoint id count.
 *
 * To fetch the handle of the IOMMU, you'll need to call
 * gh_device_find_handle_*() separately for IOMMU, then use it here to find
 * the device handle of your device.
 *
 * Returns: GH_DEV_HANDLE_INVALID if the RM RPC fails, device handle of
 *          the matching device otherwise.
 */
gh_dev_handle_t gh_device_find_handle_by_iommu(struct gh_dev_iommu_rsc_desc *iommu_rsc_desc)
{
	gh_dev_handle_t hdl = GH_DEV_HANDLE_INVALID;
	gh_dev_rsc_desc rsc_desc = {0};
	int ret;

	if (!iommu_rsc_desc)
		return GH_DEV_HANDLE_INVALID;

	GH_FILL_IOMMU_RSC_DESC(rsc_desc, iommu_rsc_desc->iommu_hdl,
			  iommu_rsc_desc->endpt_id_base, iommu_rsc_desc->endpt_id_count);

	ret = gh_rm_device_find_handle(&rsc_desc, &hdl);
	if (ret) {
		pr_err("%s: Failed to get device handle by iommu endpoint ret=%d\n", __func__, ret);
		return GH_DEV_HANDLE_INVALID;
	}

	return hdl;
}
EXPORT_SYMBOL_GPL(gh_device_find_handle_by_iommu);

/**
 * gh_device_find_handle_by_msi() - Find device handle that matches the
 * provided msi endpoint.
 * @msi_rsc_desc: struct containing the msi router handle, endpoint id base
 *                and endpoint id count.
 *
 * To fetch the handle of the MSI Router, you'll need to call
 * gh_device_find_handle_*() separately for MSI Router, then use it here to
 * find the device handle of your device.
 *
 * Returns: GH_DEV_HANDLE_INVALID if the RM RPC fails, device handle of
 *          the matching device otherwise.
 */
gh_dev_handle_t gh_device_find_handle_by_msi(struct gh_dev_msi_rsc_desc *msi_rsc_desc)
{
	gh_dev_handle_t hdl = GH_DEV_HANDLE_INVALID;
	gh_dev_rsc_desc rsc_desc = {0};
	int ret;

	if (!msi_rsc_desc)
		return GH_DEV_HANDLE_INVALID;

	GH_FILL_MSI_RSC_DESC(rsc_desc, msi_rsc_desc->rtr_hdl,
			  msi_rsc_desc->endpt_id_base, msi_rsc_desc->endpt_id_count);

	ret = gh_rm_device_find_handle(&rsc_desc, &hdl);
	if (ret) {
		pr_err("%s: Failed to get device handle by msi endpoint ret=%d\n", __func__, ret);
		return GH_DEV_HANDLE_INVALID;
	}

	return hdl;
}
EXPORT_SYMBOL_GPL(gh_device_find_handle_by_msi);

/**
 * gh_device_find_handle_by_pcie() - Find device handle that matches the
 * provided pcie function.
 * @pcie_rsc_desc: struct containing the PCIe responder id and
 *                 PCIe root complex handle.
 *
 * To fetch the handle of the PCIe Root Complex, you'll need to call
 * gh_device_find_handle_*() separately for PCIe Root Complex, then use it
 * here to find the device handle of your device.
 *
 * Returns: GH_DEV_HANDLE_INVALID if the RM RPC fails, device handle of
 *          the matching device otherwise.
 */
gh_dev_handle_t gh_device_find_handle_by_pcie(struct gh_dev_pcie_rsc_desc *pcie_rsc_desc)
{
	gh_dev_handle_t hdl = GH_DEV_HANDLE_INVALID;
	gh_dev_rsc_desc rsc_desc = {0};
	int ret;

	if (!pcie_rsc_desc)
		return GH_DEV_HANDLE_INVALID;

	GH_FILL_PCIE_RSC_DESC(rsc_desc, pcie_rsc_desc->rc_hdl, pcie_rsc_desc->responder_id);

	ret = gh_rm_device_find_handle(&rsc_desc, &hdl);
	if (ret) {
		pr_err("%s: Failed to get device handle by pcie function ret=%d\n", __func__, ret);
		return GH_DEV_HANDLE_INVALID;
	}

	return hdl;
}
EXPORT_SYMBOL_GPL(gh_device_find_handle_by_pcie);

/**
 * gh_device_get_resources(): Get all resources like mmio register region,
 * irq, iommu etc corresponding to a device handle.
 * @dev_hdl: Device handle of the device.
 * @flags: If GH_DEV_GET_RESOURCES_PA is set, returns device's physical resources,
 *	   otherwise returns device's virtual resources.
 *
 * Returns: A struct containing arrays of resources corresponding to the
 *          device handle.
 *
 * gh_device_free_resources() needs to be called by the caller of this
 * function to properly free up the memory allocated in the struct
 * reference returned by the function.
 */
struct gh_dev_resources *gh_device_get_resources(gh_dev_handle_t dev_hdl, u8 flags)
{
	gh_dev_rsc_desc *rsc_desc;
	struct gh_dev_resources *rsc;
	void *rsc_buf = NULL;
	unsigned int n_rsc = 0, n_iomem = 0, n_irq = 0;
	unsigned int n_iommu = 0, n_msi = 0, n_pcie = 0;
	int r;

	if (!dev_hdl || dev_hdl == GH_DEV_HANDLE_INVALID)
		return ERR_PTR(-EINVAL);

	rsc_buf = gh_rm_device_get_resources(dev_hdl, flags, &n_rsc);
	if (IS_ERR(rsc_buf)) {
		pr_err("%s: Failed to get resources for the requested device\n", __func__);
		return rsc_buf;
	}

	rsc_desc = rsc_buf;
	for (r = 0; r < n_rsc; r++) {
		switch (rsc_desc->iomem.type) {
		case GH_DEV_RSC_TYPE_IOMEM:
			n_iomem++;
			break;
		case GH_DEV_RSC_TYPE_IRQ:
			n_irq++;
			break;
		case GH_DEV_RSC_TYPE_IOMMU_ENDPOINT:
			n_iommu++;
			break;
		case GH_DEV_RSC_TYPE_MSI_ENDPOINT:
			n_msi++;
			break;
		case GH_DEV_RSC_TYPE_PCIE_FUNCTION:
			n_pcie++;
			break;
		}
		rsc_desc++;
	}

	rsc = kzalloc(sizeof(*rsc), GFP_KERNEL);
	if (!rsc)
		return ERR_PTR(-ENOMEM);

	rsc->iomem_rsc_desc = kmalloc_array(n_iomem,
			sizeof(struct gh_dev_iomem_rsc_desc), GFP_KERNEL);
	if (!rsc->iomem_rsc_desc)
		goto free_rsc;

	rsc->irq_rsc_desc = kmalloc_array(n_irq,
			sizeof(struct gh_dev_irq_rsc_desc), GFP_KERNEL);
	if (!rsc->irq_rsc_desc)
		goto free_rsc;

	rsc->iommu_rsc_desc = kmalloc_array(n_iommu,
			sizeof(struct gh_dev_iommu_rsc_desc), GFP_KERNEL);
	if (!rsc->iommu_rsc_desc)
		goto free_rsc;

	rsc->msi_rsc_desc = kmalloc_array(n_msi,
			sizeof(struct gh_dev_msi_rsc_desc), GFP_KERNEL);
	if (!rsc->msi_rsc_desc)
		goto free_rsc;

	rsc->pcie_rsc_desc = kmalloc_array(n_pcie,
			sizeof(struct gh_dev_pcie_rsc_desc), GFP_KERNEL);
	if (!rsc->pcie_rsc_desc)
		goto free_rsc;

	rsc->n_iomem = n_iomem;
	rsc->n_irq = n_irq;
	rsc->n_iommu = n_iommu;
	rsc->n_msi = n_msi;
	rsc->n_pcie = n_pcie;

	rsc_desc = rsc_buf;
	for (r = 0; r < n_rsc; r++) {
		switch (rsc_desc->iomem.type) {
		case GH_DEV_RSC_TYPE_IOMEM:
			n_iomem--;
			rsc->iomem_rsc_desc[n_iomem].base_addr =
				le64_to_cpu(rsc_desc->iomem.base_addr);
			rsc->iomem_rsc_desc[n_iomem].size =
				le32_to_cpu(rsc_desc->iomem.size);
			break;
		case GH_DEV_RSC_TYPE_IRQ:
			n_irq--;
			rsc->irq_rsc_desc[n_irq].irq =
				le32_to_cpu(rsc_desc->irq.irq);
			break;
		case GH_DEV_RSC_TYPE_IOMMU_ENDPOINT:
			n_iommu--;
			rsc->iommu_rsc_desc[n_iommu].iommu_hdl =
				le32_to_cpu(rsc_desc->iommu.iommu_hdl);
			rsc->iommu_rsc_desc[n_iommu].endpt_id_base =
				le32_to_cpu(rsc_desc->iommu.endpt_id_base);
			rsc->iommu_rsc_desc[n_iommu].endpt_id_count =
				le32_to_cpu(rsc_desc->iommu.endpt_id_count);
			break;
		case GH_DEV_RSC_TYPE_MSI_ENDPOINT:
			n_msi--;
			rsc->msi_rsc_desc[n_msi].rtr_hdl =
				le32_to_cpu(rsc_desc->msi.rtr_hdl);
			rsc->msi_rsc_desc[n_msi].endpt_id_base =
				le32_to_cpu(rsc_desc->msi.endpt_id_base);
			rsc->msi_rsc_desc[n_msi].endpt_id_count =
				le32_to_cpu(rsc_desc->msi.endpt_id_count);
			break;
		case GH_DEV_RSC_TYPE_PCIE_FUNCTION:
			n_pcie--;
			rsc->pcie_rsc_desc[n_pcie].responder_id =
				le16_to_cpu(rsc_desc->pcie.responder_id);
			rsc->pcie_rsc_desc[n_pcie].rc_hdl =
				le32_to_cpu(rsc_desc->pcie.rc_hdl);
			break;
		}
		rsc_desc++;
	}

	kfree(rsc_buf);
	return rsc;

free_rsc:
	kfree(rsc_buf);
	gh_device_free_resources(rsc);
	return ERR_PTR(-ENOMEM);
}
EXPORT_SYMBOL_GPL(gh_device_get_resources);

/**
 * gh_device_free_resources() - kfree the resource data dynamically allocated
 * in gh_device_get_resources.
 * @rsc: struct gh_dev_resources containing the data to be kfreed.
 */
void gh_device_free_resources(struct gh_dev_resources *rsc)
{
	if (!rsc)
		return;

	kfree(rsc->iomem_rsc_desc);
	kfree(rsc->irq_rsc_desc);
	kfree(rsc->iommu_rsc_desc);
	kfree(rsc->msi_rsc_desc);
	kfree(rsc->pcie_rsc_desc);
	kfree(rsc);
}
EXPORT_SYMBOL_GPL(gh_device_free_resources);

/**
 * gh_device_accept() - Attach the specified device to caller's IO address space.
 * @dev_hdl: Handle for the device to accept.
 * @flags: If GH_DEV_ACCEPT_RESET is set, resets the device before accepting.
 *	   If GH_DEV_ACCEPT_BIND_TO_BUS is set, binds the device to the bus.
 */
int gh_device_accept(gh_dev_handle_t dev_hdl, u8 flags, gh_dev_handle_t bus_hdl)
{
	int ret = 0;

	if (!dev_hdl || dev_hdl == GH_DEV_HANDLE_INVALID || bus_hdl == GH_DEV_HANDLE_INVALID)
		return -EINVAL;

	ret = gh_rm_device_accept(dev_hdl, flags, bus_hdl);
	if (ret)
		pr_err("%s: Failed to accept device ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(gh_device_accept);

/**
 * gh_device_lend() - Lend the specified device from caller's IO address space.
 * @dev_hdl: Handle for the device to lend.
 * @vmid: Recipient of the lent device.
 * @flags: If GH_DEV_LEND_RESET is set, resets the device after unmapping.
 */
int gh_device_lend(gh_dev_handle_t dev_hdl, gh_vmid_t vmid, u8 flags)
{
	int ret = 0;

	if (!dev_hdl || dev_hdl == GH_DEV_HANDLE_INVALID)
		return -EINVAL;

	ret = gh_rm_device_lend(dev_hdl, vmid, flags);
	if (ret)
		pr_err("%s: Failed to lend device ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(gh_device_lend);

/**
 * gh_device_release() - Release the previously accepted device.
 * @dev_hdl: Handle for the device to release.
 * @flags: If GH_DEV_RELEASE_RESET is set, resets the device after unmapping.
 */
int gh_device_release(gh_dev_handle_t dev_hdl, u8 flags)
{
	int ret = 0;

	if (!dev_hdl || dev_hdl == GH_DEV_HANDLE_INVALID)
		return -EINVAL;

	ret = gh_rm_device_release(dev_hdl, flags);
	if (ret)
		pr_err("%s: Failed to release device ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(gh_device_release);

/**
 * gh_device_reclaim() - Reclaim the previously lent device.
 * @dev_hdl: Handle for the device to reclaim.
 * @flags: If GH_DEV_RECLAIM_RESET is set, resets the device before reclaiming.
 */
int gh_device_reclaim(gh_dev_handle_t dev_hdl, u8 flags)
{
	int ret = 0;

	if (!dev_hdl || dev_hdl == GH_DEV_HANDLE_INVALID)
		return -EINVAL;

	ret = gh_rm_device_reclaim(dev_hdl, flags);
	if (ret)
		pr_err("%s: Failed to reclaim device ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(gh_device_reclaim);

/**
 * gh_device_bus_lockdown() - Lockdown the bus.
 * @dev_hdl: Device handle to the bus to be locked down and enumerated.
 */
int gh_device_bus_lockdown(gh_dev_handle_t dev_hdl)
{
	int ret = 0;

	if (!dev_hdl || dev_hdl == GH_DEV_HANDLE_INVALID)
		return -EINVAL;

	ret = gh_rm_device_bus_lockdown(dev_hdl);
	if (ret)
		pr_err("%s: Failed to lockdown bus ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(gh_device_bus_lockdown);

/**
 * gh_device_bus_unlock() - Unlock the bus.
 * @dev_hdl: Device handle to the bus to be unlocked.
 */
int gh_device_bus_unlock(gh_dev_handle_t dev_hdl)
{
	int ret = 0;

	if (!dev_hdl || dev_hdl == GH_DEV_HANDLE_INVALID)
		return -EINVAL;

	ret = gh_rm_device_bus_unlock(dev_hdl);
	if (ret)
		pr_err("%s: Failed to unlock bus ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(gh_device_bus_unlock);
