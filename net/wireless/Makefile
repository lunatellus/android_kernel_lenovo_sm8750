# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_CFG80211) += cfg80211.o
cfg80211-objs += ap.o chan.o core.o ethtool.o ibss.o mesh.o mlme.o nl80211.o ocb.o pmsr.o radiotap.o reg.o scan.o sme.o sysfs.o trace.o util.o
cfg80211-$(CONFIG_OF) += of.o
cfg80211-$(CONFIG_CFG80211_DEBUGFS) += debugfs.o
cfg80211-$(CONFIG_CFG80211_WEXT) += wext-compat.o wext-sme.o
cfg80211-$(CONFIG_CFG80211_USE_KERNEL_REGDB_KEYS) += shipped-certs.o

$(obj)/shipped-certs.c: $(sort $(wildcard $(src)/certs/*.hex))
	@$(kecho) "  GEN     $@"
	$(Q)(echo '#include "reg.h"'; \
	  echo 'const u8 shipped_regdb_certs[] = {'; \
	  echo | cat - $^ ; \
	  echo '};'; \
	  echo 'unsigned int shipped_regdb_certs_len = sizeof(shipped_regdb_certs);'; \
	 ) > $@
