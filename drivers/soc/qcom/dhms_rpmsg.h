/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * DHMS — Kernel-internal driver definitions
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 */

#ifndef __DHMS_RPMSG_H__
#define __DHMS_RPMSG_H__

#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/idr.h>
#include <linux/cdev.h>
#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/rpmsg.h>
#include <linux/dma-buf.h>

/* Pull in the userspace-facing IOCTL definitions */
#include <linux/dhms_ioctl.h>

/* ================================================================
 * Wire protocol — GLink/RPMSG message layout
 * ================================================================
 */

/**
 * enum dhms_msg_type - Message types exchanged over GLink/RPMSG
 *
 * This enum defines all message types used in the DHMS wire protocol
 * between the AP kernel and remote processors over GLink/RPMSG.
 *
 * @DHMS_MSG_IMPORT: Host → remote: buffer mapped, IOVA valid
 * @DHMS_MSG_RELEASE: Host → remote: buffer released, IOVA invalid
 * @DHMS_MSG_IMPORT_ACK: remote → Host: import confirmed
 * @DHMS_MSG_RELEASE_ACK: remote → Host: release confirmed
 * @DHMS_MSG_QUERY_REQ: remote → Host: query IOVA for a client_id
 * @DHMS_MSG_QUERY_RSP: Host → remote: IOVA response
 * @DHMS_MSG_QUERY_RSP_ACK: remote → Host: query response acknowledged
 */
enum dhms_msg_type {
	DHMS_MSG_IMPORT          = 1,
	DHMS_MSG_RELEASE         = 2,
	DHMS_MSG_IMPORT_ACK      = 3,
	DHMS_MSG_RELEASE_ACK     = 4,
	DHMS_MSG_QUERY_REQ       = 5,
	DHMS_MSG_QUERY_RSP       = 6,
	DHMS_MSG_QUERY_RSP_ACK   = 7,
};

/**
 * enum dhms_op_status - Operation status codes for DHMS response messages
 *
 * This enum defines the status codes carried in the @status field of
 * dhms_glink_msg. Set by HLOS in response messages (QUERY_RSP, RELEASE_ACK)
 * to inform the remote processor of the outcome.
 *
 * @DHMS_STATUS_OK: Operation succeeded.
 * @DHMS_STATUS_CLIENT_NOT_FOUND: client_id not found in HLOS bookkeeping.
 *   Returned in QUERY_RSP when the requested client_id does not exist.
 * @DHMS_STATUS_CLIENT_ALREADY_RELEASED: client_id was already released before
 *   this request arrived. Returned in RELEASE_ACK when the buffer had already
 *   been freed by userspace UNMAP or another processor's RELEASE.
 */
enum dhms_op_status {
	DHMS_STATUS_OK                       = 0,
	DHMS_STATUS_CLIENT_NOT_FOUND         = 1,
	DHMS_STATUS_CLIENT_ALREADY_RELEASED  = 2,
};

/**
 * struct dhms_glink_msg - Wire message format for GLink/RPMSG transport
 *
 * This structure defines the fixed-size wire message exchanged between the
 * AP kernel and remote processors over GLink/RPMSG. All fields are
 * little-endian on the wire.
 *
 * @client_id: IDR-allocated dynamic buffer identifier
 * @iova: SMMU IOVA address of the buffer
 * @size: Buffer size in bytes
 * @msg_type: Message type (enum dhms_msg_type)
 * @flags: Caller-supplied flags (forwarded from userspace)
 * @status: Operation status (enum dhms_op_status). Set by HLOS in response
 *   messages (QUERY_RSP, RELEASE_ACK). DHMS_STATUS_OK (0) on success;
 *   non-zero indicates a specific error category. Always DHMS_STATUS_OK
 *   in request messages from the remote processor.
 * @reserved: Padding to maintain 8-byte alignment of the structure.
 */
struct dhms_glink_msg {
	uint64_t  client_id;
	uint64_t  iova;
	uint64_t  size;
	uint32_t  msg_type;
	uint32_t  flags;
	uint32_t  status;
	uint32_t  reserved;
} __packed;
/* Wire size: 40 bytes */

/* ================================================================
 * ACK and processor state enumerations
 * ================================================================
 */

/**
 * enum dhms_ack_state - ACK state for a processor reference
 *
 * This enum tracks whether a remote processor has acknowledged an IMPORT
 * or RELEASE. The driver uses a fire-and-forget model where ACKs are
 * recorded asynchronously in the RPMSG callback without blocking the caller.
 *
 * @DHMS_ACK_PENDING: Waiting for the remote processor to respond.
 * @DHMS_ACK_SUCCESS: Remote confirmed the operation successfully.
 * @DHMS_ACK_NODEV: Transport disconnected (SSR) before ACK arrived.
 */
enum dhms_ack_state {
	DHMS_ACK_PENDING = 0,
	DHMS_ACK_SUCCESS = 1,
	DHMS_ACK_NODEV   = 2,
};

/**
 * enum processor_state - State of a remote processor connection
 *
 * This enum defines the lifecycle states of a remote processor's GLink
 * channel connection to the DHMS driver.
 *
 * @PROCESSOR_OFFLINE: Not connected — no GLink channel
 * @PROCESSOR_ONLINE: Connected and ready to receive messages
 * @PROCESSOR_SSR: Subsystem restart in progress
 */
enum processor_state {
	PROCESSOR_OFFLINE = 0,
	PROCESSOR_ONLINE  = 1,
	PROCESSOR_SSR     = 2,
};

/* ================================================================
 * Multi-processor support structures
 * ================================================================
 */

struct dhms_device;

/**
 * struct dhms_processor_ctx - Per-processor context
 *
 * This structure holds the context for one remote processor. One instance
 * is created per remote processor when an RPMSG channel connects via
 * dhms_rpmsg_probe() and destroyed when the channel disconnects.
 *
 * @node: Link in dhms_dev.processors list
 * @name: Processor name extracted from channel name
 * @rpdev: RPMSG device handle — used to send messages
 * @state: Current processor state (online/offline/SSR)
 * @dhms_dev: Back pointer to the owning DHMS context
 * @buffer_count: Number of buffers currently shared with this processor
 */
struct dhms_processor_ctx {
	struct list_head         node;
	char                     name[32];
	struct rpmsg_device     *rpdev;
	enum processor_state     state;
	struct dhms_device         *dhms_dev;
	atomic_t                 buffer_count;
};

/**
 * struct dhms_processor_entry - Tracks one processor's view of a buffer
 *
 * This structure tracks the ACK state for one specific processor's reference
 * to an imported buffer. Each imported buffer maintains a list of these
 * entries, one per processor that received the IMPORT message.
 *
 * @node: Link in dhms_buffer_entry.processor_refs list
 * @processor_name: Name of the processor that holds this entry
 * @rpdev: RPMSG transport to this processor
 * @ack_state: Current ACK state (updated asynchronously in callback)
 * @remote_errno: Reserved for future remote error reporting
 */
struct dhms_processor_entry {
	struct list_head         node;
	char                     processor_name[32];
	struct rpmsg_device     *rpdev;
	enum dhms_ack_state      ack_state;
	int                      remote_errno;
};

/* ================================================================
 * Per-buffer bookkeeping
 * ================================================================
 */

/**
 * struct dhms_buffer_entry - One DMA-BUF shared with remote processor(s)
 *
 * This structure tracks a single imported buffer through its full lifecycle.
 * It is created by dhms_import_dmabuf_multi() after IOMMU mapping and IMPORT
 * broadcast, stored in dhms_dev.buf_idr keyed by client_id, has its DMA-BUF
 * resources freed immediately on release, and is finally freed after all
 * RELEASE_ACKs are received in dhms_handle_release_ack().
 *
 * @client_id: IDR-allocated key, returned to userspace via DHMS_IOC_MAP
 * @fd: Original DMA-BUF fd (debug only — not used after import)
 * @dmabuf: DMA-BUF object (NULL after release)
 * @attach: DMA-BUF attachment (NULL after release)
 * @sgt: Scatter-gather table with IOVA (NULL after release)
 * @iova: SMMU IOVA sent to remote processors
 * @size: Buffer size in bytes
 * @flags: Caller-supplied flags forwarded to remote
 * @processor_refs: List of dhms_processor_entry (one per processor)
 * @ref_lock: Spinlock protecting processor_refs list
 * @num_processors: Number of processors that received this buffer
 * @acks_received: Count of ACKs received (for debugging/tracing)
 * @release_node: Link in dhms_dev.pending_releases list
 */
struct dhms_buffer_entry {
	uint64_t                   client_id;
	int                        fd;

	/* DMA-BUF resources — freed in order: unmap → detach → put */
	struct dma_buf            *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table           *sgt;

	/* IOVA sent to remote processors */
	dma_addr_t                 iova;
	size_t                     size;
	uint32_t                   flags;

	/* Multi-processor ACK tracking */
	struct list_head           processor_refs;
	spinlock_t                 ref_lock;
	int                        num_processors;
	atomic_t                   acks_received;

	/* Link in dhms_dev.pending_releases */
	struct list_head           release_node;
};

/* ================================================================
 * Per-open-file context
 * ================================================================
 */

/**
 * struct dhms_file_ctx - Created on each open() of /dev/dhms
 *
 * This structure tracks all buffers imported by a single file handle. On
 * release() (process crash or explicit close) all owned buffers are
 * automatically released, preventing leaks if userspace forgets DHMS_IOC_UNMAP.
 *
 * @ctx: Back pointer to the global DHMS context
 * @buf_list: List of dhms_buf_node (one per imported buffer)
 * @lock: Mutex protecting buf_list
 */
struct dhms_file_ctx {
	struct dhms_device  *ctx;
	struct list_head  buf_list;
	struct mutex      lock;
};

/**
 * struct dhms_buf_node - Lightweight per-fd buffer tracking node
 *
 * This structure is a lightweight node used to track a buffer's client_id
 * within a per-fd list, enabling automatic cleanup on fd close without
 * requiring access to the full dhms_buffer_entry.
 *
 * @node: Link in dhms_file_ctx.buf_list
 * @client_id: Client ID of the tracked buffer
 */
struct dhms_buf_node {
	struct list_head  node;
	uint64_t          client_id;
};

/* ================================================================
 * Global driver context
 * ================================================================
 */

/**
 * struct dhms_device - Global state for one DHMS driver instance
 *
 * This structure holds the global state for one DHMS driver instance. One
 * instance is created per platform device (one per DT node with compatible
 * "qcom,dhms-rpmsg") and manages multiple remote processors concurrently.
 *
 * @devt: Character device number
 * @cdev: Kernel cdev structure
 * @chardev: Device object for /dev/dhms
 * @class: Device class
 * @devname: Device name string ("dhms")
 * @dma_dev: DMA/IOMMU device (from qcom,dhms-dma-dev phandle)
 * @buf_idr: IDR for O(1) client_id → buffer lookup
 * @lock: Spinlock protecting buf_idr and pending_releases
 * @pending_releases: Buffers awaiting RELEASE_ACK from all processors
 * @processors: List of connected remote processors
 * @processor_lock: Spinlock protecting processors list
 * @num_processors: Count of currently online processors
 * @is_removing: Set to 1 during driver removal (signals poll waiters)
 * @wait_queue: Wait queue for poll() support and processor events
 */
struct dhms_device {
	/* Character device */
	dev_t             devt;
	struct cdev       cdev;
	struct device    *chardev;
	struct class     *class;
	char              devname[32];

	/* DMA/IOMMU device */
	struct device    *dma_dev;

	/* Buffer bookkeeping */
	struct idr        buf_idr;
	spinlock_t        lock;
	struct list_head  pending_releases;

	/* Multi-processor support */
	struct list_head  processors;
	spinlock_t        processor_lock;
	int               num_processors;

	/* Lifecycle */
	atomic_t          is_removing;
	wait_queue_head_t wait_queue;
};

/* Module-level context pointer — defined in dhms_rpmsg.c, extern here */
extern struct dhms_device *g_dhms_dev;

#endif /* __DHMS_RPMSG_H__ */
