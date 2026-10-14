/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef FSA4480_I2C_H
#define FSA4480_I2C_H

#include <linux/of.h>
#include <linux/notifier.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>

enum fsa_function {
	FSA_MIC_GND_SWAP,
	FSA_USBC_ORIENTATION_CC1,
	FSA_USBC_ORIENTATION_CC2,
	FSA_USBC_DISPLAYPORT_DISCONNECTED,
	FSA_EVENT_MAX,
};

struct regmap;
struct device;
struct power_supply;
struct iio_channel;
struct typec_mux_dev;

struct fsa4480_priv {
	struct regmap *regmap;
	struct device *dev;
	struct typec_mux_dev *mux;
	struct power_supply *usb_psy;
	struct notifier_block nb;
	struct notifier_block ucsi_nb;
	struct iio_channel *iio_ch;
	u32 use_powersupply;
	bool dual_port;
	atomic_t usbc_mode;
	struct work_struct usbc_analog_work;
	struct blocking_notifier_head fsa4480_notifier;
	struct mutex notification_lock;
};

struct fsa4480_sub_priv {
	struct regmap *regmap;
	struct device *dev;
	struct power_supply *usb_psy;
	struct notifier_block ucsi_nb;
	struct iio_channel *iio_ch;
	atomic_t usbc_mode;
	struct work_struct usbc_analog_work;
	struct blocking_notifier_head fsa4480_sub_notifier;
	struct mutex notification_lock;
	u32 use_powersupply;
};

extern unsigned int audio_4480_headset_ref;
extern struct fsa4480_priv *audio_4480_priv1;
extern struct fsa4480_sub_priv *audio_4480_priv2;
extern bool fsa4480_switch_C1_enable;
extern int direction_value;
extern unsigned int cc_port_num;

int fsa4480_sub_switch_event(struct device_node *node, enum fsa_function event);
int fsa4480_sub_reg_notifier(struct notifier_block *nb, struct device_node *node);
int fsa4480_sub_unreg_notifier(struct notifier_block *nb, struct device_node *node);

#if IS_ENABLED(CONFIG_QCOM_FSA4480_I2C)
int fsa4480_switch_event(struct device_node *node,
			 enum fsa_function event);
int fsa4480_reg_notifier(struct notifier_block *nb,
			 struct device_node *node);
int fsa4480_unreg_notifier(struct notifier_block *nb,
			   struct device_node *node);
#else
static inline int fsa4480_switch_event(struct device_node *node,
				       enum fsa_function event)
{
	return 0;
}

static inline int fsa4480_reg_notifier(struct notifier_block *nb,
				       struct device_node *node)
{
	return 0;
}

static inline int fsa4480_unreg_notifier(struct notifier_block *nb,
					 struct device_node *node)
{
	return 0;
}
#endif /* CONFIG_QCOM_FSA4480_I2C */

#endif /* FSA4480_I2C_H */

