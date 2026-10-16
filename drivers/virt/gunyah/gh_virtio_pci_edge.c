// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s:" fmt, __func__

#include <linux/device/bus.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/platform_device.h>
#include <linux/pm.h>

#include "kernel/sched/sched.h"

static void gh_virtio_pci_edge_dev_note(struct pci_dev *pdev)
{
	int err;
	int irq = pdev->irq;

	if (irq) {
		err = irq_set_irq_type(irq, IRQ_TYPE_EDGE_RISING);
		if (err) {
			pr_err("failed to set IRQ %d to RISING edge err=%d, may miss wakeups\n",
				irq, err);
			return;
		}
		err = enable_irq_wake(irq);
		if (err) {
			pr_err("failed to set IRQ %d to wakeup capable err=%d\n",
				irq, err);
			return;
		}
		pr_info("IRQ %d made wakeup and set to RISING edge\n", irq);
	} else {
		pr_err("IRQ not present\n");
	}
}

static bool is_supported_virtio_vendor(struct pci_dev *pdev)
{
	return (pdev->vendor == PCI_VENDOR_ID_REDHAT_QUMRANET ||
		pdev->vendor == PCI_VENDOR_ID_REDHAT ||
		pdev->vendor == PCI_VENDOR_ID_INTEL);
}

static int gh_virtio_pci_edge_notifier_call(struct notifier_block *nb,
					    unsigned long event,
					    void *data)
{
	struct device *dev = data;
	struct pci_dev *pdev = to_pci_dev(dev);

	if (event != BUS_NOTIFY_ADD_DEVICE)
		return NOTIFY_DONE;

	if (is_supported_virtio_vendor(pdev)) {
		gh_virtio_pci_edge_dev_note(pdev);
		dev_pm_syscore_device(&pdev->dev, true);
	}

	return NOTIFY_OK;
}

static struct notifier_block gh_virtio_pci_edge_notifier = {
	.notifier_call = gh_virtio_pci_edge_notifier_call,
};

static void gh_virtio_pci_edge_probe_devices(void)
{
	struct device *dev = NULL;
	struct pci_dev *pdev;

	while ((dev = bus_find_next_device(&pci_bus_type, dev))) {
		pdev = to_pci_dev(dev);
		if (is_supported_virtio_vendor(pdev)) {
			gh_virtio_pci_edge_dev_note(pdev);
			dev_pm_syscore_device(&pdev->dev, true);
		}
		put_device(dev);
	}
}

static int gh_virtio_pci_edge_probe(struct platform_device *pdev)
{
	int ret;

	gh_virtio_pci_edge_probe_devices();

	ret = bus_register_notifier(&pci_bus_type, &gh_virtio_pci_edge_notifier);
	if (ret)
		pr_err("PCI bus_register_notifier failed with %d, ignoring.\n", ret);

	return 0;
}

static int gh_virtio_pci_edge_suspend_late(struct device *dev)
{
	unsigned int i, nr_iowait = 0;

	for_each_possible_cpu(i)
		nr_iowait += atomic_read(&cpu_rq(i)->nr_iowait);

	if (nr_iowait) {
		pr_info("Aborting suspend due to pending IO tasks (%u)\n", nr_iowait);
		return -EBUSY;
	}

	return 0;
}

static const struct dev_pm_ops gh_virtio_pci_edge_pm_ops = {
	.suspend_late = gh_virtio_pci_edge_suspend_late,
};

static struct platform_driver gh_virtio_pci_edge_plat_driver = {
	.probe = gh_virtio_pci_edge_probe,
	.driver = {
		.name = "gh_virtio_pci_edge",
		.pm = &gh_virtio_pci_edge_pm_ops,
	},
};

static struct platform_device *gh_virtio_pci_edge_plat_dev;

static int __init gh_virtio_pci_edge_init(void)
{
	int ret;

	gh_virtio_pci_edge_plat_dev = platform_device_register_simple(
				"gh_virtio_pci_edge", -1, NULL, 0);
	if (IS_ERR(gh_virtio_pci_edge_plat_dev))
		return PTR_ERR(gh_virtio_pci_edge_plat_dev);

	ret = platform_driver_register(&gh_virtio_pci_edge_plat_driver);
	if (ret) {
		platform_device_unregister(gh_virtio_pci_edge_plat_dev);
		return ret;
	}

	return 0;
}

module_init(gh_virtio_pci_edge_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GH Virtio PCI Interrupt Edge Module");
