# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

# Subdirectories
obj-y += qcom/

obj-$(CONFIG_QTI_BATTERY_CHARGER) += qti-battery-charger.o
qti-battery-charger-objs += qti_battery_charger.o qti_charger_boost_lib.o qti_typec_class.o

obj-$(CONFIG_BATTERY_MAX1720X) += max1720x_battery.o

