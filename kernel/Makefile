# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

# Subdirectories
obj-y += locking/
obj-y += rcu/
obj-y += sched/walt/
obj-y += trace/

obj-$(CONFIG_MSM_SYSSTATS) += msm_sysstats.o

obj-$(CONFIG_TORTURE_TEST) += torture.o

