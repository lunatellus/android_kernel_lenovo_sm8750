# SPDX-License-Identifier: GPL-2.0
# ================================================================
#  common_sources.mk
#
#  Purpose:
#    This Makefile fragment implements the staging/overlay workflow
#    for SoC builds in a Make/Kbuild-friendly way. It replaces the
#    older copy‑and‑paste model from common_sources.bzl with a cleaner,
#    reproducible staging approach.
#
#  What it does:
#    - Stages upstream kernel driver .c/.h files into a dedicated
#      SoC staging directory (via rsync filters)
#    - Overlays the original SoC repository on top of that staging
#      directory so SoC‑specific files take precedence
#    - Applies patches to upstream sources to produce SoC‑specific
#      variants in the staged tree
#
#  Key sections:
#    - COMMON_PATCH_FILES: Patched files in the format:
#          dst|src|diff
#        where:
#          dst  = output file path in staged SoC repo
#          src  = original file path in common kernel source
#          diff = patch file path in original SoC repo
#
#  How to modify:
#    - To add a new patch:
#        * Append a new "dst|src|diff" entry to COMMON_PATCH_FILES
#    - Keep paths relative to the common kernel/SoC root (no leading '/')
#    - Ensure patch files apply cleanly; failed patches will stop the build
#
#  Notes:
#    - This file is intended to be Kbuild‑safe and maintainable.
#    - Modify the lists above rather than editing macros directly.
#
# ================================================================

PATCH   ?= patch
RSYNC   ?= rsync
MKDIR_P ?= mkdir -p

EXT_ROOT := $(patsubst %/,%,$(KCONFIG_EXT_PREFIX))
SRC_ROOT := $(KERNEL_SRC)
SOC_STAGING := $(abspath $(EXT_ROOT)/../soc-repo-staging)

# Use the staged tree as the effective SoC repo after this file is included.
KCONFIG_EXT_PREFIX := $(SOC_STAGING)/

# -------------------------------------------------
# Patched files in the format: dst|src|diff
#   where:
#          dst  = output file path in staged SoC repo
#          src  = original file path in common kernel source
#          diff = patch file path in original SoC repo
# -------------------------------------------------
COMMON_PATCH_FILES := \
drivers/regulator/qti_fixed_regulator.c|drivers/regulator/fixed.c|drivers/regulator/fixed.diff \
drivers/dma/qcom/gpi_fixed.c|drivers/dma/qcom/gpi.c|drivers/dma/qcom/gpi_fix.diff \
drivers/net/virtio_net.c|drivers/net/virtio_net.c|drivers/net/virtio_net_fix.diff

# -------------------------------------------------
# Helpers
# -------------------------------------------------
define SYNC_COMMON_BASE
    $(MKDIR_P) $(SOC_STAGING); \
    $(RSYNC) -a \
        --prune-empty-dirs \
        --include='/drivers/' \
        --include='/drivers/**/' \
        --include='/net/' \
        --include='/net/**/' \
        --include='*.c' \
        --include='*.h' \
        --exclude='*' \
        $(SRC_ROOT)/ $(SOC_STAGING)/
endef

define OVERLAY_SOC_REPO
    $(MKDIR_P) $(SOC_STAGING); \
    $(RSYNC) -a \
        --exclude='.git/' \
        --exclude='.gitignore' \
        $(EXT_ROOT)/ $(SOC_STAGING)/
endef

define PATCHED_DRIVERS
    rec="$(1)"; \
    dst="$${rec%%|*}"; \
    tmp="$${rec#*|}"; \
    src="$${tmp%%|*}"; \
    diff="$${tmp#*|}"; \
    $(MKDIR_P) $(SOC_STAGING)/$$(dirname $$dst); \
    $(PATCH) --follow-symlinks -p0 --batch \
        -o $(SOC_STAGING)/$$dst \
        -i $(EXT_ROOT)/$$diff \
        $(SRC_ROOT)/$$src;
endef

# -------------------------------------------------
# Public macro : To be called from Makefile
# -------------------------------------------------
define STAGE_COMMON_SOURCES
    $(call SYNC_COMMON_BASE)
    $(call OVERLAY_SOC_REPO)
    $(foreach p,$(COMMON_PATCH_FILES),$(call PATCHED_DRIVERS,$(p)))
endef
