# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

# Subdirectories
obj-y += zram/

obj-$(CONFIG_VIRTIO_BLK) += virtio_block.o
virtio_block-objs += virtio_blk.o

obj-$(CONFIG_VIRTIO_BLK_QTI_CRYPTO) += virtio_blk_qti_crypto.o

