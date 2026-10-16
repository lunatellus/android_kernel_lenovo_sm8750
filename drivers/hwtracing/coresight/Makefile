# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_CORESIGHT_CSR) += coresight-csr.o

obj-$(CONFIG_CORESIGHT_CTI) += coresight-cti.o
coresight-cti-objs += coresight-cti-core.o coresight-cti-platform.o coresight-cti-sysfs.o

obj-$(CONFIG_CORESIGHT_DUMMY) += coresight-dummy.o

obj-$(CONFIG_CORESIGHT_SOURCE_ETM4X) += coresight-etm4x.o
coresight-etm4x-objs += coresight-etm4x-cfg.o coresight-etm4x-core.o coresight-etm4x-sysfs.o

obj-$(CONFIG_CORESIGHT_LINKS_AND_SINKS) += coresight-funnel.o

obj-$(CONFIG_CORESIGHT) += coresight.o
coresight-objs += coresight-cfg-afdo.o coresight-cfg-preload.o coresight-config.o coresight-core.o coresight-etm-perf.o coresight-platform.o coresight-syscfg-configfs.o coresight-syscfg.o coresight-sysfs.o coresight-trace-id.o

obj-$(CONFIG_CORESIGHT_QMI) += coresight-qmi.o

obj-$(CONFIG_CORESIGHT_REMOTE_ETM) += coresight-remote-etm.o

obj-$(CONFIG_CORESIGHT_LINKS_AND_SINKS) += coresight-replicator.o

obj-$(CONFIG_CORESIGHT_STM) += coresight-stm.o

obj-$(CONFIG_CORESIGHT_TGU) += coresight-tgu.o

obj-$(CONFIG_CORESIGHT_LINK_AND_SINK_TMC) += coresight-tmc.o
coresight-tmc-objs += coresight-byte-cntr.o coresight-tmc-core.o coresight-tmc-etf.o coresight-tmc-etr.o coresight-tmc-usb.o

obj-$(CONFIG_CORESIGHT_REMOTE_ETM) += coresight-tmc-sec.o

obj-$(CONFIG_CORESIGHT_TPDA) += coresight-tpda.o

obj-$(CONFIG_CORESIGHT_TPDM) += coresight-tpdm.o

obj-$(CONFIG_CORESIGHT_TRACE_NOC) += coresight-trace-noc.o

obj-$(CONFIG_CORESIGHT_UETM) += coresight-uetm.o

