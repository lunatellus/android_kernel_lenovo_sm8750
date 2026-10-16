# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

cfg80211-tests-y += module.o fragmentation.o scan.o util.o chan.o

obj-$(CONFIG_CFG80211_KUNIT_TEST) += cfg80211-tests.o
