# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(src)

obj-$(CONFIG_QPNP_SMB5) += qpnp-smb5-main.o
qpnp-smb5-main-objs += storm-watch.o step-chg-jeita.o battery.o smb5-lib.o pmic-voter.o battery-profile-loader.o smb5-iio.o smbx-get-chan.o qpnp-smb5.o schgm-flash.o

obj-$(CONFIG_QPNP_QG) += qcom-qpnp-qg.o
qcom-qpnp-qg-objs += qpnp-qg.o battery-profile-loader.o pmic-voter.o qg-util.o qg-soc.o qg-sdam.o qg-battery-profile.o qg-profile-lib.o fg-alg.o

obj-$(CONFIG_SMB1398_CHARGER) += qcom-smb1398-charger.o
qcom-smb1398-charger-objs += smb1398-charger.o pmic-voter.o

obj-$(CONFIG_SMB1355_SLAVE_CHARGER) += qcom-smb1355-charger.o
qcom-smb1355-charger-objs += smb1355-charger.o pmic-voter.o

obj-$(CONFIG_QPNP_SMBLITE) += qpnp-smblite-main.o
qpnp-smblite-main-objs += step-chg-jeita.o battery.o qpnp-smblite.o smblite-lib.o pmic-voter.o storm-watch.o battery-profile-loader.o schgm-flashlite.o smblite-iio.o smbx-get-chan.o smblite-remote-bms.o

obj-$(CONFIG_QTI_QBG) += qti-qbg-main.o
qti-qbg-main-objs += qti-qbg.o qbg-sdam.o qbg-battery-profile.o battery-profile-loader.o

