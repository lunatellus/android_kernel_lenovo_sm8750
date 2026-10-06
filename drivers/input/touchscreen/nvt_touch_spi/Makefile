# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_TOUCHSCREEN_NVT_SPI) += nvt_36xxx.o
nvt_36xxx-y := nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_mp_ctrlram.o nt36xxx_platform.o
ccflags-y += -I$(srctree)/$(src)/include -include $(srctree)/$(src)/compat.h -Wno-error=incompatible-function-pointer-types
