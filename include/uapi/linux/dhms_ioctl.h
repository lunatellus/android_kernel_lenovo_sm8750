/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * DHMS — UAPI IOCTL definitions
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This file is safe to include from both kernel drivers and userspace
 * applications. It defines the IOCTL interface for /dev/dhms_<label>.
 *
 * Userspace usage:
 *   #include <linux/dhms_ioctl.h>
 *
 * Kernel usage (via dhms_rpmsg.h):
 *   #include <uapi/linux/dhms_ioctl.h>
 */

#ifndef __UAPI_DHMS_IOCTL_H__
#define __UAPI_DHMS_IOCTL_H__

#include <linux/ioctl.h>
#include <linux/types.h>

/**
 * struct dhms_dmabuf_import_data - DHMS_IOC_MAP request/response
 *
 * Passed to DHMS_IOC_MAP ioctl. On entry, dmabuf_fd and flags are
 * filled by the caller. On return, client_id is filled by the kernel
 * with the dynamically allocated buffer identifier.
 *
 * @dmabuf_fd: [in]  DMA-BUF file descriptor obtained from the allocator
 *                   (e.g., ION, dma_heap_alloc). Must be a valid fd.
 * @client_id: [out] Dynamically generated client ID returned by the kernel.
 *                   Use this value in subsequent DHMS_IOC_UNMAP calls.
 * @flags:     [in]  Reserved for future use — pass 0.
 * @reserved:  [in]  Reserved — must be 0.
 */
struct dhms_dmabuf_import_data {
	__s32  dmabuf_fd;
	__u64  client_id;
	__u32  flags;
	__u32  reserved;
};

/*
 * IOCTL magic number for DHMS device.
 * 'D' is used — verify no conflict with other drivers in your tree.
 */
#define DHMS_IOC_MAGIC  'D'

/**
 * DHMS_IOC_MAP - Import a DMA-BUF, map to SMMU IOVA, notify remote processors
 *
 * The kernel will:
 *   1. Resolve the fd to a dma_buf
 *   2. Attach and map to the DHMS IOMMU domain → get IOVA
 *   3. Allocate a dynamic client_id
 *   4. Broadcast IMPORT message (with IOVA + size) to all connected processors
 *   5. Return client_id to userspace
 *
 * Returns 0 on success, negative errno on failure.
 */
#define DHMS_IOC_MAP    _IOWR(DHMS_IOC_MAGIC, 1, struct dhms_dmabuf_import_data)

/**
 * DHMS_IOC_UNMAP - Release a mapped buffer by client_id, notify remote processors
 *
 * The kernel will:
 *   1. Look up the buffer by client_id
 *   2. Remove from IDR (no new lookups possible)
 *   3. Broadcast RELEASE message to all processors that received the IMPORT
 *   4. Free the IOVA mapping immediately
 *   5. Free the buffer struct after all RELEASE_ACKs are received
 *
 * Returns 0 on success, -EINVAL if client_id not found.
 */
#define DHMS_IOC_UNMAP  _IOW(DHMS_IOC_MAGIC,  2, struct dhms_dmabuf_import_data)

#endif /* __UAPI_DHMS_IOCTL_H__ */
