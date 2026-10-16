# SPDX-License-Identifier: GPL-2.0

subdir-ccflags-y += -I$(srctree)/arch/arm64/kvm/hyp/include/
subdir-ccflags-y += -I$(srctree)/arch/arm64/kvm/hyp/include/module
subdir-ccflags-y += -I$(src)/..
subdir-ccflags-y += -DMODULE

hyp-obj-y := arm-smmu-v3-nested.o
hyp-obj-y += smmuv2_nesting.o
hyp-obj-y += qcom_smmu_dispatcher.o
hyp-obj-y += io-pgtable-arm.o
hyp-obj-y += io-pgtable-arm-common.o
hyp-obj-y += arm-smmu-v3-common-lib.o

$(obj)/hyp.lds: $(srctree)/arch/arm64/kvm/hyp/nvhe/module.lds.S FORCE
	$(call if_changed_dep,cpp_lds_S)

include $(srctree)/arch/arm64/kvm/hyp/nvhe/Makefile.nvhe
