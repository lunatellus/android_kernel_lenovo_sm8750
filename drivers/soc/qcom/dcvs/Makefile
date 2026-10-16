# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_QCOM_BWMON) += bwmon.o

obj-$(CONFIG_QCOM_DCVS_FP) += dcvs_fp.o

obj-$(CONFIG_QCOM_MEMLAT) += memlat.o

obj-$(CONFIG_QCOM_DCVS) += qcom-dcvs.o
qcom-dcvs-objs += dcvs.o dcvs_epss.o dcvs_icc.o trace-dcvs.o

obj-$(CONFIG_QCOM_PMU_LIB) += qcom-pmu-lib.o
qcom-pmu-lib-objs += pmu_lib.o

obj-$(CONFIG_QTI_QCOM_SCMI_CLIENT) += qcom_scmi_client.o

obj-$(CONFIG_QCOM_LLCC_MISS) += llcc_miss.o

obj-$(CONFIG_QCOM_BUS_PROF) += trace-bus-prof.o

obj-$(CONFIG_QCOM_LLCC_OCCUPANCY) += llcc_occ.o

obj-$(CONFIG_QCOM_BUS_LAT) += mem_lat_prof.o

obj-$(CONFIG_QCOM_BWPROF_SCMI) += bwprof_scmi.o

obj-$(CONFIG_QTI_CPUFREQ_STATS_SCMI_V2) += cpufreq_stats_scmi_v2.o

