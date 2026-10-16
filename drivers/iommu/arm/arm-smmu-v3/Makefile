# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)
ccflags-y += -I$(src)/pkvm
ccflags-y += -I$(src)/../arm-smmu/

obj-$(CONFIG_ARM_SMMU_V3) += arm_smmu_v3.o
arm_smmu_v3-objs += arm-smmu-v3.o arm-smmu-v3-common-lib.o arm-smmu-v3-common.o arm-smmu-v3-qcom-pm.o
arm_smmu_v3-$(CONFIG_ARM_SMMU_V3_QCOM_VIRTIO) += arm-smmu-v3-qcom-virtio.o

obj-$(CONFIG_QCOM_SMMU_V2_V3_DISPATCHER) += qcom_smmu_v2_v3_dispatcher.o
qcom_smmu_v2_v3_dispatcher-objs += arm-smmu-qcom-pm.o qcom_smmu_dispatcher.o smmuv2_nesting.o arm-smmu-v3-kvm-nested.o arm-smmu-v3-common-lib.o

ifeq ($(CONFIG_QCOM_SMMU_V2_V3_DISPATCHER),m)
$(obj)/pkvm/kvm_nvhe.o: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/pkvm $(obj)/pkvm/kvm_nvhe.o

qcom_smmu_v2_v3_dispatcher-objs += pkvm/kvm_nvhe.o
endif
