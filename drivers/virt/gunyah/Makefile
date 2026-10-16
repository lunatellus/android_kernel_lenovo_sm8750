# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_GH_CTRL) += gh_ctrl.o

obj-$(CONFIG_GH_DBL) += gh_dbl.o

obj-$(CONFIG_GH_IRQ_LEND) += gh_irq_lend.o

obj-$(CONFIG_GH_MEM_NOTIFIER) += gh_mem_notifier.o

obj-$(CONFIG_GH_RM_HEAP_MANAGER) += gh_rm_heap_manager.o

obj-$(CONFIG_GH_MSGQ) += gh_msgq.o

obj-$(CONFIG_GH_PANIC_NOTIFIER) += gh_panic_notifier.o

obj-$(CONFIG_GH_RM_BOOSTER) += gh_rm_booster.o

obj-$(CONFIG_GH_RM_DRV) += gh_rm_drv.o
gh_rm_drv-objs += gh_rm_core.o gh_rm_iface.o gh_devices.o

obj-$(CONFIG_GH_VIRT_WATCHDOG) += gh_virt_wdt.o

obj-$(CONFIG_GH_GUEST_POPS) += gh_guest_pops.o

obj-$(CONFIG_GH_VIRTIO_PCI_EDGE) += gh_virtio_pci_edge.o

obj-$(CONFIG_GH_SECURE_VM_LOADER) += gunyah_loader.o
gunyah_loader-objs += gh_main.o gh_secure_vm_virtio_backend.o
gunyah_loader-$(CONFIG_GH_SECURE_VM_LOADER) += gh_secure_vm_loader.o
gunyah_loader-$(CONFIG_GH_PROXY_SCHED) += gh_proxy_sched.o

obj-$(CONFIG_GUNYAH_QCOM_PLATFORM) += gunyah_qcom.o

obj-$(CONFIG_GH_VCPU_MGR) += gh_vcpu_mgr.o

obj-$(CONFIG_GUNYAH_TRACE) += gunyah_trace.o

