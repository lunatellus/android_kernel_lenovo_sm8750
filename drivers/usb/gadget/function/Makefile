# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_USB_F_FS_IPC_LOGGING) += f_fs_ipc_log.o

obj-$(CONFIG_USB_F_CCID) += usb_f_ccid.o
usb_f_ccid-objs += f_ccid.o

obj-$(CONFIG_USB_F_CDEV) += usb_f_cdev.o
usb_f_cdev-objs += f_cdev.o

obj-$(CONFIG_USB_F_GSI) += usb_f_gsi.o
usb_f_gsi-objs += f_gsi.o rndis.o

obj-$(CONFIG_USB_F_QDSS) += usb_f_qdss.o
usb_f_qdss-objs += f_qdss.o u_qdss.o

obj-$(CONFIG_USB_F_RMNET_BAM) += usb_f_rmnet_bam.o
usb_f_rmnet_bam-objs += f_rmnet.o u_ctrl_qti.o u_bam_dmux.o

