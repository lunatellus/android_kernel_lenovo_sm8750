# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(src) -I$(srctree)

obj-$(CONFIG_LENOVO_HYPERSCHED) += lenovohyperSched.o
lenovohyperSched-y := lhsched_main.o lhsched_rwsem.o lhsched_mutex.o \
	lhsched_proc.o lhsched_prio.o lhsched_boost.o lhsched_runtime.o
