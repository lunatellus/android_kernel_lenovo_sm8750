// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. */
#include <linux/compiler.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/notifier.h>
#include <linux/mutex.h>
#include <linux/virtio.h>
#include <linux/scatterlist.h>
#include <linux/workqueue.h>
#include <linux/phy.h>
#include <linux/virtio_config.h>
#include <linux/semaphore.h>
#include <linux/emac_mdio_fe.h>

#define EMAC_MDIO_FE_ERR(fmt, args...)                            \
	pr_err("[ ERR %s: %d] " fmt "\n", __func__,  __LINE__, ##args)
#define EMAC_MDIO_FE_WARN(fmt, args...)                           \
	pr_warn("[ WARN: %s: %d] " fmt "\n", __func__,  __LINE__, ##args)
#define EMAC_MDIO_FE_INFO(fmt, args...)                           \
	pr_info("[ INFO: %s: %d] " fmt "\n", __func__,  __LINE__, ##args)
#define EMAC_MDIO_FE_DBG(fmt, args...)                           \
	pr_debug("[ DBG: %s: %d] " fmt "\n", __func__,  __LINE__, ##args)
/* Virtio ID of EMAC */
#define VIRTIO_DT_QCOM_BASE         (49152)
#define VIRTIO_DT_EMAC_MDIO         (VIRTIO_DT_QCOM_BASE + 12)
#define WAIT_PHY_REPLY_MAX_TIMEOUT  (5000)
#define WAIT_MDIO_HW_UP_MAX_TIMEOUT (500)

enum mdio_interface_type_e {
	MDIO_CLAUSE_22,
	MDIO_CLAUSE_45_INDIRECT,
	MDIO_CLAUSE_45_DIRECT,
	MDIO_MAC2MAC_WO_MDIO,
	MDIO_TYPE_TOTAL
};

enum mdio_remote_op_type_e {
	MDIO_REMOTE_OP_TYPE_NULL,
	MDIO_REMOTE_OP_TYPE_READ,
	MDIO_REMOTE_OP_TYPE_WRITE,
	MDIO_REMOTE_OP_TYPE_TOTAL
};

struct phy_remote_access_t {
	enum mdio_interface_type_e mdio_type;
	enum mdio_remote_op_type_e mdio_op_remote_type;
	unsigned char  phyaddr;
	unsigned short phydev;
	unsigned short phyreg;
	unsigned short phydata;
	unsigned int   physeq;
};

struct phy_remote_reply_t {
	int              phyrst;
	unsigned int     physeq;
};

enum emac_mdio_hw_state_type {
	EMAC_MDIO_HW_STATE_UNKNOWN = -1,
	EMAC_MDIO_HW_STATE_DOWN,
	EMAC_MDIO_HW_STATE_UP,
};

struct be_to_fe_msg {
	u16 msgid;        /* unique message id */
	u16 len;          /* command total length */
	u32 cmd;          /* command */
	struct phy_remote_reply_t result;
} __packed;

struct fe_to_be_msg {
	u8  type;         /* unique message id */
	u16 len;          /* command total length */
	u8  result;       /* command result */
	struct phy_remote_access_t request_data;
} __packed;

struct emac_mdio_dev {
	/* device driver properties */
	char                         *name;

	/* Virtio device */
	struct virtio_device          *vdev;

	struct virtqueue              *emac_mdio_fe_txq;
	struct virtqueue              *emac_mdio_fe_rxq;

	spinlock_t                    txq_lock; /* Used for transmitting */
	spinlock_t                    rxq_lock; /* Used for receiving */

	struct                        fe_to_be_msg tx_msg;
	struct                        be_to_fe_msg rx_msg[10];
	enum emac_mdio_hw_state_type  emac_mdio_hw_state;
	struct semaphore              emac_mdio_fe_sem;
	u32                           emac_mdio_fe_seq;
	struct semaphore              emac_mdio_hw_sem;
	bool                          emac_mdio_hw_pending;
	struct mutex                  emac_mdio_fe_lock; /* Used for mdio ready/write */
	s32                           phy_reply;
};

enum EMAC_MDIO_FE_VIRTQ {
	EMAC_MDIO_FE_TX_VQ     = 0,
	EMAC_MDIO_FE_RX_VQ     = 1,
	EMAC_MDIO_FE_VIRTQ_NUM = 2,
};

enum emac_mdio_fe_to_be_cmds {
	EMAC_MDIO_FE_DOWN = 0,
	EMAC_MDIO_FE_UP   = 1,
	EMAC_MDIO_FE_REQ  = 2,
};

enum emac_mdio_be_to_fe_cmds {
	EMAC_MDIO_HW_DOWN  = 0,
	EMAC_MDIO_HW_UP    = 1,
	EMAC_MDIO_HW_REPLY = 2,
};

static int emac_mdio_fe_probe(struct virtio_device *vdev);
#ifdef CONFIG_PM_SLEEP
static int emac_mdio_fe_freeze(struct virtio_device *vdev);
static int emac_mdio_fe_restore(struct virtio_device *vdev);
#endif

static struct emac_mdio_dev *emac_mdio_fe_ctx;
static const struct virtio_device_id id_table[] = {
	{ VIRTIO_DT_EMAC_MDIO, VIRTIO_DEV_ANY_ID },
	{ 0 }
};

static unsigned int features[] = {
	/* none */
};

static struct virtio_driver emac_mdio_fe_virtio_drv = {
	.driver = {
		.name = KBUILD_MODNAME,
		.owner = THIS_MODULE,
	},
	.feature_table = features,
	.feature_table_size = ARRAY_SIZE(features),
	.id_table = id_table,
	.probe = emac_mdio_fe_probe,
#ifdef CONFIG_PM_SLEEP
	.freeze = emac_mdio_fe_freeze,
	.restore = emac_mdio_fe_restore,
#endif
};

static int __maybe_unused emac_mdio_fe_xmit(struct emac_mdio_dev *pdev)
{
	unsigned long flags;
	struct scatterlist sg[1];
	struct fe_to_be_msg *msg = NULL;
	int retval = 0;

	msg = &pdev->tx_msg;
	EMAC_MDIO_FE_DBG("Entry msg len =%d", msg->len);
	sg_init_one(sg, msg, sizeof(*msg));

	spin_lock_irqsave(&pdev->txq_lock, flags);
	/* expose output buffers to other end */
	retval = virtqueue_add_outbuf(pdev->emac_mdio_fe_txq, sg, 1, msg, GFP_ATOMIC);
	spin_unlock_irqrestore(&pdev->txq_lock, flags);
	if (retval) {
		EMAC_MDIO_FE_ERR("Fail to add output buffer");
		return retval;
	}
	/* update other side after add_buf */
	spin_lock_irqsave(&pdev->txq_lock, flags);
	virtqueue_kick(pdev->emac_mdio_fe_txq);
	spin_unlock_irqrestore(&pdev->txq_lock, flags);
	EMAC_MDIO_FE_DBG("Kicked Host receive Q");
	return retval;
}

static int __maybe_unused emac_mdio_fe_xmit_and_wait(struct emac_mdio_dev *pdev)
{
	int ret = 0;
	unsigned long tmp;

	pdev->tx_msg.type = EMAC_MDIO_FE_REQ;
	pdev->tx_msg.len = sizeof(struct fe_to_be_msg);
	pdev->emac_mdio_fe_seq++;
	pdev->tx_msg.request_data.physeq = pdev->emac_mdio_fe_seq;
	pdev->phy_reply = -1;
	emac_mdio_fe_xmit(pdev);
	EMAC_MDIO_FE_DBG("Sent EMAC_MDIO_FE_REQ Cmd");
	tmp = msecs_to_jiffies(WAIT_PHY_REPLY_MAX_TIMEOUT);
	ret = down_timeout(&pdev->emac_mdio_fe_sem, tmp);
	if (ret == 0)
		ret = (int)pdev->phy_reply;
	else if (ret == -ETIME)
		EMAC_MDIO_FE_WARN("Wait for phy reply timeout");
	else
		EMAC_MDIO_FE_WARN("Unknown error return value");
	return ret;
}

static int __maybe_unused emac_mdio_fe_wait_for_hw_up(struct emac_mdio_dev *pdev)
{
	int ret = 0;
	unsigned long tmp;

	if (pdev->emac_mdio_hw_state != EMAC_MDIO_HW_STATE_UP) {
		if (pdev->emac_mdio_hw_state == EMAC_MDIO_HW_STATE_UNKNOWN) {
			pdev->tx_msg.type = EMAC_MDIO_FE_UP;
			pdev->tx_msg.len = sizeof(struct fe_to_be_msg);
			emac_mdio_fe_xmit(pdev);
			EMAC_MDIO_FE_INFO("Sent Register Event Cmd");
		}
		tmp = msecs_to_jiffies(WAIT_MDIO_HW_UP_MAX_TIMEOUT);
		pdev->emac_mdio_hw_pending = true;
		ret = down_timeout(&pdev->emac_mdio_hw_sem, tmp);
		pdev->emac_mdio_hw_pending = false;
		if (ret == 0) {
			EMAC_MDIO_FE_INFO("MDIO HW status is up");
		} else if (ret == -ETIME) {
			EMAC_MDIO_FE_WARN("Wait for MDIO HW up timeout");
			ret = -EIO;
		} else {
			EMAC_MDIO_FE_WARN("Unknown error return value");
		}
	}
	return ret;
}

static void emac_mdio_fe_replenish_rxbuf(struct emac_mdio_dev *pdev, struct be_to_fe_msg *msg)
{
	struct scatterlist sg[1];
	unsigned long flags;

	EMAC_MDIO_FE_DBG("Entry");
	memset(msg, 0x0, sizeof(*msg));
	sg_init_one(sg, msg, sizeof(*msg));
	/* expose input buffers to other end */
	spin_lock_irqsave(&pdev->rxq_lock, flags);
	virtqueue_add_inbuf(pdev->emac_mdio_fe_rxq, sg, 1, msg, GFP_ATOMIC);
	spin_unlock_irqrestore(&pdev->rxq_lock, flags);
}

static void emac_mdio_fe_update(struct emac_mdio_dev *pdev, struct be_to_fe_msg *msg)
{
	unsigned int req_seq = 0;
	unsigned int reply_seq = 0;
	int          phy_reply = 0;

	if (!pdev || !msg) {
		EMAC_MDIO_FE_ERR("pdev or msg is NULL");
		return;
	}
	EMAC_MDIO_FE_DBG("Receive msg->cmd= %d", msg->cmd);
	switch (msg->cmd) {
	case EMAC_MDIO_HW_DOWN:
		EMAC_MDIO_FE_INFO("Notify EMAC_MDIO_HW_DOWN");
		pdev->emac_mdio_hw_state = EMAC_MDIO_HW_STATE_DOWN;
		break;
	case EMAC_MDIO_HW_UP:
		EMAC_MDIO_FE_INFO("Notify EMAC_MDIO_HW_UP");
		pdev->emac_mdio_hw_state = EMAC_MDIO_HW_STATE_UP;
		if (pdev->emac_mdio_hw_pending)
			up(&pdev->emac_mdio_hw_sem);
		else
			EMAC_MDIO_FE_WARN("emac_mdio_fe is not in pending status, no need to up");
		break;
	case EMAC_MDIO_HW_REPLY:
		req_seq = pdev->emac_mdio_fe_seq;
		reply_seq = msg->result.physeq;
		phy_reply = msg->result.phyrst;
		EMAC_MDIO_FE_DBG("Notify EMAC_MDIO_HW_REPLY with rst as %d", phy_reply);
		if (req_seq == reply_seq) {
			pdev->phy_reply = phy_reply;
			up(&pdev->emac_mdio_fe_sem);
		} else {
			EMAC_MDIO_FE_WARN("req_seq:%u != reply_seq:%u", req_seq, reply_seq);
		}
		break;

	default:
		EMAC_MDIO_FE_WARN("Received cmd %d not recognized ",  msg->cmd);
		break;
	}
}

/**
 * This is similar to RX complete interrupt.
 * It seems like single kick but needs to treat as if_start.
 */
static void emac_mdio_fe_recv_done(struct virtqueue *rvq)
{
	struct emac_mdio_dev *pdev = rvq->vdev->priv;
	struct be_to_fe_msg *msg;
	unsigned long flags;
	unsigned int len;

	EMAC_MDIO_FE_DBG("Entry");
	while (1) {
		EMAC_MDIO_FE_DBG("Call Virtqueue_get_buff");
		spin_lock_irqsave(&pdev->rxq_lock, flags);
		msg = virtqueue_get_buf(pdev->emac_mdio_fe_rxq, &len);
		spin_unlock_irqrestore(&pdev->rxq_lock, flags);
		if (!msg) {
			EMAC_MDIO_FE_DBG("incoming signal, but no used buffer");
			break;
		}
		EMAC_MDIO_FE_DBG("Got Buffer len %d ", len);
		/* Process received message, can be stubbed out */
		emac_mdio_fe_update(pdev, msg);

		/* Reclaim RX buffer */
		emac_mdio_fe_replenish_rxbuf(pdev, msg);
	}

	spin_lock_irqsave(&pdev->rxq_lock, flags);
	virtqueue_kick(pdev->emac_mdio_fe_rxq);
	spin_unlock_irqrestore(&pdev->rxq_lock, flags);
}

static void emac_mdio_fe_xmit_done(struct virtqueue *txq)
{
	struct emac_mdio_dev *pdev = txq->vdev->priv;
	struct fe_to_be_msg  *msg = NULL;
	unsigned long        flags = 0;
	unsigned int         len = 0;

	EMAC_MDIO_FE_DBG("-->");
	while (1) {
		EMAC_MDIO_FE_DBG("Call virtqueue_get_buf");
		spin_lock_irqsave(&pdev->txq_lock, flags);
		msg = virtqueue_get_buf(pdev->emac_mdio_fe_txq, &len);
		spin_unlock_irqrestore(&pdev->txq_lock, flags);
		if (!msg)
			break;
	}
	EMAC_MDIO_FE_DBG("<--");
}

static void emac_mdio_fe_allocate_rxbufs(struct emac_mdio_dev *pdev)
{
	unsigned long flags;
	int i, size;

	spin_lock_irqsave(&pdev->rxq_lock, flags);
	size = virtqueue_get_vring_size(pdev->emac_mdio_fe_rxq);
	spin_unlock_irqrestore(&pdev->rxq_lock, flags);
	if (size > ARRAY_SIZE(pdev->rx_msg))
		size = ARRAY_SIZE(pdev->rx_msg);
	for (i = 0; i < size; i++)
		emac_mdio_fe_replenish_rxbuf(pdev, &pdev->rx_msg[i]);
}

static int emac_mdio_fe_init_vqs(struct emac_mdio_dev *pdev)
{
	struct virtqueue *vqs[EMAC_MDIO_FE_VIRTQ_NUM];
	static const char *const names[] = { "emac_mdio_tx", "emac_mdio_rx" };
	vq_callback_t *cbs[] = {emac_mdio_fe_xmit_done, emac_mdio_fe_recv_done};
	int ret;
	struct virtqueue_info vqs_info[2];

	vqs_info[0].callback = cbs[0];
	vqs_info[0].name = names[0];

	vqs_info[1].callback = cbs[1];
	vqs_info[1].name = names[1];

	/* Find VirtQueues and Register callback*/
	ret = virtio_find_vqs(pdev->vdev, EMAC_MDIO_FE_VIRTQ_NUM, vqs, vqs_info, NULL);
	if (ret) {
		EMAC_MDIO_FE_ERR("virtio_find_vqs failed");
		return ret;
	}
	EMAC_MDIO_FE_INFO("VirtQ Callback Reg Complete");
	/* Initialize TX VQ*/
	spin_lock_init(&pdev->txq_lock);
	pdev->emac_mdio_fe_txq = vqs[EMAC_MDIO_FE_TX_VQ];

	/* Initialized RX VQ*/
	spin_lock_init(&pdev->rxq_lock);
	pdev->emac_mdio_fe_rxq = vqs[EMAC_MDIO_FE_RX_VQ];
	EMAC_MDIO_FE_INFO("VirtQ Init Complete");
	return 0;
}

int virtio_mdio_read(int addr, int regnum)
{
	struct phy_remote_access_t *phy_request = NULL;
	int ret = 0;

	mutex_lock(&emac_mdio_fe_ctx->emac_mdio_fe_lock);
	ret = emac_mdio_fe_wait_for_hw_up(emac_mdio_fe_ctx);
	if (ret == 0) {
		phy_request = &emac_mdio_fe_ctx->tx_msg.request_data;
		memset(phy_request, 0, sizeof(struct phy_remote_access_t));
		phy_request->mdio_type = MDIO_CLAUSE_22;
		phy_request->mdio_op_remote_type = MDIO_REMOTE_OP_TYPE_READ;
		phy_request->phyaddr = addr;
		phy_request->phyreg = regnum;
		EMAC_MDIO_FE_DBG("Send EMAC_MDIO_FE_REQ Event Cmd");
		ret = emac_mdio_fe_xmit_and_wait(emac_mdio_fe_ctx);
	}
	mutex_unlock(&emac_mdio_fe_ctx->emac_mdio_fe_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(virtio_mdio_read);

int virtio_mdio_write(int addr, int regnum, u16 val)
{
	struct phy_remote_access_t *phy_request = NULL;
	int ret = 0;

	mutex_lock(&emac_mdio_fe_ctx->emac_mdio_fe_lock);
	ret = emac_mdio_fe_wait_for_hw_up(emac_mdio_fe_ctx);
	if (ret == 0) {
		phy_request = &emac_mdio_fe_ctx->tx_msg.request_data;
		memset(phy_request, 0, sizeof(struct phy_remote_access_t));
		phy_request->mdio_type = MDIO_CLAUSE_22;
		phy_request->mdio_op_remote_type = MDIO_REMOTE_OP_TYPE_WRITE;
		phy_request->phyaddr = addr;
		phy_request->phyreg = regnum;
		phy_request->phydata = val;
		EMAC_MDIO_FE_DBG("Send EMAC_MDIO_FE_REQ Event Cmd");
		ret = emac_mdio_fe_xmit_and_wait(emac_mdio_fe_ctx);
	}
	mutex_unlock(&emac_mdio_fe_ctx->emac_mdio_fe_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(virtio_mdio_write);

int virtio_mdio_read_c45(int addr, int devnum, int regnum)
{
	struct phy_remote_access_t *phy_request = NULL;
	int ret = 0;

	mutex_lock(&emac_mdio_fe_ctx->emac_mdio_fe_lock);
	ret = emac_mdio_fe_wait_for_hw_up(emac_mdio_fe_ctx);
	if (ret == 0) {
		phy_request = &emac_mdio_fe_ctx->tx_msg.request_data;
		memset(phy_request, 0, sizeof(struct phy_remote_access_t));
		phy_request->mdio_type = MDIO_CLAUSE_45_DIRECT;
		phy_request->mdio_op_remote_type = MDIO_REMOTE_OP_TYPE_READ;
		phy_request->phyaddr = addr;
		phy_request->phydev = devnum;
		phy_request->phyreg = regnum;
		EMAC_MDIO_FE_DBG("Send EMAC_MDIO_FE_REQ Event Cmd");
		ret = emac_mdio_fe_xmit_and_wait(emac_mdio_fe_ctx);
	}
	mutex_unlock(&emac_mdio_fe_ctx->emac_mdio_fe_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(virtio_mdio_read_c45);

int virtio_mdio_write_c45(int addr, int devnum, int regnum, u16 val)
{
	struct phy_remote_access_t *phy_request = NULL;
	int ret = 0;

	mutex_lock(&emac_mdio_fe_ctx->emac_mdio_fe_lock);
	ret = emac_mdio_fe_wait_for_hw_up(emac_mdio_fe_ctx);
	if (ret == 0) {
		phy_request = &emac_mdio_fe_ctx->tx_msg.request_data;
		memset(phy_request, 0, sizeof(struct phy_remote_access_t));
		phy_request->mdio_type = MDIO_CLAUSE_45_DIRECT;
		phy_request->mdio_op_remote_type = MDIO_REMOTE_OP_TYPE_WRITE;
		phy_request->phyaddr = addr;
		phy_request->phydev = devnum;
		phy_request->phyreg = regnum;
		phy_request->phydata = val;
		EMAC_MDIO_FE_DBG("Send EMAC_MDIO_FE_REQ Event Cmd");
		ret = emac_mdio_fe_xmit_and_wait(emac_mdio_fe_ctx);
	}
	mutex_unlock(&emac_mdio_fe_ctx->emac_mdio_fe_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(virtio_mdio_write_c45);

static int emac_mdio_fe_probe(struct virtio_device *vdev)
{
	int ret;
	struct emac_mdio_dev *pdev;
	unsigned long flags;

	EMAC_MDIO_FE_INFO("Start Probe allocate devm");
	/*
	 * Resource Managed kzalloc and mem allocated with the fun is auto freed on driver detach
	 * Allocations are cleaned up automatically shall the probe itself fail
	 */
	pdev = devm_kzalloc(&vdev->dev, sizeof(*pdev), GFP_KERNEL);
	if (!pdev) {
		ret = -ENOMEM;
		return ret;
	}
	sema_init(&pdev->emac_mdio_fe_sem, (0));
	pdev->emac_mdio_fe_seq = 0;
	sema_init(&pdev->emac_mdio_hw_sem, (0));
	pdev->emac_mdio_hw_pending = false;
	mutex_init(&pdev->emac_mdio_fe_lock);

	emac_mdio_fe_ctx = pdev;
	vdev->priv = pdev;
	pdev->vdev = vdev;
	pdev->name = "emac_mdio_fe";

	/* Initialize States */
	pdev->emac_mdio_hw_state = EMAC_MDIO_HW_STATE_UNKNOWN;
	EMAC_MDIO_FE_INFO("Init VQS");
	/* Allocate and Initialize RX and TX VirtQueues */
	ret = emac_mdio_fe_init_vqs(pdev);
	if (ret) {
		EMAC_MDIO_FE_ERR("emac_mdio_fe failed to init vqs");
		return ret;
	}
	/* enable vq use in probe function */
	virtio_device_ready(vdev);
	EMAC_MDIO_FE_INFO("Allocate RXBufs");
	emac_mdio_fe_allocate_rxbufs(pdev);
	/* Enable TX Complete ISR */
	virtqueue_enable_cb(pdev->emac_mdio_fe_txq);
	/* Enable Rx Complete ISR*/
	virtqueue_enable_cb(pdev->emac_mdio_fe_rxq);
	/* Kick Host */
	spin_lock_irqsave(&pdev->rxq_lock, flags);
	virtqueue_kick(pdev->emac_mdio_fe_rxq);
	spin_unlock_irqrestore(&pdev->rxq_lock, flags);
	EMAC_MDIO_FE_INFO("Kicked Host VirtQ");
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int emac_mdio_fe_freeze(struct virtio_device *vdev)
{
	virtio_reset_device(vdev);
	vdev->config->del_vqs(vdev);
	return 0;
}

static int emac_mdio_fe_restore(struct virtio_device *vdev)
{
	struct emac_mdio_dev *pdev = vdev->priv;
	int ret;

	ret = emac_mdio_fe_init_vqs(pdev);
	if (ret)
		dev_err(&vdev->dev, "fail to initialize virtqueues\n");
	virtio_device_ready(vdev);
	emac_mdio_fe_allocate_rxbufs(pdev);
	/* Enable TX Complete ISR */
	virtqueue_enable_cb(pdev->emac_mdio_fe_txq);
	/* Enable Rx Complete ISR*/
	virtqueue_enable_cb(pdev->emac_mdio_fe_rxq);
	/* Kick Host */
	virtqueue_kick(pdev->emac_mdio_fe_rxq);
	return 0;
}
#endif

static int __init emac_mdio_fe_init(void)
{
	EMAC_MDIO_FE_INFO("%s: Module Entry", __func__);
	return register_virtio_driver(&emac_mdio_fe_virtio_drv);
}

static void __exit emac_mdio_fe_exit(void)
{
	unregister_virtio_driver(&emac_mdio_fe_virtio_drv);
}

module_init(emac_mdio_fe_init);
module_exit(emac_mdio_fe_exit);
MODULE_SOFTDEP("post: stmmac");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EMAC Virt MDIO FE Driver");
