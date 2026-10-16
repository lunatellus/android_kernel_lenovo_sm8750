# SPDX-License-Identifier: GPL-2.0

M ?= $(shell pwd)

DTSTREE ?= $(shell realpath -s \
	$(dir $(KCONFIG_EXT_PREFIX)/arch/arm64/boot/dts/vendor/Makefile) \
	--relative-to $(KERNEL_SRC))

srctree = $(KERNEL_SRC)
# -------------------------------------------------
# Common upstream source staging (macros only)
# -------------------------------------------------
include $(M)/common_sources.mk
STAGING_DIR := $(shell realpath -s $(abspath $(SOC_STAGING)) --relative-to $(M))

soc = $(abspath $(SOC_STAGING))
LINUXINCLUDE := \
  -I$(soc)/. \
  -I$(soc)/arch/arm64/include \
  -I$(soc)/include \
  -I$(soc)/include/uapi \
  -I$(soc)/include/linux \
  -I$(srctree)/ \
  $(LINUXINCLUDE)


# -------------------------------------------------
# Subdirectories
# -------------------------------------------------
obj-y += $(STAGING_DIR)/arch/arm64/gunyah/
obj-y += $(STAGING_DIR)/drivers/
obj-y += $(STAGING_DIR)/kernel/
obj-y += $(STAGING_DIR)/lib/
obj-y += $(STAGING_DIR)/mm/
obj-y += $(STAGING_DIR)/net/
obj-y += $(STAGING_DIR)/sound/

# -------------------------------------------------
# Targets
# -------------------------------------------------
all:
	$(STAGE_COMMON_SOURCES)
	$(MAKE) -C $(KERNEL_SRC) M=$(M) modules $(KBUILD_OPTIONS)
	$(MAKE) -C $(KERNEL_SRC) dtbs V=1 dtstree=$(DTSTREE)

modules_install:
	$(MAKE) INSTALL_MOD_STRIP=1 -C $(KERNEL_SRC) M=$(M) modules_install

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(M) clean

