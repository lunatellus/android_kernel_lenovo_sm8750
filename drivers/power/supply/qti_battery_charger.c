// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#define pr_fmt(fmt)	"BATTERY_CHG: %s: " fmt, __func__

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/device.h>
#include <linux/extcon-provider.h>
#include <linux/firmware.h>
#include <linux/hq_hw_info.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/rpmsg.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/reboot.h>
#include <linux/thermal.h>
#include <linux/soc/qcom/qti_pmic_glink.h>
#include <linux/soc/qcom/battery_charger.h>
#include <linux/soc/qcom/panel_event_notifier.h>
#ifdef CONFIG_QTI_CHARGER_NFC_VREG
#include "qti_charger_boost_lib.h"
#include <linux/kernel.h>
#endif
#include "qti_typec_class.h"

#define MSG_OWNER_BC			32778
#define MSG_TYPE_REQ_RESP		1
#define MSG_TYPE_NOTIFY			2

/* opcode for battery charger */
#define BC_SET_NOTIFY_REQ		0x04
#define BC_DISABLE_NOTIFY_REQ		0x05
#define BC_NOTIFY_IND			0x07
#define BC_BATTERY_INFORMATION_GET	0x09
#define BC_SHUTDOWN_NOTIFY_V2		0x22
#define BC_BATTERY_STATUS_GET		0x30
#define BC_BATTERY_STATUS_SET		0x31
#define BC_USB_STATUS_GET(port_id)	(0x32 | (port_id) << 16)
#define BC_USB_STATUS_SET(port_id)	(0x33 | (port_id) << 16)
#define BC_WLS_STATUS_GET		0x34
#define BC_WLS_STATUS_SET		0x35
/* Lenovo property channel */
#define BC_LENOVO_PROP_GET		0x60
#define BC_LENOVO_PROP_SET		0x61
#define BC_SHIP_MODE_REQ_SET		0x36
#define BC_WLS_FW_CHECK_UPDATE		0x40
#define BC_WLS_FW_PUSH_BUF_REQ		0x41
#define BC_WLS_FW_UPDATE_STATUS_RESP	0x42
#define BC_WLS_FW_PUSH_BUF_RESP		0x43
#define BC_WLS_FW_GET_VERSION		0x44
#define BC_SHUTDOWN_NOTIFY		0x47
#define BC_CHG_CTRL_LIMIT_EN		0x48
#define BC_BATTERY_ABNORMAL_NOTIFY	0x90
#define BC_HBOOST_VMAX_CLAMP_NOTIFY	0x79
#define BC_GENERIC_NOTIFY		0x80

/* Generic definitions */
#define CHEMISTRY_MAX_STR_LEN		4
#define MAX_STR_LEN			128
#define BC_WAIT_TIME_MS			1000
#define BC_FW_VER_WAIT_TIME_MS		1500
#define WLS_FW_PREPARE_TIME_MS		1000
#define WLS_FW_WAIT_TIME_MS		500
#define WLS_FW_UPDATE_TIME_MS		1000
#define WLS_FW_BUF_SIZE			128
#define CPS_WLS_FW_BUF_SIZE		256
#define DEFAULT_RESTRICT_FCC_UA		1000000

enum usb_connector_type {
	USB_CONNECTOR_TYPE_TYPEC,
	USB_CONNECTOR_TYPE_MICRO_USB,
};

enum usb_port_id {
	USB_1_PORT_ID,
	USB_2_PORT_ID,
	NUM_USB_PORTS,
};

enum psy_type {
	PSY_TYPE_BATTERY,
	PSY_TYPE_USB,
	PSY_TYPE_USB_2,
	PSY_TYPE_WLS,
	PSY_TYPE_LENOVO,
	PSY_TYPE_MAX,
};

enum huaqin_property_id {
	HQ_POWER_SUPPLY_PROP_CC_ORIENTATION,
	HQ_POWER_SUPPLY_PROP_CC_ORIENTATION2,
	HQ_POWER_SUPPLY_PROP_IC_MAIN_CHARGE,
	HQ_POWER_SUPPLY_PROP_IC_FG_1,
	HQ_POWER_SUPPLY_PROP_IC_FG_2,
	HQ_POWER_SUPPLY_PROP_IC_CP_1,
	HQ_POWER_SUPPLY_PROP_IC_CP_2,
	HQ_POWER_SUPPLY_PROP_IC_QI,
	HQ_POWER_SUPPLY_PROP_IC_PDPHY,
	HQ_POWER_SUPPLY_PROP_IC_QC,
	HQ_POWER_SUPPLY_PROP_BATTERY_MANUFACTURER_1,
	HQ_POWER_SUPPLY_PROP_BATTERY_MANUFACTURER_2,
	HQ_POWER_SUPPLY_PROP_FG_1_TEMP,
	HQ_POWER_SUPPLY_PROP_FG_2_TEMP,
	HQ_POWER_SUPPLY_PROP_FG_1_CURRENT,
	HQ_POWER_SUPPLY_PROP_FG_2_CURRENT,
	HQ_POWER_SUPPLY_PROP_FG_1_VOLTAGE,
	HQ_POWER_SUPPLY_PROP_FG_2_VOLTAGE,
	HQ_POWER_SUPPLY_PROP_FG_1_PRODUCE_DATE,
	HQ_POWER_SUPPLY_PROP_FG_2_PRODUCE_DATE,
	HQ_POWER_SUPPLY_PROP_FG_1_ACTIVATE_DATE,
	HQ_POWER_SUPPLY_PROP_FG_2_ACTIVATE_DATE,
	HQ_POWER_SUPPLY_PROP_FG_1_SOH,
	HQ_POWER_SUPPLY_PROP_FG_2_SOH,
	HQ_POWER_SUPPLY_PROP_FG_1_CYCLE_COUNT,
	HQ_POWER_SUPPLY_PROP_FG_2_CYCLE_COUNT,
	HQ_POWER_SUPPLY_PROP_FG_MONITOR_CMD,
	HQ_POWER_SUPPLY_PROP_INPUT_SUSPEND,
	HQ_POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLE,
	HQ_POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING,
	HQ_POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING_EU,
	HQ_POWER_SUPPLY_PROP_BATTERY_RECHARGE_SETTING,
	HQ_POWER_SUPPLY_PROP_BATTERY_MAINTENANCE,
	HQ_POWER_SUPPLY_PROP_BATTERY_MAINTENANCE20,
	HQ_POWER_SUPPLY_PROP_BATTERY_PRODUCE_DATE,
	HQ_POWER_SUPPLY_PROP_BATTERY_ACTIVATE_DATE,
	HQ_POWER_SUPPLY_PROP_BATTERY_SOH,
	HQ_POWER_SUPPLY_PROP_BATTERY_CV,
	HQ_POWER_SUPPLY_PROP_BATTERY_CHG_FV,
	HQ_POWER_SUPPLY_PROP_BATTERY_FAKE_CYCLE_COUNT,
	HQ_POWER_SUPPLY_PROP_BATTERY_FAKE_SOC,
	HQ_POWER_SUPPLY_PROP_BATTERY_ABNORMAL_STATE,
	HQ_POWER_SUPPLY_PROP_IC_SC760X_1,
	HQ_POWER_SUPPLY_PROP_IC_SC760X_2,
	HQ_POWER_SUPPLY_PROP_FG_1_SOC,
	HQ_POWER_SUPPLY_PROP_FG_2_SOC,
	HQ_POWER_SUPPLY_PROP_SC8541_MATSET_IBUS,
	HQ_POWER_SUPPLY_PROP_SC8541_SLAVE_IBUS,
	HQ_POWER_SUPPLY_PROP_IBUS_TOTAL,
	HQ_POWER_SUPPLY_PROP_USB1_ONLINE,
	HQ_POWER_SUPPLY_PROP_USB2_ONLINE,
	HQ_POWER_SUPPLY_PROP_SC8541_DUMP,
	HQ_POWER_SUPPLY_PROP_PPS_STATUS,
	HQ_POWER_SUPPLY_PROP_FG_1_BATTERY_USED,
	HQ_POWER_SUPPLY_PROP_FG_2_BATTERY_USED,
	HQ_POWER_SUPPLY_PROP_SC7603_LOWPOWER,
	HQ_POWER_SUPPLY_PROPERTIES_MAX,
};

enum sysfs_property {
	POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLE,
	POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING,
	POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING_EU,
	POWER_SUPPLY_PROP_BATTERY_RECHARGE_SETTING,
	POWER_SUPPLY_PROP_BATTERY_MAINTENANCE,
	POWER_SUPPLY_PROP_BATTERY_MAINTENANCE20,
	POWER_SUPPLY_PROP_BATTERY_SHIPPING_MODE,
	POWER_SUPPLY_PROP_BATTERY_PRODUCE_DATE,
	POWER_SUPPLY_PROP_BATTERY_ACTIVATE_DATE,
	POWER_SUPPLY_PROP_BATTERY_SOH,
	POWER_SUPPLY_PROP_BATTERY_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_BATTERY_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_BATTERY_CV,
	POWER_SUPPLY_PROP_BATTERY_CHG_FV,
	POWER_SUPPLY_PROP_BATTERY_FAKE_CYCLE_COUNT,
	POWER_SUPPLY_PROP_BATTERY_FAKE_SOC,
	POWER_SUPPLY_PROP_BATTERY_ABNORMAL_STATE,
};

enum ship_mode_type {
	SHIP_MODE_PMIC,
	SHIP_MODE_PACK_SIDE,
};

/* property ids */
enum battery_property_id {
	BATT_STATUS,
	BATT_HEALTH,
	BATT_PRESENT,
	BATT_CHG_TYPE,
	BATT_CAPACITY,
	BATT_SOH,
	BATT_VOLT_OCV,
	BATT_VOLT_NOW,
	BATT_VOLT_MAX,
	BATT_CURR_NOW,
	BATT_CHG_CTRL_LIM,
	BATT_CHG_CTRL_LIM_MAX,
	BATT_TEMP,
	BATT_TECHNOLOGY,
	BATT_CHG_COUNTER,
	BATT_CYCLE_COUNT,
	BATT_CHG_FULL_DESIGN,
	BATT_CHG_FULL,
	BATT_MODEL_NAME,
	BATT_TTF_AVG,
	BATT_TTE_AVG,
	BATT_RESISTANCE,
	BATT_POWER_NOW,
	BATT_POWER_AVG,
	BATT_CHG_CTRL_EN,
	BATT_CHG_CTRL_START_THR,
	BATT_CHG_CTRL_END_THR,
	BATT_CURR_AVG,
	BATT_PARALLEL_CELL_COUNT,
	BATT_PROP_MAX,
};

enum usb_property_id {
	USB_ONLINE,
	USB_VOLT_NOW,
	USB_VOLT_MAX,
	USB_CURR_NOW,
	USB_CURR_MAX,
	USB_INPUT_CURR_LIMIT,
	USB_TYPE,
	USB_ADAP_TYPE,
	USB_MOISTURE_DET_EN,
	USB_MOISTURE_DET_STS,
	USB_TEMP,
	USB_REAL_TYPE,
	USB_TYPEC_COMPLIANT,
	USB_SCOPE,
	USB_CONNECTOR_TYPE,
	F_ACTIVE,
	USB_NUM_PORTS,
	USB_PROP_MAX,
};

enum wireless_property_id {
	WLS_ONLINE,
	WLS_VOLT_NOW,
	WLS_VOLT_MAX,
	WLS_CURR_NOW,
	WLS_CURR_MAX,
	WLS_TYPE,
	WLS_BOOST_EN,
	WLS_HBOOST_VMAX,
	WLS_INPUT_CURR_LIMIT,
	WLS_ADAP_TYPE,
	WLS_CONN_TEMP,
	WLS_PROP_MAX,
};

enum {
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP = 0x80,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3,
	QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5,
};

struct battery_charger_set_notify_msg {
	struct pmic_glink_hdr	hdr;
	u32			battery_id;
	u32			power_state;
	u32			low_capacity;
	u32			high_capacity;
};

struct battery_charger_notify_msg {
	struct pmic_glink_hdr	hdr;
	u32			notification;
};

struct battery_charger_req_msg {
	struct pmic_glink_hdr	hdr;
	u32			battery_id;
	u32			property_id;
	u32			value;
};

struct battery_charger_resp_msg {
	struct pmic_glink_hdr	hdr;
	u32			property_id;
	u32			value;
	u32			ret_code;
};

struct battery_model_resp_msg {
	struct pmic_glink_hdr	hdr;
	u32			property_id;
	char			model[MAX_STR_LEN];
};

/* Response structure for BC_BATTERY_INFORMATION_GET (Opcode 0x09) */
struct battery_information_resp_msg {
	struct pmic_glink_hdr	hdr;
	u32 power_unit;
	u32 design_capacity;
	u32 last_full_capacity;
	u32 battery_tech;
	u32 design_voltage;
	u32 capacity_warning;
	u32 capacity_low;
	u32 cycle_count;
	u32 accuracy;
	u32 max_sample_time;
	u32 min_sample_time;
	u32 max_average_interval;
	u32 min_average_interval;
	u32 capacity_granularity1;
	u32 capacity_granularity2;
	u32 swappable;
	u32 capabilities;
	char model_number[MAX_STR_LEN];
	char serial_number[MAX_STR_LEN];
	char battery_type[MAX_STR_LEN];
	char oem_info[MAX_STR_LEN];
	char battery_chemistry[CHEMISTRY_MAX_STR_LEN];
	char uid[MAX_STR_LEN];
	u32 critical_bias;
	u8 day;
	u8 month;
	u16 year;
	u32 battery_id;
	u32 batt_estimated_time;
};

struct wireless_fw_check_req {
	struct pmic_glink_hdr	hdr;
	u32			fw_version;
	u32			fw_size;
	u32			fw_crc;
};

struct wireless_fw_check_resp {
	struct pmic_glink_hdr	hdr;
	u32			ret_code;
};

struct wireless_fw_push_buf_req {
	struct pmic_glink_hdr	hdr;
	u8			buf[CPS_WLS_FW_BUF_SIZE];
	u32			fw_chunk_id;
};

struct wireless_fw_push_buf_resp {
	struct pmic_glink_hdr	hdr;
	u32			fw_update_status;
};

struct wireless_fw_update_status {
	struct pmic_glink_hdr	hdr;
	u32			fw_update_done;
};

struct wireless_fw_get_version_req {
	struct pmic_glink_hdr	hdr;
};

struct wireless_fw_get_version_resp {
	struct pmic_glink_hdr	hdr;
	u32			fw_version;
};

struct battery_charger_ship_mode_req_msg {
	struct pmic_glink_hdr	hdr;
	u32			ship_mode_type;
};

struct battery_charger_chg_ctrl_msg {
	struct pmic_glink_hdr	hdr;
	u32			enable;
	u32			target_soc;
	u32			delta_soc;
};

struct psy_state {
	struct power_supply	*psy;
	char			*model;
	char			*manufacturer;
	const int		*map;
	u32			*prop;
	u32			prop_count;
	u32			opcode_get;
	u32			opcode_set;
};

struct battery_charger_config {
	u32			opcode;
	const struct power_supply_desc	*batt_psy_desc;
};

struct battery_chg_dev {
	struct device			*dev;
	struct class			battery_class;
	struct pmic_glink_client	*client;
	struct typec_role_class		*typec_class;
	struct mutex			rw_lock;
	struct rw_semaphore		state_sem;
	struct completion		ack;
	struct completion		fw_buf_ack;
	struct completion		fw_update_ack;
	struct psy_state		psy_list[PSY_TYPE_MAX];
	struct dentry			*debugfs_dir;
	void				*notifier_cookie;
	/* extcon for VBUS/ID notification for USB for micro USB */
	struct extcon_dev		*extcon;
	u32				*thermal_levels;
	const char			*wls_fw_name;
	int				curr_thermal_level;
	int				num_thermal_levels;
	int				shutdown_volt_mv;
	atomic_t			state;
	struct work_struct		subsys_up_work;
	struct work_struct		usb_type_work;
	struct work_struct		battery_check_work;
	struct wakeup_source		*charge_wakelock;
	int				fake_soc;
	bool				block_tx;
	bool				ship_mode_en;
	bool				ship_mode_immediate;
	bool				debug_battery_detected;
	bool				wls_not_supported;
	bool				wls_fw_update_reqd;
	u32				wls_fw_version;
	u16				wls_fw_crc;
	u32				wls_fw_update_time_ms;
	struct notifier_block		reboot_notifier;
	u32				thermal_fcc_ua;
	u32				restrict_fcc_ua;
	u32				last_fcc_ua;
	u32				usb_icl_ua[NUM_USB_PORTS];
	u32				thermal_fcc_step;
	u32				connector_type;
	u32				usb_prev_mode;
	bool				restrict_chg_en;
	u8				chg_ctrl_start_thr;
	u8				chg_ctrl_end_thr;
	bool				chg_ctrl_en;
	bool				usb_active[NUM_USB_PORTS];
	/* To track the driver initialization status */
	bool				initialized;
	bool				notify_en;
	bool				is_suspended;
	struct mutex			suspend_lock;
	bool				error_prop;
	struct delayed_work		lenovo_chg_monitor_work;
	struct delayed_work		device_info_work;
	struct thermal_zone_device	*usb1_therm;
	struct thermal_zone_device	*usb2_therm;
	bool				battery_abnormal;
	int				fake_abnormal_level;
	bool				lenovo_chg_initialized;
	bool				micro_usb;
	const struct battery_charger_config	*config;
#ifdef CONFIG_QTI_CHARGER_NFC_VREG
	struct bharger_regulator	nfc_vreg;
#endif
	u32				num_usb_ports;
};

static struct battery_chg_dev *gbcdev;

static const int battery_prop_map[BATT_PROP_MAX] = {
	[BATT_STATUS]		= POWER_SUPPLY_PROP_STATUS,
	[BATT_HEALTH]		= POWER_SUPPLY_PROP_HEALTH,
	[BATT_PRESENT]		= POWER_SUPPLY_PROP_PRESENT,
	[BATT_CHG_TYPE]		= POWER_SUPPLY_PROP_CHARGE_TYPE,
	[BATT_CAPACITY]		= POWER_SUPPLY_PROP_CAPACITY,
	[BATT_VOLT_OCV]		= POWER_SUPPLY_PROP_VOLTAGE_OCV,
	[BATT_VOLT_NOW]		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
	[BATT_VOLT_MAX]		= POWER_SUPPLY_PROP_VOLTAGE_MAX,
	[BATT_CURR_NOW]		= POWER_SUPPLY_PROP_CURRENT_NOW,
	[BATT_CHG_CTRL_LIM]	= POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	[BATT_CHG_CTRL_LIM_MAX]	= POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	[BATT_TEMP]		= POWER_SUPPLY_PROP_TEMP,
	[BATT_TECHNOLOGY]	= POWER_SUPPLY_PROP_TECHNOLOGY,
	[BATT_CHG_COUNTER]	= POWER_SUPPLY_PROP_CHARGE_COUNTER,
	[BATT_CYCLE_COUNT]	= POWER_SUPPLY_PROP_CYCLE_COUNT,
	[BATT_CHG_FULL_DESIGN]	= POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	[BATT_CHG_FULL]		= POWER_SUPPLY_PROP_CHARGE_FULL,
	[BATT_MODEL_NAME]	= POWER_SUPPLY_PROP_MODEL_NAME,
	[BATT_TTF_AVG]		= POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	[BATT_TTE_AVG]		= POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	[BATT_POWER_NOW]	= POWER_SUPPLY_PROP_POWER_NOW,
	[BATT_POWER_AVG]	= POWER_SUPPLY_PROP_POWER_AVG,
	[BATT_CHG_CTRL_START_THR] = POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD,
	[BATT_CHG_CTRL_END_THR]   = POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD,
	[BATT_CURR_AVG]		= POWER_SUPPLY_PROP_CURRENT_AVG,
};

static const int hq_prop_map[HQ_POWER_SUPPLY_PROPERTIES_MAX] = { 0 };

static const int usb_prop_map[USB_PROP_MAX] = {
	[USB_ONLINE]		= POWER_SUPPLY_PROP_ONLINE,
	[USB_VOLT_NOW]		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
	[USB_VOLT_MAX]		= POWER_SUPPLY_PROP_VOLTAGE_MAX,
	[USB_CURR_NOW]		= POWER_SUPPLY_PROP_CURRENT_NOW,
	[USB_CURR_MAX]		= POWER_SUPPLY_PROP_CURRENT_MAX,
	[USB_INPUT_CURR_LIMIT]	= POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	[USB_ADAP_TYPE]		= POWER_SUPPLY_PROP_USB_TYPE,
	[USB_TEMP]		= POWER_SUPPLY_PROP_TEMP,
	[USB_SCOPE]		= POWER_SUPPLY_PROP_SCOPE,
};

static const int wls_prop_map[WLS_PROP_MAX] = {
	[WLS_ONLINE]		= POWER_SUPPLY_PROP_ONLINE,
	[WLS_VOLT_NOW]		= POWER_SUPPLY_PROP_VOLTAGE_NOW,
	[WLS_VOLT_MAX]		= POWER_SUPPLY_PROP_VOLTAGE_MAX,
	[WLS_CURR_NOW]		= POWER_SUPPLY_PROP_CURRENT_NOW,
	[WLS_CURR_MAX]		= POWER_SUPPLY_PROP_CURRENT_MAX,
	[WLS_INPUT_CURR_LIMIT]	= POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	[WLS_CONN_TEMP]		= POWER_SUPPLY_PROP_TEMP,
};

static const unsigned int bcdev_usb_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

/* Standard usb_type definitions similar to power_supply_sysfs.c */
static const char * const power_supply_usb_type_text[] = {
	"Unknown", "SDP", "DCP", "CDP", "ACA", "C",
	"PD", "PD_DRP", "PD_PPS", "BrickID"
};

/* Custom usb_type definitions */
static const char * const qc_power_supply_usb_type_text[] = {
	"HVDCP", "HVDCP_3", "HVDCP_3P5"
};

/* wireless_type definitions */
static const char * const qc_power_supply_wls_type_text[] = {
	"Unknown", "BPP", "EPP", "HPP"
};

static RAW_NOTIFIER_HEAD(hboost_notifier);

int register_hboost_event_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&hboost_notifier, nb);
}
EXPORT_SYMBOL(register_hboost_event_notifier);

int unregister_hboost_event_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&hboost_notifier, nb);
}
EXPORT_SYMBOL(unregister_hboost_event_notifier);

static int battery_chg_fw_write(struct battery_chg_dev *bcdev, void *data,
				int len)
{
	int rc;

	down_read(&bcdev->state_sem);
	if (atomic_read(&bcdev->state) == PMIC_GLINK_STATE_DOWN) {
		pr_debug("glink state is down\n");
		up_read(&bcdev->state_sem);
		return -ENOTCONN;
	}
	up_read(&bcdev->state_sem);

	reinit_completion(&bcdev->fw_buf_ack);
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->fw_buf_ack,
					msecs_to_jiffies(WLS_FW_WAIT_TIME_MS));
		if (!rc) {
			pr_err("Error, timed out sending message\n");
			return -ETIMEDOUT;
		}

		rc = 0;
	}

	return rc;
}

static int battery_chg_write(struct battery_chg_dev *bcdev, void *data,
				int len, u32 wait_time)
{
	int rc;

	/*
	 * When the subsystem goes down, it's better to return the last
	 * known values until it comes back up. Hence, return 0 so that
	 * pmic_glink_write() is not attempted until pmic glink is up.
	 */
	down_read(&bcdev->state_sem);
	if (atomic_read(&bcdev->state) == PMIC_GLINK_STATE_DOWN) {
		pr_debug("glink state is down\n");
		up_read(&bcdev->state_sem);
		return 0;
	}
	up_read(&bcdev->state_sem);

	if (bcdev->debug_battery_detected && bcdev->block_tx) {
		return 0;
	}

	mutex_lock(&bcdev->rw_lock);
	reinit_completion(&bcdev->ack);
	bcdev->error_prop = false;
	rc = pmic_glink_write(bcdev->client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->ack,
					msecs_to_jiffies(wait_time));
		if (!rc) {
			pr_err("Error, timed out sending message\n");
			mutex_unlock(&bcdev->rw_lock);
			return -ETIMEDOUT;
		}
		rc = 0;

		/*
		 * In case the opcode used is not supported, the remote
		 * processor might ack it immediately with a return code indicating
		 * an error. This additional check is to check if such an error has
		 * happened and return immediately with error in that case. This
		 * avoids wasting time waiting in the above timeout condition for this
		 * type of error.
		 */
		if (bcdev->error_prop) {
			bcdev->error_prop = false;
			rc = -ENODATA;
		}
	}
	mutex_unlock(&bcdev->rw_lock);

	return rc;
}

static int write_property_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id, u32 val)
{
	struct battery_charger_req_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.battery_id = 0;
	req_msg.value = val;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_set;

	if (pst->psy)
		pr_debug("psy: %s prop_id: %u val: %u\n", pst->psy->desc->name,
			req_msg.property_id, val);

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg), BC_WAIT_TIME_MS);
}

static int read_property_id(struct battery_chg_dev *bcdev,
			struct psy_state *pst, u32 prop_id)
{
	struct battery_charger_req_msg req_msg = { { 0 } };

	req_msg.property_id = prop_id;
	req_msg.battery_id = 0;
	req_msg.value = 0;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = pst->opcode_get;

	if (pst->psy)
		pr_debug("psy: %s prop_id: %u\n", pst->psy->desc->name,
			req_msg.property_id);

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg), BC_WAIT_TIME_MS);
}

static int read_battery_information(struct battery_chg_dev *bcdev)
{
	struct battery_charger_req_msg req_msg = { { 0 } };

	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = BC_BATTERY_INFORMATION_GET;
	req_msg.battery_id = 0;

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg), BC_WAIT_TIME_MS);
}

static int get_property_id(struct psy_state *pst,
			enum power_supply_property prop)
{
	u32 i;

	for (i = 0; i < pst->prop_count; i++)
		if (pst->map[i] == prop)
			return i;

	if (pst->psy)
		pr_err("No property id for property %d in psy %s\n", prop,
			pst->psy->desc->name);

	return -ENOENT;
}

static void battery_chg_notify_disable(struct battery_chg_dev *bcdev)
{
	struct battery_charger_set_notify_msg req_msg = { { 0 } };
	int rc;

	if (bcdev->notify_en) {
		/* Send request to disable notification */
		req_msg.hdr.owner = MSG_OWNER_BC;
		req_msg.hdr.type = MSG_TYPE_NOTIFY;
		req_msg.hdr.opcode = BC_DISABLE_NOTIFY_REQ;

		rc = battery_chg_write(bcdev, &req_msg, sizeof(req_msg), BC_WAIT_TIME_MS);
		if (rc < 0)
			pr_err("Failed to disable notification rc=%d\n", rc);
		else
			bcdev->notify_en = false;
	}
}

static void battery_chg_notify_enable(struct battery_chg_dev *bcdev)
{
	struct battery_charger_set_notify_msg req_msg = { { 0 } };
	int rc;

	if (!bcdev->notify_en) {
		/* Send request to enable notification */
		req_msg.hdr.owner = MSG_OWNER_BC;
		req_msg.hdr.type = MSG_TYPE_NOTIFY;
		req_msg.hdr.opcode = BC_SET_NOTIFY_REQ;

		rc = battery_chg_write(bcdev, &req_msg, sizeof(req_msg), BC_WAIT_TIME_MS);
		if (rc < 0)
			pr_err("Failed to enable notification rc=%d\n", rc);
		else
			bcdev->notify_en = true;
	}
}

static void battery_chg_state_cb(void *priv, enum pmic_glink_state state)
{
	struct battery_chg_dev *bcdev = priv;

	pr_debug("state: %d\n", state);

	down_write(&bcdev->state_sem);
	if (!bcdev->initialized) {
		pr_warn("Driver not initialized, pmic_glink state %d\n", state);
		up_write(&bcdev->state_sem);
		return;
	}
	atomic_set(&bcdev->state, state);
	up_write(&bcdev->state_sem);

	if (state == PMIC_GLINK_STATE_UP)
		schedule_work(&bcdev->subsys_up_work);
	else if (state == PMIC_GLINK_STATE_DOWN)
		bcdev->notify_en = false;
}

/**
 * qti_battery_charger_get_prop() - Gets the property being requested
 *
 * @name: Power supply name
 * @prop_id: Property id to be read
 * @val: Pointer to value that needs to be updated
 *
 * Return: 0 if success, negative on error.
 */
int qti_battery_charger_get_prop(const char *name,
				enum battery_charger_prop prop_id, int *val)
{
	struct power_supply *psy;
	struct battery_chg_dev *bcdev;
	struct psy_state *pst;
	int rc = 0;

	if (prop_id >= BATTERY_CHARGER_PROP_MAX)
		return -EINVAL;

	if (strcmp(name, "battery") && strcmp(name, "usb") &&
	    strcmp(name, "usb-2") && strcmp(name, "wireless"))
		return -EINVAL;

	psy = power_supply_get_by_name(name);
	if (!psy)
		return -ENODEV;

	bcdev = power_supply_get_drvdata(psy);
	if (!bcdev)
		return -ENODEV;

	power_supply_put(psy);

	switch (prop_id) {
	case BATTERY_RESISTANCE:
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
		rc = read_property_id(bcdev, pst, BATT_RESISTANCE);
		if (!rc)
			*val = pst->prop[BATT_RESISTANCE];
		break;
	default:
		break;
	}

	return rc;
}
EXPORT_SYMBOL(qti_battery_charger_get_prop);

static bool validate_message(struct battery_chg_dev *bcdev,
			struct battery_charger_resp_msg *resp_msg, size_t len)
{
	if (len != sizeof(*resp_msg)) {
		pr_err("Incorrect response length %zu for opcode %#x\n", len,
			resp_msg->hdr.opcode);
		return false;
	}

	if (resp_msg->ret_code) {
		pr_err_ratelimited("Error in response for opcode %#x prop_id %u, rc=%d\n",
			resp_msg->hdr.opcode, resp_msg->property_id,
			(int)resp_msg->ret_code);
		bcdev->error_prop = true;
		return false;
	}

	return true;
}

static void handle_battery_info(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
	struct battery_information_resp_msg *info_msg = data;
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

	if (len != sizeof(*info_msg)) {
		pr_err("Incorrect response length %zu for BC_BATTERY_INFORMATION_GET\n", len);
		return;
	}

	if (!pst->manufacturer) {
		pr_err("manufacturer buffer not allocated\n");
		return;
	}

	strscpy(pst->manufacturer, info_msg->oem_info, MAX_STR_LEN);
}

#define MODEL_DEBUG_BOARD	"Debug_Board"
static void handle_message(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
	struct battery_charger_resp_msg *resp_msg = data;
	struct battery_model_resp_msg *model_resp_msg = data;
	struct wireless_fw_check_resp *fw_check_msg;
	struct wireless_fw_push_buf_resp *fw_resp_msg;
	struct wireless_fw_update_status *fw_update_msg;
	struct wireless_fw_get_version_resp *fw_ver_msg;
	struct psy_state *pst;
	bool ack_set = false;

	switch (resp_msg->hdr.opcode) {
	case BC_BATTERY_INFORMATION_GET:
		handle_battery_info(bcdev, data, len);
		ack_set = true;
		break;
	case BC_BATTERY_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];

		/* Handle model response uniquely as it's a string */
		if (pst->model && len == sizeof(*model_resp_msg)) {
			memcpy(pst->model, model_resp_msg->model, MAX_STR_LEN);
			ack_set = true;
			bcdev->debug_battery_detected = !strcmp(pst->model,
					MODEL_DEBUG_BOARD);
			break;
		}

		/* Other response should be of same type as they've u32 value */
		if (validate_message(bcdev, resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	case BC_USB_STATUS_GET(USB_1_PORT_ID):
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		if (validate_message(bcdev, resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	case BC_USB_STATUS_GET(USB_2_PORT_ID):
		if (bcdev->num_usb_ports < 2) {
			pr_debug("opcode: %u for missing USB_2 port\n",
					resp_msg->hdr.opcode);
			break;
		}
		pst = &bcdev->psy_list[PSY_TYPE_USB_2];
		if (validate_message(bcdev, resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	case BC_LENOVO_PROP_GET:
		pst = &bcdev->psy_list[PSY_TYPE_LENOVO];
		if (validate_message(bcdev, resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	case BC_WLS_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_WLS];
		if (validate_message(bcdev, resp_msg, len) &&
		    resp_msg->property_id < pst->prop_count) {
			pst->prop[resp_msg->property_id] = resp_msg->value;
			ack_set = true;
		}

		break;
	case BC_USB_STATUS_SET(USB_2_PORT_ID):
		if (bcdev->num_usb_ports < 2) {
			pr_debug("opcode: %u for missing USB_2 port\n",
					resp_msg->hdr.opcode);
			break;
		}
		fallthrough;
	case BC_BATTERY_STATUS_SET:
	case BC_USB_STATUS_SET(USB_1_PORT_ID):
	case BC_WLS_STATUS_SET:
	case BC_LENOVO_PROP_SET:
		if (validate_message(bcdev, data, len))
			ack_set = true;

		break;
	case BC_SET_NOTIFY_REQ:
	case BC_DISABLE_NOTIFY_REQ:
	case BC_SHUTDOWN_NOTIFY:
	case BC_SHUTDOWN_NOTIFY_V2:
	case BC_SHIP_MODE_REQ_SET:
	case BC_CHG_CTRL_LIMIT_EN:
		/* Always ACK response for notify or ship_mode request */
		ack_set = true;
		break;
	case BC_WLS_FW_CHECK_UPDATE:
		if (len == sizeof(*fw_check_msg)) {
			fw_check_msg = data;
			if (fw_check_msg->ret_code == 1)
				bcdev->wls_fw_update_reqd = true;
			ack_set = true;
		} else {
			pr_err("Incorrect response length %zu for wls_fw_check_update\n",
				len);
		}
		break;
	case BC_WLS_FW_PUSH_BUF_RESP:
		if (len == sizeof(*fw_resp_msg)) {
			fw_resp_msg = data;
			if (fw_resp_msg->fw_update_status == 1)
				complete(&bcdev->fw_buf_ack);
		} else {
			pr_err("Incorrect response length %zu for wls_fw_push_buf_resp\n",
				len);
		}
		break;
	case BC_WLS_FW_UPDATE_STATUS_RESP:
		if (len == sizeof(*fw_update_msg)) {
			fw_update_msg = data;
			if (fw_update_msg->fw_update_done == 1)
				complete(&bcdev->fw_update_ack);
			else
				pr_err("Wireless FW update not done %d\n",
					(int)fw_update_msg->fw_update_done);
		} else {
			pr_err("Incorrect response length %zu for wls_fw_update_status_resp\n",
				len);
		}
		break;
	case BC_WLS_FW_GET_VERSION:
		if (len == sizeof(*fw_ver_msg)) {
			fw_ver_msg = data;
			bcdev->wls_fw_version = fw_ver_msg->fw_version;
			ack_set = true;
		} else {
			pr_err("Incorrect response length %zu for wls_fw_get_version\n",
				len);
		}
		break;
#ifdef CONFIG_QTI_CHARGER_NFC_VREG
	case BC_CHG_CTRL_EN_W_BOOST:
		ack_set = true;
		break;
#endif
	default:
		pr_err("Unknown opcode: %u\n", resp_msg->hdr.opcode);
		break;
	}

	if (ack_set || bcdev->error_prop)
		complete(&bcdev->ack);
}

static void battery_chg_update_uusb_type(struct battery_chg_dev *bcdev,
					 u32 adap_type)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;
	u32 scope;

	/* Handle the extcon notification for uUSB case only */
	if ((bcdev->connector_type != USB_CONNECTOR_TYPE_MICRO_USB) ||
	    !bcdev->extcon || !bcdev->typec_class)
		return;

	rc = read_property_id(bcdev, pst, USB_SCOPE);
	if (rc < 0) {
		dev_err(bcdev->dev, "Failed to read USB_SCOPE rc=%d\n", rc);
		return;
	}
	scope = pst->prop[USB_SCOPE];

	switch (scope) {
	case POWER_SUPPLY_SCOPE_DEVICE:
		if (adap_type == POWER_SUPPLY_USB_TYPE_SDP ||
		    adap_type == POWER_SUPPLY_USB_TYPE_CDP) {
			/* Device mode connect notification */
			extcon_set_state_sync(bcdev->extcon, EXTCON_USB, 1);
			bcdev->usb_prev_mode = EXTCON_USB;
			rc = qti_typec_partner_register(bcdev->typec_class,
							TYPEC_DEVICE);
			if (rc < 0)
				dev_err(bcdev->dev, "Failed to register typec partner rc=%d\n",
					rc);
		}
		break;
	case POWER_SUPPLY_SCOPE_SYSTEM:
		/* Host mode connect notification */
		extcon_set_state_sync(bcdev->extcon, EXTCON_USB_HOST, 1);
		bcdev->usb_prev_mode = EXTCON_USB_HOST;
		rc = qti_typec_partner_register(bcdev->typec_class, TYPEC_HOST);
		if (rc < 0)
			dev_err(bcdev->dev, "Failed to register typec partner rc=%d\n",
				rc);
		break;
	default:
		if (bcdev->usb_prev_mode == EXTCON_USB ||
		    bcdev->usb_prev_mode == EXTCON_USB_HOST) {
			/* Disconnect notification */
			extcon_set_state_sync(bcdev->extcon,
					      bcdev->usb_prev_mode, 0);
			bcdev->usb_prev_mode = EXTCON_NONE;
			qti_typec_partner_unregister(bcdev->typec_class);
		}
		break;
	}
}

static struct power_supply_desc usb_psy_desc[NUM_USB_PORTS];

static bool is_offmode(void)
{
	static struct device_node *np;
	static bool offmode;
	const char *bootargs;

	if (np)
		return offmode;

	bootargs = NULL;
	np = of_find_node_by_path("/chosen");
	if (np) {
		of_property_read_string(np, "bootargs", &bootargs);
		if (!bootargs)
			pr_err("%s: failed to get bootargs property\n", __func__);
		else if (strstr(bootargs, "offmode.charge=1"))
			offmode = true;
	}

	pr_err("offmode.charge=%d\n", offmode);
	return offmode;
}

static void battery_chg_update_usb_type_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev, usb_type_work);
	struct power_supply_desc *desc;
	struct psy_state *pst = NULL;
	struct psy_state *batt_pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	struct psy_state *wls_pst = &bcdev->psy_list[PSY_TYPE_WLS];
	int rc, i;

	for (i = 0; i < NUM_USB_PORTS; i++) {
		if (!bcdev->usb_active[i])
			continue;

		bcdev->usb_active[i] = false;

		switch (i) {
		case USB_1_PORT_ID:
			pst = &bcdev->psy_list[PSY_TYPE_USB];
			break;
		case USB_2_PORT_ID:
			pst = &bcdev->psy_list[PSY_TYPE_USB_2];
			break;
		default:
			pr_err_ratelimited("USB port %d not supported\n", i);
			continue;
		}

		desc = &usb_psy_desc[i];

		rc = read_property_id(bcdev, pst, USB_ADAP_TYPE);
		if (rc < 0) {
			pr_err("Failed to read USB_ADAP_TYPE rc=%d\n", rc);
			continue;
		}

		if (pst->prop[USB_ADAP_TYPE] == POWER_SUPPLY_USB_TYPE_UNKNOWN) {
			desc->type = POWER_SUPPLY_TYPE_USB;
			pr_debug("Unknown USB adapter type\n");
			continue;
		}

		/* Reset usb_icl_ua whenever USB adapter type changes */
		if (pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_SDP &&
		    pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_CDP &&
		    pst->prop[USB_ADAP_TYPE] != POWER_SUPPLY_USB_TYPE_PD)
			bcdev->usb_icl_ua[i] = 0;

		pr_debug("usb_adap_type: %u\n", pst->prop[USB_ADAP_TYPE]);

		switch (pst->prop[USB_ADAP_TYPE]) {
		case POWER_SUPPLY_USB_TYPE_SDP:
			desc->type = POWER_SUPPLY_TYPE_USB;
			break;
		case POWER_SUPPLY_USB_TYPE_DCP:
		case POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID:
		case QTI_POWER_SUPPLY_USB_TYPE_HVDCP:
		case QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3:
		case QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5:
			desc->type = POWER_SUPPLY_TYPE_USB_DCP;
			break;
		case POWER_SUPPLY_USB_TYPE_CDP:
			desc->type = POWER_SUPPLY_TYPE_USB_CDP;
			break;
		case POWER_SUPPLY_USB_TYPE_ACA:
			desc->type = POWER_SUPPLY_TYPE_USB_ACA;
			break;
		case POWER_SUPPLY_USB_TYPE_C:
			desc->type = POWER_SUPPLY_TYPE_USB_TYPE_C;
			break;
		case POWER_SUPPLY_USB_TYPE_PD:
		case POWER_SUPPLY_USB_TYPE_PD_DRP:
		case POWER_SUPPLY_USB_TYPE_PD_PPS:
			desc->type = POWER_SUPPLY_TYPE_USB_PD;
			break;
		default:
			desc->type = POWER_SUPPLY_TYPE_USB;
			break;
		}
	}

	pst = &bcdev->psy_list[PSY_TYPE_USB];

	rc = read_property_id(bcdev, batt_pst, BATT_STATUS);
	if (rc < 0) {
		pr_err("Failed to read BATT_STATUS, rc=%d\n", rc);
		goto out_uusb;
	}

	rc = read_property_id(bcdev, wls_pst, WLS_ADAP_TYPE);
	if (rc < 0) {
		pr_err("Failed to read WLS_ADAP_TYPE rc=%d\n", rc);
		goto out_uusb;
	}

	rc = read_property_id(bcdev, pst, USB_REAL_TYPE);
	if (rc < 0)
		pr_err("Failed to read USB_REAL_TYPE rc=%d\n", rc);

	rc = read_property_id(bcdev, pst, USB_VOLT_NOW);
	if (rc < 0)
		pr_err("Failed to read USB_VOLT_NOW rc=%d\n", rc);

	rc = read_property_id(bcdev, wls_pst, WLS_VOLT_NOW);
	if (rc < 0)
		pr_err("Failed to read WLS_VOLT_NOW rc=%d\n", rc);

	if (!pst->prop[USB_ADAP_TYPE] && !wls_pst->prop[WLS_ADAP_TYPE])
		goto release_wakelock;
	if (batt_pst->prop[BATT_STATUS] == POWER_SUPPLY_STATUS_FULL)
		goto release_wakelock;
	if (batt_pst->prop[BATT_STATUS] == POWER_SUPPLY_STATUS_NOT_CHARGING &&
	    !is_offmode())
		goto release_wakelock;
	if (bcdev->charge_wakelock && !bcdev->charge_wakelock->active) {
		pr_info("hold wakelock!!! usb_type:[%d,%d], status:%d\n ",
			pst->prop[USB_ADAP_TYPE],
			wls_pst->prop[WLS_ADAP_TYPE],
			batt_pst->prop[BATT_STATUS]);
		__pm_stay_awake(bcdev->charge_wakelock);
	}
	goto update_lenovo;

release_wakelock:
	if (bcdev->charge_wakelock && bcdev->charge_wakelock->active) {
		pr_info("release wakelock!!! usb_type:[%d,%d], status:%d\n ",
			pst->prop[USB_ADAP_TYPE],
			wls_pst->prop[WLS_ADAP_TYPE],
			batt_pst->prop[BATT_STATUS]);
		__pm_relax(bcdev->charge_wakelock);
	}

update_lenovo:
	if (bcdev->lenovo_chg_initialized)
		mod_delayed_work(system_wq, &bcdev->lenovo_chg_monitor_work, 0);

out_uusb:
	if (bcdev->micro_usb && pst)
		battery_chg_update_uusb_type(bcdev, pst->prop[USB_ADAP_TYPE]);
}

static void battery_chg_check_status_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev,
					battery_check_work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	struct psy_state *lenovo_pst =
		&bcdev->psy_list[PSY_TYPE_LENOVO];
	char *envp[][2] = {
		{ "BATTERY_STATE=LENOVO_CHARGE_EVENT_0", NULL },
		{ "BATTERY_STATE=LENOVO_CHARGE_EVENT_1", NULL },
		{ "BATTERY_STATE=LENOVO_CHARGE_EVENT_2", NULL },
		{ "BATTERY_STATE=LENOVO_CHARGE_EVENT_3", NULL },
		{ "BATTERY_STATE=LENOVO_CHARGE_EVENT_4", NULL },
	};
	u32 abnormal_state;
	int rc;

	if (bcdev->battery_abnormal) {
		rc = read_property_id(bcdev, lenovo_pst,
				      HQ_POWER_SUPPLY_PROP_BATTERY_ABNORMAL_STATE);
		if (rc < 0) {
			pr_err("Failed to read BATTERY_ABNORMAL_STATE, rc=%d\n",
			       rc);
			return;
		}

		if (bcdev->fake_abnormal_level >= 0 &&
		    bcdev->fake_abnormal_level < 5)
			abnormal_state = bcdev->fake_abnormal_level;
		else
			abnormal_state =
				lenovo_pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_ABNORMAL_STATE];

		kobject_uevent_env(&bcdev->dev->kobj, KOBJ_CHANGE,
				   envp[abnormal_state]);
		bcdev->battery_abnormal = false;
	}
	rc = read_property_id(bcdev, pst, BATT_STATUS);

	if (rc < 0) {
		pr_err("Failed to read BATT_STATUS, rc=%d\n", rc);
		return;
	}

	if (pst->prop[BATT_STATUS] == POWER_SUPPLY_STATUS_CHARGING) {
		pr_debug("Battery is charging\n");
		return;
	}

	rc = read_property_id(bcdev, pst, BATT_CAPACITY);
	if (rc < 0) {
		pr_err("Failed to read BATT_CAPACITY, rc=%d\n", rc);
		return;
	}

	if (DIV_ROUND_CLOSEST(pst->prop[BATT_CAPACITY], 100) > 0) {
		pr_debug("Battery SOC is > 0\n");
		return;
	}

	/*
	 * If we are here, then battery is not charging and SOC is 0.
	 * Check the battery voltage and if it's lower than shutdown voltage,
	 * then initiate an emergency shutdown.
	 */

	rc = read_property_id(bcdev, pst, BATT_VOLT_NOW);
	if (rc < 0) {
		pr_err("Failed to read BATT_VOLT_NOW, rc=%d\n", rc);
		return;
	}

	if (pst->prop[BATT_VOLT_NOW] / 1000 > bcdev->shutdown_volt_mv) {
		pr_debug("Battery voltage is > %d mV\n",
			bcdev->shutdown_volt_mv);
		return;
	}

	pr_emerg("Initiating a shutdown in 100 ms\n");
	msleep(100);
	pr_emerg("Attempting kernel_power_off: Battery voltage low\n");
	kernel_power_off();
}

static void handle_notification(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
	struct battery_charger_notify_msg *notify_msg = data;
	struct psy_state *pst = NULL;
	u32 hboost_vmax_mv, notification;

	if (len != sizeof(*notify_msg)) {
		pr_err("Incorrect response length %zu\n", len);
		return;
	}

	notification = notify_msg->notification;
	pr_debug("notification: %#x\n", notification);
	if ((notification & 0xffff) == BC_HBOOST_VMAX_CLAMP_NOTIFY) {
		hboost_vmax_mv = (notification >> 16) & 0xffff;
		raw_notifier_call_chain(&hboost_notifier, VMAX_CLAMP, &hboost_vmax_mv);
		pr_debug("hBoost is clamped at %u mV\n", hboost_vmax_mv);
		return;
	}

	switch (notification) {
	case BC_BATTERY_STATUS_GET:
	case BC_GENERIC_NOTIFY:
		schedule_work(&bcdev->usb_type_work);
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
		if (bcdev->shutdown_volt_mv > 0)
			schedule_work(&bcdev->battery_check_work);
		break;
	case BC_USB_STATUS_GET(USB_1_PORT_ID):
		bcdev->usb_active[USB_1_PORT_ID] = true;
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		schedule_work(&bcdev->usb_type_work);
		break;
	case BC_USB_STATUS_GET(USB_2_PORT_ID):
		if (bcdev->num_usb_ports < 2) {
			pr_debug("notification: %u for missing USB_2 port\n",
					notification);
			break;
		}
		bcdev->usb_active[USB_2_PORT_ID] = true;
		pst = &bcdev->psy_list[PSY_TYPE_USB_2];
		schedule_work(&bcdev->usb_type_work);
		break;
	case BC_WLS_STATUS_GET:
		pst = &bcdev->psy_list[PSY_TYPE_WLS];
		schedule_work(&bcdev->usb_type_work);
		break;
	case BC_LENOVO_PROP_GET:
		pst = &bcdev->psy_list[PSY_TYPE_LENOVO];
		break;
	case BC_BATTERY_ABNORMAL_NOTIFY:
		bcdev->battery_abnormal = true;
		schedule_work(&bcdev->battery_check_work);
		return;
	default:
		break;
	}

	if (pst && pst->psy) {
		/*
		 * For charger mode, keep the device awake at least for 50 ms
		 * so that device won't enter suspend when a non-SDP charger
		 * is removed. This would allow the userspace process like
		 * "charger" to be able to read power supply uevents to take
		 * appropriate actions (e.g. shutting down when the charger is
		 * unplugged).
		 */
		power_supply_changed(pst->psy);
		pm_wakeup_dev_event(bcdev->dev, 50, true);
	}
}

static int battery_chg_callback(void *priv, void *data, size_t len)
{
	struct pmic_glink_hdr *hdr = data;
	struct battery_chg_dev *bcdev = priv;

	pr_debug("owner: %u type: %u opcode: %#x len: %zu\n", hdr->owner,
		hdr->type, hdr->opcode, len);

	down_read(&bcdev->state_sem);
	if (!bcdev->initialized) {
		pr_debug("Driver initialization failed: Dropping glink callback message: state %d\n",
			 atomic_read(&bcdev->state));
		up_read(&bcdev->state_sem);
		return 0;
	}
	up_read(&bcdev->state_sem);

	if (hdr->opcode == BC_NOTIFY_IND)
		handle_notification(bcdev, data, len);
	else
		handle_message(bcdev, data, len);

	return 0;
}

static int wls_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_WLS];
	int prop_id, rc;

	pval->intval = -ENODATA;

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;

	pval->intval = pst->prop[prop_id];

	return 0;
}

static int wls_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *pval)
{
	return 0;
}

static int wls_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	return 0;
}

static enum power_supply_property wls_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_TEMP,
};

static struct power_supply_desc wls_psy_desc = {
	.name			= "wireless",
	/* Stock wls_psy_desc.type is 4 (POWER_SUPPLY_TYPE_USB). */
	.type			= POWER_SUPPLY_TYPE_USB,
	.properties		= wls_props,
	.num_properties		= ARRAY_SIZE(wls_props),
	.get_property		= wls_psy_get_prop,
	.set_property		= wls_psy_set_prop,
	.property_is_writeable	= wls_psy_prop_is_writeable,
};

static const char *get_wls_type_name(u32 wls_type)
{
	if (wls_type >= ARRAY_SIZE(qc_power_supply_wls_type_text))
		return "Unknown";

	return qc_power_supply_wls_type_text[wls_type];
}

static const char *get_usb_type_name(u32 usb_type)
{
	if (usb_type >= QTI_POWER_SUPPLY_USB_TYPE_HVDCP &&
	    usb_type <= QTI_POWER_SUPPLY_USB_TYPE_HVDCP_3P5)
		return qc_power_supply_usb_type_text[
			usb_type - QTI_POWER_SUPPLY_USB_TYPE_HVDCP];

	if (usb_type < ARRAY_SIZE(power_supply_usb_type_text))
		return power_supply_usb_type_text[usb_type];

	return "Unknown";
}
static ssize_t real_type_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct psy_state *pst;
	int rc;

	if (IS_ERR(gbcdev) || !gbcdev)
		return -EINVAL;

	pst = &gbcdev->psy_list[PSY_TYPE_USB];
	rc = read_property_id(gbcdev, pst, USB_REAL_TYPE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			get_usb_type_name(pst->prop[USB_REAL_TYPE]));
}
static struct device_attribute real_type_attr =
	__ATTR(real_type, 0644, real_type_show, NULL);

static ssize_t input_suspend_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct psy_state *pst;
	int rc;

	if (IS_ERR(gbcdev) || !gbcdev)
		return -EINVAL;

	pst = &gbcdev->psy_list[PSY_TYPE_LENOVO];
	rc = read_property_id(gbcdev, pst, HQ_POWER_SUPPLY_PROP_INPUT_SUSPEND);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			pst->prop[HQ_POWER_SUPPLY_PROP_INPUT_SUSPEND]);
}

static ssize_t input_suspend_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct psy_state *pst;
	bool suspend = false;
	int rc;

	if (IS_ERR(gbcdev) || !gbcdev)
		return -EINVAL;

	rc = kstrtobool(buf, &suspend);
	if (rc)
		return -EINVAL;

	pst = &gbcdev->psy_list[PSY_TYPE_LENOVO];
	rc = write_property_id(gbcdev, pst, HQ_POWER_SUPPLY_PROP_INPUT_SUSPEND, suspend);
	if (rc < 0)
		return rc;

	pr_info("Lenovo Charger input_suspend:%d\n", suspend);
	return count;
}

static struct device_attribute input_suspend_attr =
	__ATTR(input_suspend, 0644, input_suspend_show, input_suspend_store);

static ssize_t det_gpio_port1_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct psy_state *pst;
	int rc;

	if (IS_ERR(gbcdev) || !gbcdev)
		return -EINVAL;

	pst = &gbcdev->psy_list[PSY_TYPE_LENOVO];
	rc = read_property_id(gbcdev, pst, HQ_POWER_SUPPLY_PROP_USB1_ONLINE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 pst->prop[HQ_POWER_SUPPLY_PROP_USB1_ONLINE]);
}

static struct device_attribute det_gpio_port1_attr =
	__ATTR(det_gpio_port1, 0444, det_gpio_port1_show, NULL);

static ssize_t det_gpio_port2_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct psy_state *pst;
	int rc;

	if (IS_ERR(gbcdev) || !gbcdev)
		return -EINVAL;

	pst = &gbcdev->psy_list[PSY_TYPE_LENOVO];
	rc = read_property_id(gbcdev, pst, HQ_POWER_SUPPLY_PROP_USB2_ONLINE);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 pst->prop[HQ_POWER_SUPPLY_PROP_USB2_ONLINE]);
}

static struct device_attribute det_gpio_port2_attr =
	__ATTR(det_gpio_port2, 0444, det_gpio_port2_show, NULL);
static struct attribute *usb_psy_attrs[] = {
	&input_suspend_attr.attr,
	&real_type_attr.attr,
	&det_gpio_port1_attr.attr,
	&det_gpio_port2_attr.attr,
	NULL,
};

static const struct attribute_group usb_psy_attrs_group = {
	.attrs = usb_psy_attrs,
};

struct battery_sysfs_field {
	struct device_attribute attr;
	u32 index;
};

static int lenovo_battery_sysfs_get_property(struct power_supply *psy,
					     u32 index)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_LENOVO];
	int val = 0;

	switch (index) {
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLE:
		if (read_property_id(bcdev, pst,
				     HQ_POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLE) >= 0)
			val = pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLE];
		break;
	case POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING:
		if (read_property_id(bcdev, pst,
				     HQ_POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING) >= 0)
			val = pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING];
		break;
	case POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING_EU:
		if (read_property_id(bcdev, pst,
				     HQ_POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING_EU) >= 0)
			val = pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING_EU];
		break;
	case POWER_SUPPLY_PROP_BATTERY_RECHARGE_SETTING:
		if (read_property_id(bcdev, pst,
				     HQ_POWER_SUPPLY_PROP_BATTERY_RECHARGE_SETTING) >= 0)
			val = pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_RECHARGE_SETTING];
		break;
	case POWER_SUPPLY_PROP_BATTERY_MAINTENANCE:
		if (read_property_id(bcdev, pst,
				     HQ_POWER_SUPPLY_PROP_BATTERY_MAINTENANCE) >= 0)
			val = pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_MAINTENANCE];
		break;
	case POWER_SUPPLY_PROP_BATTERY_MAINTENANCE20:
		if (read_property_id(bcdev, pst,
				     HQ_POWER_SUPPLY_PROP_BATTERY_MAINTENANCE20) >= 0)
			val = pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_MAINTENANCE20];
		break;
	case POWER_SUPPLY_PROP_BATTERY_SHIPPING_MODE:
		val = bcdev->ship_mode_en;
		break;
	case POWER_SUPPLY_PROP_BATTERY_PRODUCE_DATE:
		if (read_property_id(bcdev, pst,
				     HQ_POWER_SUPPLY_PROP_BATTERY_PRODUCE_DATE) >= 0)
			val = pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_PRODUCE_DATE];
		break;
	case POWER_SUPPLY_PROP_BATTERY_ACTIVATE_DATE:
		if (read_property_id(bcdev, pst,
				     HQ_POWER_SUPPLY_PROP_BATTERY_ACTIVATE_DATE) >= 0)
			val = pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_ACTIVATE_DATE];
		break;
	case POWER_SUPPLY_PROP_BATTERY_SOH:
		if (read_property_id(bcdev, pst,
				     HQ_POWER_SUPPLY_PROP_BATTERY_SOH) >= 0)
			val = pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_SOH];
		break;
	case POWER_SUPPLY_PROP_BATTERY_CV:
		if (read_property_id(bcdev, pst,
				     HQ_POWER_SUPPLY_PROP_BATTERY_CV) >= 0)
			val = pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_CV];
		break;
	case POWER_SUPPLY_PROP_BATTERY_CHG_FV:
		if (read_property_id(bcdev, pst,
				     HQ_POWER_SUPPLY_PROP_BATTERY_CHG_FV) >= 0)
			val = pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_CHG_FV];
		break;
	case POWER_SUPPLY_PROP_BATTERY_FAKE_CYCLE_COUNT:
		if (read_property_id(bcdev, pst,
				     HQ_POWER_SUPPLY_PROP_BATTERY_FAKE_CYCLE_COUNT) >= 0)
			val = pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_FAKE_CYCLE_COUNT];
		break;
	case POWER_SUPPLY_PROP_BATTERY_FAKE_SOC:
		if (read_property_id(bcdev, pst,
				     HQ_POWER_SUPPLY_PROP_BATTERY_FAKE_SOC) >= 0)
			val = pst->prop[HQ_POWER_SUPPLY_PROP_BATTERY_FAKE_SOC];
		break;
	case POWER_SUPPLY_PROP_BATTERY_ABNORMAL_STATE:
		val = bcdev->fake_abnormal_level;
		break;
	default:
		dev_err(bcdev->dev, ">>>Lenovo battery get error property<<<\n");
	}

	return val;
}

static ssize_t lenovo_battery_sysfs_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct battery_sysfs_field *field =
		container_of(attr, struct battery_sysfs_field, attr);
	struct power_supply *psy = dev_get_drvdata(dev);
	int val;

	if (!psy) {
		dev_info(&psy->dev, "invalid battery psy");
		return 0;
	}

	val = lenovo_battery_sysfs_get_property(psy, field->index);

	return scnprintf(buf, PAGE_SIZE,
			 (field->index == POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING ||
			  field->index == POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING_EU) ?
			 "%06d\n" : "%d\n", val);
}

static int lenovo_battery_sysfs_set_property(struct power_supply *psy,
					     int val, u32 index)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_LENOVO];
	int rc = 0;

	pr_info("Lenovo battery set property = %d, value = %d\n", index, val);
	switch (index) {
	case POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLE:
		rc = write_property_id(bcdev, pst,
				       HQ_POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLE, val);
		if (rc < 0)
			pr_err("Failed to set charging_enable, rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING:
		rc = write_property_id(bcdev, pst,
				       HQ_POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING, val);
		if (rc < 0)
			pr_err("Failed to set protection setting, rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING_EU:
		rc = write_property_id(bcdev, pst,
				       HQ_POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING_EU, val);
		if (rc < 0)
			pr_err("Failed to set protection setting eu, rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_BATTERY_RECHARGE_SETTING:
		rc = write_property_id(bcdev, pst,
				       HQ_POWER_SUPPLY_PROP_BATTERY_RECHARGE_SETTING, val);
		if (rc < 0)
			pr_err("Failed to set recharge setting rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_BATTERY_MAINTENANCE:
		rc = write_property_id(bcdev, pst,
				       HQ_POWER_SUPPLY_PROP_BATTERY_MAINTENANCE, val);
		if (rc < 0)
			pr_err("Failed to set battery maintenance rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_BATTERY_MAINTENANCE20:
		rc = write_property_id(bcdev, pst,
				       HQ_POWER_SUPPLY_PROP_BATTERY_MAINTENANCE20, val);
		if (rc < 0)
			pr_err("Failed to set battery maintenance20 rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_BATTERY_SHIPPING_MODE:
		bcdev->ship_mode_en = !!val;
		break;
	case POWER_SUPPLY_PROP_BATTERY_ACTIVATE_DATE:
		rc = write_property_id(bcdev, pst,
				       HQ_POWER_SUPPLY_PROP_BATTERY_ACTIVATE_DATE, val);
		if (rc < 0)
			pr_err("Failed to set charging enable, rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_BATTERY_CV:
		rc = write_property_id(bcdev, pst,
				       HQ_POWER_SUPPLY_PROP_BATTERY_CV, val);
		if (rc < 0)
			pr_err("Failed to set battery cv, rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_BATTERY_CHG_FV:
		rc = write_property_id(bcdev, pst,
				       HQ_POWER_SUPPLY_PROP_BATTERY_CHG_FV, val);
		if (rc < 0)
			pr_err("Failed to set battery chg FV, rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_BATTERY_FAKE_CYCLE_COUNT:
		rc = write_property_id(bcdev, pst,
				       HQ_POWER_SUPPLY_PROP_BATTERY_FAKE_CYCLE_COUNT, val);
		if (rc < 0)
			pr_err("Failed to set fake cycle count, rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_BATTERY_FAKE_SOC:
		rc = write_property_id(bcdev, pst,
				       HQ_POWER_SUPPLY_PROP_BATTERY_FAKE_SOC, val);
		if (rc < 0)
			pr_err("Failed to set fake soc, rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_BATTERY_ABNORMAL_STATE:
		bcdev->fake_abnormal_level = val;
		bcdev->battery_abnormal = true;
		queue_work_on(32, system_wq, &bcdev->battery_check_work);
		break;
	default:
		dev_err(bcdev->dev, ">>>Lenovo battery set error property<<<\n");
	}

	return rc;
}

static ssize_t lenovo_battery_sysfs_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_sysfs_field *field =
		container_of(attr, struct battery_sysfs_field, attr);
	struct power_supply *psy;
	ssize_t rc;
	int val = 0;

	rc = kstrtoint(buf, 10, &val);
	if (rc < 0)
		return rc;

	psy = dev_get_drvdata(dev);
	if (!psy) {
		dev_info(&psy->dev, "invalid battery psy");
		return rc;
	}

	lenovo_battery_sysfs_set_property(psy, val, field->index);

	return count;
}

#define BATTERY_SYSFS_FIELD(_name, _index)				\
	{ .attr = __ATTR(_name, 0664, lenovo_battery_sysfs_show,	\
			  lenovo_battery_sysfs_store), .index = (_index) }
#define BATTERY_SYSFS_FIELD_RO(_name, _index)				\
	{ .attr = __ATTR(_name, 0664, lenovo_battery_sysfs_show, NULL),	\
	  .index = (_index) }

static struct battery_sysfs_field battery_sysfs_field_tbl[] = {
	BATTERY_SYSFS_FIELD(charging_enable, POWER_SUPPLY_PROP_BATTERY_CHARGING_ENABLE),
	BATTERY_SYSFS_FIELD(protection_setting, POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING),
	BATTERY_SYSFS_FIELD(protection_setting_eu, POWER_SUPPLY_PROP_BATTERY_PROTECTION_SETTING_EU),
	BATTERY_SYSFS_FIELD(recharge_setting, POWER_SUPPLY_PROP_BATTERY_RECHARGE_SETTING),
	BATTERY_SYSFS_FIELD(battery_maintenance, POWER_SUPPLY_PROP_BATTERY_MAINTENANCE),
	BATTERY_SYSFS_FIELD(battery_maintenance20, POWER_SUPPLY_PROP_BATTERY_MAINTENANCE20),
	BATTERY_SYSFS_FIELD(shipping_mode, POWER_SUPPLY_PROP_BATTERY_SHIPPING_MODE),
	BATTERY_SYSFS_FIELD_RO(produce_date, POWER_SUPPLY_PROP_BATTERY_PRODUCE_DATE),
	BATTERY_SYSFS_FIELD(activate_date, POWER_SUPPLY_PROP_BATTERY_ACTIVATE_DATE),
	BATTERY_SYSFS_FIELD_RO(soh, POWER_SUPPLY_PROP_BATTERY_SOH),
	BATTERY_SYSFS_FIELD(cv, POWER_SUPPLY_PROP_BATTERY_CV),
	BATTERY_SYSFS_FIELD(chg_fv, POWER_SUPPLY_PROP_BATTERY_CHG_FV),
	BATTERY_SYSFS_FIELD(fake_cycle_count, POWER_SUPPLY_PROP_BATTERY_FAKE_CYCLE_COUNT),
	BATTERY_SYSFS_FIELD(fake_soc, POWER_SUPPLY_PROP_BATTERY_FAKE_SOC),
	BATTERY_SYSFS_FIELD(fake_abnormal_level, POWER_SUPPLY_PROP_BATTERY_ABNORMAL_STATE),
};

static struct attribute *battery_sysfs_attrs[
	ARRAY_SIZE(battery_sysfs_field_tbl) + 1];

static const struct attribute_group battery_sysfs_attr_group = {
	.attrs = battery_sysfs_attrs,
};

int hq_battery_sysfs_create_group(struct battery_chg_dev *bcdev)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int i, rc;

	if (!pst->psy) {
		dev_info(bcdev->dev, "Invalid battery psy\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(battery_sysfs_field_tbl); i++)
		battery_sysfs_attrs[i] =
			&battery_sysfs_field_tbl[i].attr.attr;
	battery_sysfs_attrs[ARRAY_SIZE(battery_sysfs_field_tbl)] = NULL;

	rc = sysfs_create_group(&pst->psy->dev.kobj,
				&battery_sysfs_attr_group);
	if (rc)
		dev_info(bcdev->dev, "Failed to create battery_sysfs\n");

	return rc;
}
EXPORT_SYMBOL(hq_battery_sysfs_create_group);

int hq_usb_sysfs_create_group(struct battery_chg_dev *bcdev)
{
	return sysfs_create_group(
		&bcdev->psy_list[PSY_TYPE_USB].psy->dev.kobj,
		&usb_psy_attrs_group);
}
EXPORT_SYMBOL(hq_usb_sysfs_create_group);

static int usb_psy_set_icl(struct battery_chg_dev *bcdev,
		struct psy_state *pst, u32 prop_id, int val)
{
	u32 temp;
	int rc;

	if (!pst->psy)
		return -ENODEV;

	rc = read_property_id(bcdev, pst, USB_ADAP_TYPE);
	if (rc < 0) {
		pr_err("Failed to read prop USB_ADAP_TYPE, rc=%d\n", rc);
		return rc;
	}

	/* Allow this only for SDP, CDP or USB_PD and not for other charger types */
	switch (pst->prop[USB_ADAP_TYPE]) {
	case POWER_SUPPLY_USB_TYPE_SDP:
	case POWER_SUPPLY_USB_TYPE_PD:
	case POWER_SUPPLY_USB_TYPE_CDP:
		break;
	default:
		if (!strcmp(pst->psy->desc->name, "usb-2"))
			bcdev->usb_icl_ua[USB_2_PORT_ID] = 0;
		else
			bcdev->usb_icl_ua[USB_1_PORT_ID] = 0;
		return -EINVAL;
	}

	/*
	 * Input current limit (ICL) can be set by different clients. E.g. USB
	 * driver can request for a current of 500/900 mA depending on the
	 * port type. Also, clients like EUD driver can pass 0 or -22 to
	 * suspend or unsuspend the input for its use case.
	 */

	temp = val;
	if (val < 0)
		temp = UINT_MAX;

	rc = write_property_id(bcdev, pst, prop_id, temp);
	if (rc < 0) {
		pr_err("Failed to set ICL (%u uA) rc=%d\n", temp, rc);
	} else {
		pr_debug("Set ICL to %u\n", temp);

		if (!strcmp(pst->psy->desc->name, "usb-2"))
			bcdev->usb_icl_ua[USB_2_PORT_ID] = temp;
		else
			bcdev->usb_icl_ua[USB_1_PORT_ID] = temp;
	}

	return rc;
}

static int usb_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst;
	int prop_id, rc;

	if (!strcmp(psy->desc->name, "usb-2"))
		pst = &bcdev->psy_list[PSY_TYPE_USB_2];
	else
		pst = &bcdev->psy_list[PSY_TYPE_USB];

	pval->intval = -ENODATA;
	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;

	pval->intval = pst->prop[prop_id];
	if (prop == POWER_SUPPLY_PROP_TEMP)
		pval->intval = DIV_ROUND_CLOSEST((int)pval->intval, 10);

	return 0;
}

static int usb_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst;
	int prop_id, rc = 0;

	if (!strcmp(psy->desc->name, "usb-2"))
		pst = &bcdev->psy_list[PSY_TYPE_USB_2];
	else
		pst = &bcdev->psy_list[PSY_TYPE_USB];

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("%s online:%d\n", __func__, pval->intval);
		rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_LENOVO],
				       0x37, pval->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (pst->prop[USB_REAL_TYPE] == POWER_SUPPLY_USB_TYPE_SDP &&
		    bcdev->psy_list[PSY_TYPE_BATTERY].prop[BATT_STATUS] ==
				POWER_SUPPLY_STATUS_FULL &&
		    pval->intval < 500000)
			rc = usb_psy_set_icl(bcdev, pst, prop_id, 500000);
		else
			rc = usb_psy_set_icl(bcdev, pst, prop_id, pval->intval);
		break;
	default:
		break;
	}

	return rc;
}

static int usb_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		return 1;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return 1;
	default:
		break;
	}

	return 0;
}

static enum power_supply_property usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_TEMP,
};

static enum power_supply_property uusb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_SCOPE,
};

static struct power_supply_desc usb_psy_desc[NUM_USB_PORTS] = {
	[USB_1_PORT_ID] = {
		.name			= "usb",
		.type			= POWER_SUPPLY_TYPE_USB,
		.properties		= usb_props,
		.num_properties		= ARRAY_SIZE(usb_props),
		.get_property		= usb_psy_get_prop,
		.set_property		= usb_psy_set_prop,
		.usb_types = BIT(POWER_SUPPLY_USB_TYPE_UNKNOWN) |
			     BIT(POWER_SUPPLY_USB_TYPE_SDP)     |
			     BIT(POWER_SUPPLY_USB_TYPE_DCP)     |
			     BIT(POWER_SUPPLY_USB_TYPE_CDP)     |
			     BIT(POWER_SUPPLY_USB_TYPE_ACA)     |
			     BIT(POWER_SUPPLY_USB_TYPE_C)       |
			     BIT(POWER_SUPPLY_USB_TYPE_PD)      |
			     BIT(POWER_SUPPLY_USB_TYPE_PD_DRP)  |
			     BIT(POWER_SUPPLY_USB_TYPE_PD_PPS)  |
			     BIT(POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID),
		.property_is_writeable	= usb_psy_prop_is_writeable,
	},

	[USB_2_PORT_ID] = {
		.name			= "usb-2",
		.type			= POWER_SUPPLY_TYPE_USB,
		.properties		= usb_props,
		.num_properties		= ARRAY_SIZE(usb_props),
		.get_property		= usb_psy_get_prop,
		.set_property		= usb_psy_set_prop,
		.usb_types = BIT(POWER_SUPPLY_USB_TYPE_UNKNOWN) |
			     BIT(POWER_SUPPLY_USB_TYPE_SDP)     |
			     BIT(POWER_SUPPLY_USB_TYPE_DCP)     |
			     BIT(POWER_SUPPLY_USB_TYPE_CDP)     |
			     BIT(POWER_SUPPLY_USB_TYPE_ACA)     |
			     BIT(POWER_SUPPLY_USB_TYPE_C)       |
			     BIT(POWER_SUPPLY_USB_TYPE_PD)      |
			     BIT(POWER_SUPPLY_USB_TYPE_PD_DRP)  |
			     BIT(POWER_SUPPLY_USB_TYPE_PD_PPS)  |
			     BIT(POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID),
		.property_is_writeable	= usb_psy_prop_is_writeable,
	}
};

#define CHARGE_CTRL_START_THR_MIN	50
#define CHARGE_CTRL_START_THR_MAX	95
#define CHARGE_CTRL_END_THR_MIN		55
#define CHARGE_CTRL_END_THR_MAX		100
#define CHARGE_CTRL_DELTA_SOC		5

static int battery_psy_set_charge_threshold(struct battery_chg_dev *bcdev,
					u32 target_soc, u32 delta_soc)
{
	struct battery_charger_chg_ctrl_msg msg = { { 0 } };
	int rc;

	if (!bcdev->chg_ctrl_en)
		return 0;

	if (target_soc > CHARGE_CTRL_END_THR_MAX)
		target_soc = CHARGE_CTRL_END_THR_MAX;

	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_CHG_CTRL_LIMIT_EN;
	/* Stock sends the thresholds with enable = 0. */
	msg.enable = 0;
	msg.target_soc = target_soc;
	msg.delta_soc = delta_soc;

	rc = battery_chg_write(bcdev, &msg, sizeof(msg), BC_WAIT_TIME_MS);
	if (rc < 0)
		pr_err("Failed to set charge_control thresholds, rc=%d\n", rc);
	else
		pr_debug("target_soc: %u delta_soc: %u\n", target_soc, delta_soc);

	return rc;
}

static int battery_psy_set_charge_end_threshold(struct battery_chg_dev *bcdev,
					int val)
{
	u32 delta_soc = CHARGE_CTRL_DELTA_SOC;
	int rc;

	if (val < CHARGE_CTRL_END_THR_MIN ||
	    val > CHARGE_CTRL_END_THR_MAX) {
		pr_err("Charge control end_threshold should be within [%u %u]\n",
			CHARGE_CTRL_END_THR_MIN, CHARGE_CTRL_END_THR_MAX);
		return -EINVAL;
	}

	if (bcdev->chg_ctrl_start_thr && val > bcdev->chg_ctrl_start_thr)
		delta_soc = val - bcdev->chg_ctrl_start_thr;

	rc = battery_psy_set_charge_threshold(bcdev, val, delta_soc);
	if (rc < 0)
		pr_err("Failed to set charge control end threshold %u, rc=%d\n",
			val, rc);
	else
		bcdev->chg_ctrl_end_thr = val;

	return rc;
}

static int battery_psy_set_charge_start_threshold(struct battery_chg_dev *bcdev,
					int val)
{
	u32 target_soc, delta_soc;
	int rc;

	if (val < CHARGE_CTRL_START_THR_MIN ||
	    val > CHARGE_CTRL_START_THR_MAX) {
		pr_err("Charge control start_threshold should be within [%u %u]\n",
			CHARGE_CTRL_START_THR_MIN, CHARGE_CTRL_START_THR_MAX);
		return -EINVAL;
	}

	if (val > bcdev->chg_ctrl_end_thr) {
		target_soc = val +  CHARGE_CTRL_DELTA_SOC;
		delta_soc = CHARGE_CTRL_DELTA_SOC;
	} else {
		target_soc = bcdev->chg_ctrl_end_thr;
		delta_soc = bcdev->chg_ctrl_end_thr - val;
	}

	rc = battery_psy_set_charge_threshold(bcdev, target_soc, delta_soc);
	if (rc < 0)
		pr_err("Failed to set charge control start threshold %u, rc=%d\n",
			val, rc);
	else
		bcdev->chg_ctrl_start_thr = val;

	return rc;
}

static int get_charge_control_en(struct battery_chg_dev *bcdev)
{
	int rc;

	rc = read_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_BATTERY],
				BATT_CHG_CTRL_EN);
	if (rc < 0)
		pr_err("Failed to read the CHG_CTRL_EN, rc = %d\n", rc);
	else
		bcdev->chg_ctrl_en =
			bcdev->psy_list[PSY_TYPE_BATTERY].prop[BATT_CHG_CTRL_EN];

	return rc;
}

static int __battery_psy_set_charge_current(struct battery_chg_dev *bcdev,
					u32 fcc_ua)
{
	int rc;

	if (bcdev->restrict_chg_en) {
		fcc_ua = min_t(u32, fcc_ua, bcdev->restrict_fcc_ua);
		fcc_ua = min_t(u32, fcc_ua, bcdev->thermal_fcc_ua);
	}

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_BATTERY],
				BATT_CHG_CTRL_LIM, fcc_ua);
	if (rc < 0) {
		pr_err("Failed to set FCC %u, rc=%d\n", fcc_ua, rc);
	} else {
		pr_debug("Set FCC to %u uA\n", fcc_ua);
		bcdev->last_fcc_ua = fcc_ua;
	}

	return rc;
}

static int battery_psy_set_charge_current(struct battery_chg_dev *bcdev,
					int val)
{
	int rc;
	u32 fcc_ua, prev_fcc_ua;

	if (!bcdev->num_thermal_levels)
		return 0;

	if (bcdev->num_thermal_levels < 0) {
		pr_err("Incorrect num_thermal_levels\n");
		return -EINVAL;
	}

	if (val < 0 || val > bcdev->num_thermal_levels)
		return -EINVAL;

	if (bcdev->thermal_fcc_step)
		fcc_ua = bcdev->psy_list[PSY_TYPE_BATTERY].prop[BATT_CHG_CTRL_LIM_MAX]
				- (bcdev->thermal_fcc_step * val);
	else
		fcc_ua = bcdev->thermal_levels[val];

	prev_fcc_ua = bcdev->thermal_fcc_ua;
	bcdev->thermal_fcc_ua = fcc_ua;

	rc = __battery_psy_set_charge_current(bcdev, fcc_ua);
	if (!rc)
		bcdev->curr_thermal_level = val;
	else
		bcdev->thermal_fcc_ua = prev_fcc_ua;

	pr_info("Lenovo Thermal limit fcc_ua:%d\n", fcc_ua);
	return rc;
}

static int battery_psy_get_prop(struct power_supply *psy,
		enum power_supply_property prop,
		union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int prop_id, rc;

	if (prop == POWER_SUPPLY_PROP_MANUFACTURER) {
		if (!pst->manufacturer[0]) {
			rc = read_battery_information(bcdev);
			if (rc < 0)
				return rc;
		}
		pval->strval = pst->manufacturer ? pst->manufacturer : "";
		return 0;
	}

	pval->intval = -ENODATA;

	/*
	 * The prop id of TIME_TO_FULL_NOW and TIME_TO_FULL_AVG is same.
	 * So, map the prop id of TIME_TO_FULL_AVG for TIME_TO_FULL_NOW.
	 * TIME_TO_EMPTY_NOW likewise reads the TIME_TO_EMPTY_AVG property.
	 */
	if (prop == POWER_SUPPLY_PROP_TIME_TO_FULL_NOW)
		prop = POWER_SUPPLY_PROP_TIME_TO_FULL_AVG;
	if (prop == POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW)
		prop = POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG;

	prop_id = get_property_id(pst, prop);
	if (prop_id < 0)
		return prop_id;

	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;

	switch (prop) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
		pval->strval = pst->model;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		pval->intval = DIV_ROUND_CLOSEST(pst->prop[prop_id], 100);
		if (IS_ENABLED(CONFIG_QTI_PMIC_GLINK_CLIENT_DEBUG) &&
		   (bcdev->fake_soc >= 0 && bcdev->fake_soc <= 100))
			pval->intval = bcdev->fake_soc;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		pval->intval = DIV_ROUND_CLOSEST((int)pst->prop[prop_id], 10);
		break;
	default:
		pval->intval = pst->prop[prop_id];
		break;
	}

	return rc;
}

static int battery_psy_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *pval)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD:
		return battery_psy_set_charge_start_threshold(bcdev,
								pval->intval);
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD:
		return battery_psy_set_charge_end_threshold(bcdev,
								pval->intval);
	default:
		return -EINVAL;
	}

	return 0;
}

static int battery_psy_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD:
		return 1;
	default:
		break;
	}

	return 0;
}

static const enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
};

static const struct power_supply_desc batt_psy_desc = {
	.name			= "battery",
	.type			= POWER_SUPPLY_TYPE_BATTERY,
	.properties		= battery_props,
	.num_properties		= ARRAY_SIZE(battery_props),
	.get_property		= battery_psy_get_prop,
	.set_property		= battery_psy_set_prop,
	.property_is_writeable	= battery_psy_prop_is_writeable,
};

static const enum power_supply_property x1e80100_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_START_THRESHOLD,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_END_THRESHOLD,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_POWER_AVG,
};

static const struct power_supply_desc x1e80100_batt_psy_desc = {
	.name			= "battery",
	.type			= POWER_SUPPLY_TYPE_BATTERY,
	.properties		= x1e80100_battery_props,
	.num_properties		= ARRAY_SIZE(x1e80100_battery_props),
	.get_property		= battery_psy_get_prop,
	.set_property		= battery_psy_set_prop,
	.property_is_writeable	= battery_psy_prop_is_writeable,
};

static int battery_chg_init_psy(struct battery_chg_dev *bcdev)
{
	struct power_supply_config psy_cfg = {};
	struct psy_state *pst;
	int rc;

	psy_cfg.drv_data = bcdev;
	psy_cfg.of_node = bcdev->dev->of_node;
	if (bcdev->micro_usb) {
		usb_psy_desc[USB_1_PORT_ID].properties = uusb_props;
		usb_psy_desc[USB_1_PORT_ID].num_properties = ARRAY_SIZE(uusb_props);
	}

	bcdev->psy_list[PSY_TYPE_USB].psy =
		devm_power_supply_register(bcdev->dev, &usb_psy_desc[USB_1_PORT_ID], &psy_cfg);
	if (IS_ERR(bcdev->psy_list[PSY_TYPE_USB].psy)) {
		rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_USB].psy);
		bcdev->psy_list[PSY_TYPE_USB].psy = NULL;
		pr_err("Failed to register USB power supply, rc=%d\n", rc);
		return rc;
	}

	if (bcdev->num_usb_ports > 1) {
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		rc = read_property_id(bcdev, pst, USB_NUM_PORTS);
		if (rc < 0) {
			pr_debug("Failed to read prop USB_NUM_PORTS, rc=%d\n", rc);
		} else if (pst->prop[USB_NUM_PORTS] > NUM_USB_PORTS) {
			pr_err("Number of USB ports detected as supported:%d, greater than 2\n",
				pst->prop[USB_NUM_PORTS]);
		} else if (pst->prop[USB_NUM_PORTS] != bcdev->num_usb_ports) {
			pr_err("Number of USB ports detected as supported:%d, configured in apps:%d\n",
				pst->prop[USB_NUM_PORTS], bcdev->num_usb_ports);
		}
	}

	if (bcdev->num_usb_ports == 2) {
		bcdev->usb_active[USB_2_PORT_ID] = true;
		bcdev->psy_list[PSY_TYPE_USB_2].psy =
			devm_power_supply_register(bcdev->dev,
				&usb_psy_desc[USB_2_PORT_ID], &psy_cfg);
		if (IS_ERR(bcdev->psy_list[PSY_TYPE_USB_2].psy)) {
			rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_USB_2].psy);
			bcdev->psy_list[PSY_TYPE_USB_2].psy = NULL;
			pr_err("Failed to register USB_2 power supply, rc=%d\n", rc);
			return rc;
		}
	}

	if (bcdev->wls_not_supported) {
		pr_debug("Wireless charging is not supported\n");
	} else {
		bcdev->psy_list[PSY_TYPE_WLS].psy =
			devm_power_supply_register(bcdev->dev, &wls_psy_desc, &psy_cfg);

		if (IS_ERR(bcdev->psy_list[PSY_TYPE_WLS].psy)) {
			rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_WLS].psy);
			bcdev->psy_list[PSY_TYPE_WLS].psy = NULL;
			pr_err("Failed to register wireless power supply, rc=%d\n", rc);
			return rc;
		}
	}

	bcdev->psy_list[PSY_TYPE_BATTERY].psy =
		devm_power_supply_register(bcdev->dev, bcdev->config->batt_psy_desc,
						&psy_cfg);
	if (IS_ERR(bcdev->psy_list[PSY_TYPE_BATTERY].psy)) {
		rc = PTR_ERR(bcdev->psy_list[PSY_TYPE_BATTERY].psy);
		bcdev->psy_list[PSY_TYPE_BATTERY].psy = NULL;
		pr_err("Failed to register battery power supply, rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static void devices_get_informs_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(to_delayed_work(work),
			struct battery_chg_dev, device_info_work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_LENOVO];
	int rc;

#define GET_AND_REGISTER_INFO(_prop, _id, _name) do {		\
		rc = read_property_id(bcdev, pst, (_prop));		\
		if (rc < 0)					\
			return;					\
		if (pst->prop[(_prop)])				\
			hq_regiser_hw_info((_id), (_name));	\
	} while (0)

	GET_AND_REGISTER_INFO(HQ_POWER_SUPPLY_PROP_IC_MAIN_CHARGE, HWID_MASTER1_CHARGE, "PMIH0104");
	GET_AND_REGISTER_INFO(HQ_POWER_SUPPLY_PROP_IC_FG_1, HWID_FG1_CHARGE, "FMM8013c10_A");
	GET_AND_REGISTER_INFO(HQ_POWER_SUPPLY_PROP_IC_FG_2, HWID_FG2_CHARGE, "FMM8013c10_B");
	GET_AND_REGISTER_INFO(HQ_POWER_SUPPLY_PROP_IC_CP_1, HWID_MASTER_CP_CHARGE, "SC8541_A");
	GET_AND_REGISTER_INFO(HQ_POWER_SUPPLY_PROP_IC_CP_2, HWID_SLAVE_CP_CHARGE, "SC8541_B");
	GET_AND_REGISTER_INFO(HQ_POWER_SUPPLY_PROP_IC_PDPHY, HWID_PD_CHARGE, "AW35615");
	GET_AND_REGISTER_INFO(HQ_POWER_SUPPLY_PROP_IC_QC, HWID_QC_CHARGE, "WT6670F");
	GET_AND_REGISTER_INFO(HQ_POWER_SUPPLY_PROP_IC_SC760X_1, HWID_SC7603A_CHARGE, "SC7603A");
	GET_AND_REGISTER_INFO(HQ_POWER_SUPPLY_PROP_IC_SC760X_2, HWID_SC7603B_CHARGE, "SC7603B");
#undef GET_AND_REGISTER_INFO
}

static void lenovo_chg_monitor_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(to_delayed_work(work),
			struct battery_chg_dev, lenovo_chg_monitor_work);
	struct psy_state *batt = &bcdev->psy_list[PSY_TYPE_BATTERY];
	struct psy_state *usb = &bcdev->psy_list[PSY_TYPE_USB];
	struct psy_state *wls = &bcdev->psy_list[PSY_TYPE_WLS];
	struct gpio_desc *gpio_desc;
	int batt_temp_c, temp1 = 0, temp2 = 0, rc;
	int gpio1, gpio2;

	bcdev->lenovo_chg_initialized = true;
	batt_temp_c = DIV_ROUND_CLOSEST((int)batt->prop[BATT_TEMP], 10);

	if (IS_ERR(bcdev->usb1_therm) || !bcdev->usb1_therm) {
		bcdev->usb1_therm =
			thermal_zone_get_zone_by_name("usb1-conn-therm");
		if (IS_ERR(bcdev->usb1_therm) || !bcdev->usb1_therm) {
			pr_err("Failed to get usb1-conn-therm\n");
			goto reschedule;
		}
	}

	if (IS_ERR(bcdev->usb2_therm) || !bcdev->usb2_therm) {
		bcdev->usb2_therm =
			thermal_zone_get_zone_by_name("usb2-conn-therm");
		if (IS_ERR(bcdev->usb2_therm) || !bcdev->usb2_therm) {
			pr_err("Failed to get usb2-conn-therm\n");
			goto reschedule;
		}
	}

	rc = thermal_zone_get_temp(bcdev->usb1_therm, &temp1);
	if (rc) {
		pr_err("Failed to get usb1-conn-temp, ret:%d\n", rc);
		goto reschedule;
	}
	temp1 /= 100;

	rc = thermal_zone_get_temp(bcdev->usb2_therm, &temp2);
	if (rc) {
		pr_err("Failed to get usb2-conn-temp, ret:%d\n", rc);
		goto reschedule;
	}
	temp2 /= 100;

	if (temp1 < 450 ||
	    (temp1 <= 499 && temp1 - batt_temp_c <= 99)) {
		gpio_desc = gpio_to_desc(515);
		gpio1 = gpiod_get_raw_value(gpio_desc);
		if (gpio1) {
			gpio_desc = gpio_to_desc(515);
			gpiod_direction_output_raw(gpio_desc, 0);
		}
	} else if (temp1 > 750 ||
		   (temp1 >= 651 && temp1 - batt_temp_c >= 301)) {
		gpio_desc = gpio_to_desc(515);
		gpio1 = gpiod_get_raw_value(gpio_desc);
		if (!gpio1) {
			gpio_desc = gpio_to_desc(515);
			gpiod_direction_output_raw(gpio_desc, 1);
		}
	}

	if (temp2 < 450 ||
	    (temp2 <= 499 && temp2 - batt_temp_c <= 99)) {
		gpio_desc = gpio_to_desc(728);
		gpio2 = gpiod_get_raw_value(gpio_desc);
		if (gpio2) {
			gpio_desc = gpio_to_desc(728);
			gpiod_direction_output_raw(gpio_desc, 0);
		}
	} else if (temp2 > 700 ||
		   (temp2 >= 601 && temp2 - batt_temp_c >= 201)) {
		gpio_desc = gpio_to_desc(728);
		gpio2 = gpiod_get_raw_value(gpio_desc);
		if (!gpio2) {
			gpio_desc = gpio_to_desc(728);
			gpiod_direction_output_raw(gpio_desc, 1);
		}
	}

	pr_info("TYPE = [%s,%d,%d], VBUS = [%d,%d], USB_TEMP = [%d,%d], BATT_TEMP = %d, GPIO:[%d,%d]\n",
		get_usb_type_name(usb->prop[USB_REAL_TYPE]), usb_psy_desc[USB_1_PORT_ID].type,
		wls_psy_desc.type, usb->prop[USB_VOLT_NOW],
		wls->prop[WLS_VOLT_NOW], temp1, temp2, batt_temp_c,
		gpiod_get_raw_value(gpio_to_desc(515)),
		gpiod_get_raw_value(gpio_to_desc(728)));

reschedule:
	queue_delayed_work_on(32, system_wq, &bcdev->lenovo_chg_monitor_work,
			      1250);
}

static void battery_chg_subsys_up_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev, subsys_up_work);
	struct psy_state *pst;
	int rc;

	battery_chg_notify_enable(bcdev);

	/*
	 * Give some time after enabling notification so that USB adapter type
	 * information can be obtained properly which is essential for setting
	 * USB ICL.
	 */
	msleep(200);

	if (bcdev->last_fcc_ua) {
		rc = __battery_psy_set_charge_current(bcdev,
				bcdev->last_fcc_ua);
		if (rc < 0)
			pr_err("Failed to set FCC (%u uA), rc=%d\n",
				bcdev->last_fcc_ua, rc);
	}

	if (bcdev->usb_icl_ua[USB_1_PORT_ID]) {
		pst = &bcdev->psy_list[PSY_TYPE_USB];
		rc = usb_psy_set_icl(bcdev, pst, USB_INPUT_CURR_LIMIT,
				bcdev->usb_icl_ua[USB_1_PORT_ID]);
		if (rc < 0)
			pr_err("Failed to set ICL(%u uA), rc=%d\n",
				bcdev->usb_icl_ua[USB_1_PORT_ID], rc);
	}

	if (bcdev->num_usb_ports == 2 && bcdev->usb_icl_ua[USB_2_PORT_ID]) {
		pst = &bcdev->psy_list[PSY_TYPE_USB_2];
		rc = usb_psy_set_icl(bcdev, pst, USB_INPUT_CURR_LIMIT,
				bcdev->usb_icl_ua[USB_2_PORT_ID]);
		if (rc < 0)
			pr_err("Failed to set USB-2 ICL(%u uA), rc=%d\n",
				bcdev->usb_icl_ua[USB_2_PORT_ID], rc);
	}
}

static int wireless_fw_send_firmware(struct battery_chg_dev *bcdev,
					const struct firmware *fw)
{
	struct wireless_fw_push_buf_req msg = {};
	const u8 *ptr;
	u32 i, buf_size, num_chunks, partial_chunk_size;
	int rc;

	if (strstr(bcdev->wls_fw_name, "cps"))
		buf_size = CPS_WLS_FW_BUF_SIZE;
	else
		buf_size = WLS_FW_BUF_SIZE;

	num_chunks = fw->size / buf_size;
	partial_chunk_size = fw->size % buf_size;

	if (!num_chunks)
		return -EINVAL;

	pr_debug("Updating FW...\n");

	ptr = fw->data;
	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_WLS_FW_PUSH_BUF_REQ;

	for (i = 0; i < num_chunks; i++, ptr += buf_size) {
		msg.fw_chunk_id = i + 1;
		memcpy(msg.buf, ptr, buf_size);

		pr_debug("sending FW chunk %u\n", i + 1);
		rc = battery_chg_fw_write(bcdev, &msg, sizeof(msg));
		if (rc < 0)
			return rc;
	}

	if (partial_chunk_size) {
		msg.fw_chunk_id = i + 1;
		memset(msg.buf, 0, buf_size);
		memcpy(msg.buf, ptr, partial_chunk_size);

		pr_debug("sending partial FW chunk %u\n", i + 1);
		rc = battery_chg_fw_write(bcdev, &msg, sizeof(msg));
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int wireless_fw_check_for_update(struct battery_chg_dev *bcdev,
					u32 version, size_t size)
{
	struct wireless_fw_check_req req_msg = {};

	bcdev->wls_fw_update_reqd = false;

	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = BC_WLS_FW_CHECK_UPDATE;
	req_msg.fw_version = version;
	req_msg.fw_size = size;
	req_msg.fw_crc = bcdev->wls_fw_crc;

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg), BC_FW_VER_WAIT_TIME_MS);
}

#define IDT9415_FW_MAJOR_VER_OFFSET		0x84
#define IDT9415_FW_MINOR_VER_OFFSET		0x86
#define IDT_FW_MAJOR_VER_OFFSET		0x94
#define IDT_FW_MINOR_VER_OFFSET		0x96
#define CPS_FW_MAJOR_VER_OFFSET		0xC4
#define CPS_FW_MINOR_VER_OFFSET		0xC5

static u32 wireless_fw_version_get(struct battery_chg_dev *bcdev, const u8 *data, bool force)
{
	u16 maj_ver = 0, min_ver = 0;
	u32 version = 0;

	if (force)
		return UINT_MAX;

	if (strstr(bcdev->wls_fw_name, "idt9412")) {
		maj_ver = le16_to_cpu(*(__le16 *)(data + IDT_FW_MAJOR_VER_OFFSET));
		min_ver = le16_to_cpu(*(__le16 *)(data + IDT_FW_MINOR_VER_OFFSET));
		version = (maj_ver << 16 | min_ver);
	} else if (strstr(bcdev->wls_fw_name, "idt")) {
		maj_ver = le16_to_cpu(*(__le16 *)(data + IDT9415_FW_MAJOR_VER_OFFSET));
		min_ver = le16_to_cpu(*(__le16 *)(data + IDT9415_FW_MINOR_VER_OFFSET));
		version = (maj_ver << 16 | min_ver);
	} else if (strstr(bcdev->wls_fw_name, "cps"))
		version = be16_to_cpu(*(__be16 *)(data + CPS_FW_MAJOR_VER_OFFSET));

	return version;
}

static int wireless_fw_update(struct battery_chg_dev *bcdev, bool force)
{
	const struct firmware *fw;
	struct psy_state *pst;
	u32 version;
	int rc;

	if (!bcdev->wls_fw_name) {
		pr_err("wireless FW name is not specified\n");
		return -EINVAL;
	}

	pm_stay_awake(bcdev->dev);

	/*
	 * Check for USB presence. If nothing is connected, check whether
	 * battery SOC is at least 50% before allowing FW update.
	 */
	pst = &bcdev->psy_list[PSY_TYPE_USB];
	rc = read_property_id(bcdev, pst, USB_ONLINE);
	if (rc < 0)
		goto out;

	if (bcdev->num_usb_ports == 2 && !pst->prop[USB_ONLINE]) {
		pst = &bcdev->psy_list[PSY_TYPE_USB_2];
		rc = read_property_id(bcdev, pst, USB_ONLINE);
		if (rc < 0)
			goto out;
	}

	if (!pst->prop[USB_ONLINE]) {
		pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
		rc = read_property_id(bcdev, pst, BATT_CAPACITY);
		if (rc < 0)
			goto out;

		if ((pst->prop[BATT_CAPACITY] / 100) < 50) {
			pr_err("Battery SOC should be at least 50%% or connect charger\n");
			rc = -EINVAL;
			goto out;
		}
	}

	rc = firmware_request_nowarn(&fw, bcdev->wls_fw_name, bcdev->dev);
	if (rc) {
		pr_err("Couldn't get firmware rc=%d\n", rc);
		goto out;
	}

	if (!fw || !fw->data || !fw->size) {
		pr_err("Invalid firmware\n");
		rc = -EINVAL;
		goto release_fw;
	}

	if (fw->size < SZ_16K) {
		pr_err("Invalid firmware size %zu\n", fw->size);
		rc = -EINVAL;
		goto release_fw;
	}

	version = wireless_fw_version_get(bcdev, fw->data, force);

	pr_debug("FW size: %zu version: %#x\n", fw->size, version);

	rc = wireless_fw_check_for_update(bcdev, version, fw->size);
	if (rc < 0) {
		pr_err("Wireless FW update not needed, rc=%d\n", rc);
		goto release_fw;
	}

	if (!bcdev->wls_fw_update_reqd) {
		pr_warn("Wireless FW update not required\n");
		goto release_fw;
	}

	/* Wait for IDT to be setup by charger firmware */
	msleep(WLS_FW_PREPARE_TIME_MS);

	reinit_completion(&bcdev->fw_update_ack);
	rc = wireless_fw_send_firmware(bcdev, fw);
	if (rc < 0) {
		pr_err("Failed to send FW chunk, rc=%d\n", rc);
		goto release_fw;
	}

	pr_debug("Waiting for fw_update_ack\n");
	rc = wait_for_completion_timeout(&bcdev->fw_update_ack,
				msecs_to_jiffies(bcdev->wls_fw_update_time_ms));
	if (!rc) {
		pr_err("Error, timed out updating firmware\n");
		rc = -ETIMEDOUT;
		goto release_fw;
	} else {
		pr_debug("Waited for %d ms\n",
			bcdev->wls_fw_update_time_ms - jiffies_to_msecs(rc));
		rc = 0;
	}

	pr_info("Wireless FW update done\n");

release_fw:
	bcdev->wls_fw_crc = 0;
	release_firmware(fw);
out:
	pm_relax(bcdev->dev);

	return rc;
}

static ssize_t qti_charger_ro_show(const struct class *c,
				const struct class_attribute *attr, char *buf,
				int psy_type, int prop_id)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[psy_type];
	int rc;

	rc = read_property_id(bcdev, pst, prop_id);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", (int)pst->prop[prop_id]);
}

#define QTI_CHARGER_RO_SHOW(name, psy_type, prop_id)			\
	static ssize_t name##_show(const struct class *c,			\
					const struct class_attribute *attr,	\
					char *buf)			\
	{								\
		return qti_charger_ro_show(c, attr, buf, psy_type, prop_id); \
	}								\
	static CLASS_ATTR_RO(name)

#define QTI_CHARGER_RW_SHOW(name, psy_type, prop_id)			\
	static ssize_t name##_show(const struct class *c,			\
					const struct class_attribute *attr,	\
					char *buf)			\
	{								\
		return qti_charger_ro_show(c, attr, buf, psy_type, prop_id); \
	}								\
	static CLASS_ATTR_RW(name)

#define QTI_CHARGER_RW_PROP_SHOW(name, field)				\
	static ssize_t name##_show(const struct class *c,			\
					const struct class_attribute *attr,	\
					char *buf)			\
	{								\
		struct battery_chg_dev *bcdev = container_of(c,		\
				struct battery_chg_dev, battery_class);	\
		return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->field);	\
	}								\
	static CLASS_ATTR_RW(name)

#define QTI_CHARGER_TYPE_RO_SHOW(name, psy, psy_type, prop_id)		\
	static ssize_t name##_show(const struct class *c,			\
					const struct class_attribute *attr,	\
					char *buf)			\
	{								\
		struct battery_chg_dev *bcdev = container_of(c,		\
				struct battery_chg_dev, battery_class);	\
		struct psy_state *pst = &bcdev->psy_list[psy_type];	\
		int rc;							\
									\
		rc = read_property_id(bcdev, pst, prop_id);		\
		if (rc < 0)						\
			return rc;					\
									\
		return scnprintf(buf, PAGE_SIZE, "%s\n",		\
				get_##psy##_type_name(pst->prop[prop_id]));	\
	}								\
	static CLASS_ATTR_RO(name)

static ssize_t wireless_fw_update_time_ms_store(const struct class *c,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	if (kstrtou32(buf, 0, &bcdev->wls_fw_update_time_ms))
		return -EINVAL;

	return count;
}

QTI_CHARGER_RW_PROP_SHOW(wireless_fw_update_time_ms, wls_fw_update_time_ms);

static ssize_t wireless_fw_crc_store(const struct class *c,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	u16 val;

	if (kstrtou16(buf, 0, &val) || !val)
		return -EINVAL;

	bcdev->wls_fw_crc = val;

	return count;
}
static CLASS_ATTR_WO(wireless_fw_crc);

static ssize_t wireless_fw_version_show(const struct class *c,
					const struct class_attribute *attr,
					char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct wireless_fw_get_version_req req_msg = {};
	int rc;

	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = BC_WLS_FW_GET_VERSION;

	rc = battery_chg_write(bcdev, &req_msg, sizeof(req_msg), BC_FW_VER_WAIT_TIME_MS);
	if (rc < 0) {
		pr_err("Failed to get FW version rc=%d\n", rc);
		return rc;
	}

	return scnprintf(buf, PAGE_SIZE, "%#x\n", bcdev->wls_fw_version);
}
static CLASS_ATTR_RO(wireless_fw_version);

static ssize_t wireless_fw_force_update_store(const struct class *c,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	bool val;
	int rc;

	if (kstrtobool(buf, &val) || !val)
		return -EINVAL;

	rc = wireless_fw_update(bcdev, true);
	if (rc < 0)
		return rc;

	return count;
}
static CLASS_ATTR_WO(wireless_fw_force_update);

static ssize_t wireless_fw_update_store(const struct class *c,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	bool val;
	int rc;

	if (kstrtobool(buf, &val) || !val)
		return -EINVAL;

	rc = wireless_fw_update(bcdev, false);
	if (rc < 0)
		return rc;

	return count;
}
static CLASS_ATTR_WO(wireless_fw_update);

QTI_CHARGER_TYPE_RO_SHOW(wireless_type, wls, PSY_TYPE_WLS, WLS_ADAP_TYPE);

static ssize_t charge_control_en_store(const struct class *c,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	if (val == bcdev->chg_ctrl_en)
		return count;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_BATTERY],
				BATT_CHG_CTRL_EN, val);
	if (rc < 0) {
		pr_err("Failed to set charge_control_en, rc=%d\n", rc);
		return rc;
	}

	bcdev->chg_ctrl_en = val;

	return count;
}

static ssize_t charge_control_en_show(const struct class *c,
				const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;

	rc = get_charge_control_en(bcdev);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->chg_ctrl_en);
}
static CLASS_ATTR_RW(charge_control_en);

QTI_CHARGER_RO_SHOW(usb_typec_compliant, PSY_TYPE_USB, USB_TYPEC_COMPLIANT);

QTI_CHARGER_RO_SHOW(usb_num_ports, PSY_TYPE_USB, USB_NUM_PORTS);

QTI_CHARGER_TYPE_RO_SHOW(usb_real_type, usb, PSY_TYPE_USB, USB_REAL_TYPE);

QTI_CHARGER_RO_SHOW(usb_2_typec_compliant, PSY_TYPE_USB_2, USB_TYPEC_COMPLIANT);

QTI_CHARGER_TYPE_RO_SHOW(usb_2_real_type, usb, PSY_TYPE_USB_2, USB_REAL_TYPE);

static ssize_t restrict_cur_store(const struct class *c,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	u32 fcc_ua, prev_fcc_ua;

	if (kstrtou32(buf, 0, &fcc_ua) || fcc_ua > bcdev->thermal_fcc_ua)
		return -EINVAL;

	prev_fcc_ua = bcdev->restrict_fcc_ua;
	bcdev->restrict_fcc_ua = fcc_ua;
	if (bcdev->restrict_chg_en) {
		rc = __battery_psy_set_charge_current(bcdev, fcc_ua);
		if (rc < 0) {
			bcdev->restrict_fcc_ua = prev_fcc_ua;
			return rc;
		}
	}

	return count;
}
QTI_CHARGER_RW_PROP_SHOW(restrict_cur, restrict_fcc_ua);

static ssize_t restrict_chg_store(const struct class *c,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	bcdev->restrict_chg_en = val;
	rc = __battery_psy_set_charge_current(bcdev, bcdev->restrict_chg_en ?
			bcdev->restrict_fcc_ua : bcdev->thermal_fcc_ua);
	if (rc < 0)
		return rc;

	return count;
}
QTI_CHARGER_RW_PROP_SHOW(restrict_chg, restrict_chg_en);

static ssize_t fake_soc_store(const struct class *c,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	bcdev->fake_soc = val;
	pr_debug("Set fake soc to %d\n", val);

	if (IS_ENABLED(CONFIG_QTI_PMIC_GLINK_CLIENT_DEBUG) && pst->psy)
		power_supply_changed(pst->psy);

	return count;
}
QTI_CHARGER_RW_PROP_SHOW(fake_soc, fake_soc);

static ssize_t wireless_boost_en_store(const struct class *c,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[PSY_TYPE_WLS],
				WLS_BOOST_EN, val);
	if (rc < 0)
		return rc;

	return count;
}

QTI_CHARGER_RW_SHOW(wireless_boost_en, PSY_TYPE_WLS, WLS_BOOST_EN);

static ssize_t _moisture_detection_en_store(const struct class *c,
					const struct class_attribute *attr, enum psy_type type,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	rc = write_property_id(bcdev, &bcdev->psy_list[type],
				USB_MOISTURE_DET_EN, val);
	if (rc < 0)
		return rc;

	return count;
}

static ssize_t moisture_detection_en_store(const struct class *c,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	return _moisture_detection_en_store(c, attr, PSY_TYPE_USB, buf, count);
}

QTI_CHARGER_RW_SHOW(moisture_detection_en, PSY_TYPE_USB, USB_MOISTURE_DET_EN);

static ssize_t moisture_detection_usb_2_en_store(const struct class *c,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	return _moisture_detection_en_store(c, attr, PSY_TYPE_USB_2, buf, count);
}

QTI_CHARGER_RW_SHOW(moisture_detection_usb_2_en, PSY_TYPE_USB_2, USB_MOISTURE_DET_EN);

QTI_CHARGER_RO_SHOW(moisture_detection_status, PSY_TYPE_USB, USB_MOISTURE_DET_STS);

QTI_CHARGER_RO_SHOW(moisture_detection_usb_2_status, PSY_TYPE_USB_2, USB_MOISTURE_DET_STS);

QTI_CHARGER_RO_SHOW(resistance, PSY_TYPE_BATTERY, BATT_RESISTANCE);

QTI_CHARGER_RO_SHOW(soh, PSY_TYPE_BATTERY, BATT_SOH);

static int battery_chg_ship_mode(struct battery_chg_dev *bcdev)
{
	struct battery_charger_ship_mode_req_msg msg = { { 0 } };
	int rc;

	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_SHIP_MODE_REQ_SET;
	msg.ship_mode_type = SHIP_MODE_PMIC;

	rc = battery_chg_write(bcdev, &msg, sizeof(msg), BC_WAIT_TIME_MS);
	if (rc < 0)
		pr_emerg("Failed to write ship mode: %d\n", rc);

	return rc;
}

static ssize_t ship_mode_en_store(const struct class *c,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	int rc;

	if (kstrtobool(buf, &bcdev->ship_mode_en))
		return -EINVAL;

	if (bcdev->ship_mode_en && bcdev->ship_mode_immediate) {
		rc = battery_chg_ship_mode(bcdev);
		if (rc < 0)
			return rc;
	}

	return count;
}

static ssize_t ship_mode_en_show(const struct class *c,
				const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bcdev->ship_mode_en);
}
static CLASS_ATTR_RW(ship_mode_en);

#define BATT_PARALLEL_CELL_AVAIL_COUNT(val)		FIELD_GET(GENMASK(15, 8), val)
#define BATT_PARALLEL_CELL_TOTAL_COUNT(val)		FIELD_GET(GENMASK(7, 0), val)

static ssize_t battery_parallel_cell_count_show(const struct class *c,
			const struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev, battery_class);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int rc;

	rc = read_property_id(bcdev, pst, BATT_PARALLEL_CELL_COUNT);
	if (rc < 0)
		return rc;

	return scnprintf(buf, PAGE_SIZE, "Parallel battery cell count (available/total): %lu/%lu\n",
		BATT_PARALLEL_CELL_AVAIL_COUNT(pst->prop[BATT_PARALLEL_CELL_COUNT]),
		BATT_PARALLEL_CELL_TOTAL_COUNT(pst->prop[BATT_PARALLEL_CELL_COUNT]));
}
static CLASS_ATTR_RO(battery_parallel_cell_count);

#define LENOVO_CLASS_PROP_RO(_name, _id)					\
static ssize_t _name##_show(const struct class *c,				\
			    const struct class_attribute *attr, char *buf)	\
{										\
	struct battery_chg_dev *bcdev = container_of(c,				\
					struct battery_chg_dev, battery_class);	\
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_LENOVO];		\
	int rc = read_property_id(bcdev, pst, _id);				\
										\
	if (rc < 0)								\
		return rc;							\
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[_id]);		\
}										\
static CLASS_ATTR_RO(_name)

#define LENOVO_CLASS_PROP_RW(_name, _id, _base)				\
static ssize_t _name##_show(const struct class *c,				\
			    const struct class_attribute *attr, char *buf)	\
{										\
	struct battery_chg_dev *bcdev = container_of(c,				\
					struct battery_chg_dev, battery_class);	\
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_LENOVO];		\
	int rc = read_property_id(bcdev, pst, _id);				\
										\
	if (rc < 0)								\
		return rc;							\
	return scnprintf(buf, PAGE_SIZE, "%d\n", pst->prop[_id]);		\
}										\
static ssize_t _name##_store(const struct class *c,				\
			     const struct class_attribute *attr,		\
			     const char *buf, size_t count)			\
{										\
	struct battery_chg_dev *bcdev = container_of(c,				\
					struct battery_chg_dev, battery_class);	\
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_LENOVO];		\
	int val;								\
	int rc;									\
										\
	if (kstrtoint(buf, _base, &val))					\
		return -EINVAL;							\
	rc = write_property_id(bcdev, pst, _id, val);				\
	return rc < 0 ? rc : count;						\
}										\
static CLASS_ATTR_RW(_name)
LENOVO_CLASS_PROP_RO(typec_cc_orientation, HQ_POWER_SUPPLY_PROP_CC_ORIENTATION);
LENOVO_CLASS_PROP_RO(typec_cc_orientation2, HQ_POWER_SUPPLY_PROP_CC_ORIENTATION2);
LENOVO_CLASS_PROP_RO(chip_main_charge, HQ_POWER_SUPPLY_PROP_IC_MAIN_CHARGE);
LENOVO_CLASS_PROP_RO(chip_fg1, HQ_POWER_SUPPLY_PROP_IC_FG_1);
LENOVO_CLASS_PROP_RO(chip_fg2, HQ_POWER_SUPPLY_PROP_IC_FG_2);
LENOVO_CLASS_PROP_RO(chip_cp1, HQ_POWER_SUPPLY_PROP_IC_CP_1);
LENOVO_CLASS_PROP_RO(chip_cp2, HQ_POWER_SUPPLY_PROP_IC_CP_2);
LENOVO_CLASS_PROP_RO(chip_qi, HQ_POWER_SUPPLY_PROP_IC_QI);
LENOVO_CLASS_PROP_RO(chip_pdphy, HQ_POWER_SUPPLY_PROP_IC_PDPHY);
LENOVO_CLASS_PROP_RO(chip_qc, HQ_POWER_SUPPLY_PROP_IC_QC);
LENOVO_CLASS_PROP_RO(battery1_manufacturer, HQ_POWER_SUPPLY_PROP_BATTERY_MANUFACTURER_1);
LENOVO_CLASS_PROP_RO(battery2_manufacturer, HQ_POWER_SUPPLY_PROP_BATTERY_MANUFACTURER_2);
LENOVO_CLASS_PROP_RW(fg1_temp, HQ_POWER_SUPPLY_PROP_FG_1_TEMP, 0);
LENOVO_CLASS_PROP_RW(fg2_temp, HQ_POWER_SUPPLY_PROP_FG_2_TEMP, 0);
LENOVO_CLASS_PROP_RO(fg1_current, HQ_POWER_SUPPLY_PROP_FG_1_CURRENT);
LENOVO_CLASS_PROP_RO(fg2_current, HQ_POWER_SUPPLY_PROP_FG_2_CURRENT);
LENOVO_CLASS_PROP_RO(fg1_voltage, HQ_POWER_SUPPLY_PROP_FG_1_VOLTAGE);
LENOVO_CLASS_PROP_RO(fg2_voltage, HQ_POWER_SUPPLY_PROP_FG_2_VOLTAGE);
LENOVO_CLASS_PROP_RO(fg1_produce_date, HQ_POWER_SUPPLY_PROP_FG_1_PRODUCE_DATE);
LENOVO_CLASS_PROP_RO(fg2_produce_date, HQ_POWER_SUPPLY_PROP_FG_2_PRODUCE_DATE);
LENOVO_CLASS_PROP_RW(fg1_activate_date, HQ_POWER_SUPPLY_PROP_FG_1_ACTIVATE_DATE, 10);
LENOVO_CLASS_PROP_RW(fg2_activate_date, HQ_POWER_SUPPLY_PROP_FG_2_ACTIVATE_DATE, 10);
LENOVO_CLASS_PROP_RO(fg1_soh, HQ_POWER_SUPPLY_PROP_FG_1_SOH);
LENOVO_CLASS_PROP_RO(fg2_soh, HQ_POWER_SUPPLY_PROP_FG_2_SOH);
LENOVO_CLASS_PROP_RW(fg1_cycle_count, HQ_POWER_SUPPLY_PROP_FG_1_CYCLE_COUNT, 10);
LENOVO_CLASS_PROP_RW(fg2_cycle_count, HQ_POWER_SUPPLY_PROP_FG_2_CYCLE_COUNT, 10);
LENOVO_CLASS_PROP_RW(fg_monitor_cmd, HQ_POWER_SUPPLY_PROP_FG_MONITOR_CMD, 10);
LENOVO_CLASS_PROP_RO(chip_sc7603a, HQ_POWER_SUPPLY_PROP_IC_SC760X_1);
LENOVO_CLASS_PROP_RO(chip_sc7603b, HQ_POWER_SUPPLY_PROP_IC_SC760X_2);
LENOVO_CLASS_PROP_RO(fg1_soc, HQ_POWER_SUPPLY_PROP_FG_1_SOC);
LENOVO_CLASS_PROP_RO(fg2_soc, HQ_POWER_SUPPLY_PROP_FG_2_SOC);
LENOVO_CLASS_PROP_RO(sc8541_master_ibus, HQ_POWER_SUPPLY_PROP_SC8541_MATSET_IBUS);
LENOVO_CLASS_PROP_RO(sc8541_slave_ibus, HQ_POWER_SUPPLY_PROP_SC8541_SLAVE_IBUS);
LENOVO_CLASS_PROP_RO(ibus_total, HQ_POWER_SUPPLY_PROP_IBUS_TOTAL);
LENOVO_CLASS_PROP_RO(sc8541_dump_register, HQ_POWER_SUPPLY_PROP_SC8541_DUMP);
LENOVO_CLASS_PROP_RO(pps_status, HQ_POWER_SUPPLY_PROP_PPS_STATUS);
LENOVO_CLASS_PROP_RW(fg1_used, HQ_POWER_SUPPLY_PROP_FG_1_BATTERY_USED, 10);
LENOVO_CLASS_PROP_RW(fg2_used, HQ_POWER_SUPPLY_PROP_FG_2_BATTERY_USED, 10);

#undef LENOVO_CLASS_PROP_RO

static ssize_t battery_chg_notify_store(const struct class *c,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	bool val;

	if (kstrtobool(buf, &val))
		return -EINVAL;

	if (val) {
		bcdev->notify_en = false;
		battery_chg_notify_enable(bcdev);
	} else {
		bcdev->notify_en = true;
		battery_chg_notify_disable(bcdev);
	}

	return count;
}
static CLASS_ATTR_WO(battery_chg_notify);

/* Battery classes with wireless */
static struct attribute *battery_class_attrs[] = {
	&class_attr_soh.attr,
	&class_attr_resistance.attr,
	&class_attr_moisture_detection_status.attr,
	&class_attr_moisture_detection_en.attr,
	&class_attr_wireless_boost_en.attr,
	&class_attr_fake_soc.attr,
	&class_attr_wireless_fw_update.attr,
	&class_attr_wireless_fw_force_update.attr,
	&class_attr_wireless_fw_version.attr,
	&class_attr_wireless_fw_crc.attr,
	&class_attr_wireless_fw_update_time_ms.attr,
	&class_attr_wireless_type.attr,
	&class_attr_ship_mode_en.attr,
	&class_attr_restrict_chg.attr,
	&class_attr_restrict_cur.attr,
	&class_attr_usb_num_ports.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_usb_typec_compliant.attr,
	&class_attr_battery_parallel_cell_count.attr,
	&class_attr_typec_cc_orientation.attr,
	&class_attr_typec_cc_orientation2.attr,
	&class_attr_chip_main_charge.attr,
	&class_attr_chip_fg1.attr,
	&class_attr_chip_fg2.attr,
	&class_attr_chip_cp1.attr,
	&class_attr_chip_cp2.attr,
	&class_attr_chip_qi.attr,
	&class_attr_chip_pdphy.attr,
	&class_attr_chip_qc.attr,
	&class_attr_chip_sc7603a.attr,
	&class_attr_chip_sc7603b.attr,
	&class_attr_battery1_manufacturer.attr,
	&class_attr_fg1_temp.attr,
	&class_attr_fg1_current.attr,
	&class_attr_fg1_voltage.attr,
	&class_attr_fg1_produce_date.attr,
	&class_attr_fg1_activate_date.attr,
	&class_attr_fg1_soh.attr,
	&class_attr_fg1_cycle_count.attr,
	&class_attr_fg_monitor_cmd.attr,
	&class_attr_battery2_manufacturer.attr,
	&class_attr_fg2_temp.attr,
	&class_attr_fg2_current.attr,
	&class_attr_fg2_voltage.attr,
	&class_attr_fg2_produce_date.attr,
	&class_attr_fg2_activate_date.attr,
	&class_attr_fg2_soh.attr,
	&class_attr_fg2_cycle_count.attr,
	&class_attr_charge_control_en.attr,
	&class_attr_fg1_soc.attr,
	&class_attr_fg2_soc.attr,
	&class_attr_sc8541_master_ibus.attr,
	&class_attr_sc8541_slave_ibus.attr,
	&class_attr_ibus_total.attr,
	&class_attr_sc8541_dump_register.attr,
	&class_attr_pps_status.attr,
	&class_attr_fg1_used.attr,
	&class_attr_fg2_used.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_class);

static struct attribute *battery_class_pmw6100_attrs[] = {
	&class_attr_soh.attr,
	&class_attr_resistance.attr,
	&class_attr_moisture_detection_status.attr,
	&class_attr_moisture_detection_en.attr,
	&class_attr_wireless_boost_en.attr,
	&class_attr_fake_soc.attr,
	&class_attr_wireless_fw_update.attr,
	&class_attr_wireless_fw_force_update.attr,
	&class_attr_wireless_fw_version.attr,
	&class_attr_wireless_fw_crc.attr,
	&class_attr_wireless_fw_update_time_ms.attr,
	&class_attr_wireless_type.attr,
	&class_attr_ship_mode_en.attr,
	&class_attr_restrict_chg.attr,
	&class_attr_restrict_cur.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_usb_typec_compliant.attr,
	&class_attr_charge_control_en.attr,
	&class_attr_battery_parallel_cell_count.attr,
	&class_attr_battery_chg_notify.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_class_pmw6100);

static struct attribute *battery_class_usb_2_attrs[] = {
	&class_attr_soh.attr,
	&class_attr_resistance.attr,
	&class_attr_moisture_detection_status.attr,
	&class_attr_moisture_detection_usb_2_status.attr,
	&class_attr_moisture_detection_en.attr,
	&class_attr_moisture_detection_usb_2_en.attr,
	&class_attr_wireless_boost_en.attr,
	&class_attr_fake_soc.attr,
	&class_attr_wireless_fw_update.attr,
	&class_attr_wireless_fw_force_update.attr,
	&class_attr_wireless_fw_version.attr,
	&class_attr_wireless_fw_crc.attr,
	&class_attr_wireless_fw_update_time_ms.attr,
	&class_attr_wireless_type.attr,
	&class_attr_ship_mode_en.attr,
	&class_attr_restrict_chg.attr,
	&class_attr_restrict_cur.attr,
	&class_attr_usb_num_ports.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_usb_2_real_type.attr,
	&class_attr_usb_typec_compliant.attr,
	&class_attr_usb_2_typec_compliant.attr,
	&class_attr_charge_control_en.attr,
	&class_attr_battery_parallel_cell_count.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_class_usb_2);

/* Battery classes without wireless */
static struct attribute *battery_class_no_wls_attrs[] = {
	&class_attr_soh.attr,
	&class_attr_resistance.attr,
	&class_attr_moisture_detection_status.attr,
	&class_attr_moisture_detection_en.attr,
	&class_attr_fake_soc.attr,
	&class_attr_ship_mode_en.attr,
	&class_attr_restrict_chg.attr,
	&class_attr_restrict_cur.attr,
	&class_attr_usb_num_ports.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_usb_typec_compliant.attr,
	&class_attr_charge_control_en.attr,
	&class_attr_battery_parallel_cell_count.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_class_no_wls);

static struct attribute *battery_class_no_wls_usb_2_attrs[] = {
	&class_attr_soh.attr,
	&class_attr_resistance.attr,
	&class_attr_moisture_detection_status.attr,
	&class_attr_moisture_detection_usb_2_status.attr,
	&class_attr_moisture_detection_en.attr,
	&class_attr_moisture_detection_usb_2_en.attr,
	&class_attr_fake_soc.attr,
	&class_attr_ship_mode_en.attr,
	&class_attr_restrict_chg.attr,
	&class_attr_restrict_cur.attr,
	&class_attr_usb_num_ports.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_usb_2_real_type.attr,
	&class_attr_usb_typec_compliant.attr,
	&class_attr_usb_2_typec_compliant.attr,
	&class_attr_charge_control_en.attr,
	&class_attr_battery_parallel_cell_count.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_class_no_wls_usb_2);

#ifdef CONFIG_DEBUG_FS
static void battery_chg_add_debugfs(struct battery_chg_dev *bcdev)
{
	int rc;
	struct dentry *dir;

	dir = debugfs_create_dir("battery_charger", NULL);
	if (IS_ERR(dir)) {
		rc = PTR_ERR(dir);
		pr_err("Failed to create charger debugfs directory, rc=%d\n",
			rc);
		return;
	}

	bcdev->debugfs_dir = dir;
	debugfs_create_bool("block_tx", 0600, dir, &bcdev->block_tx);
}
#else
static void battery_chg_add_debugfs(struct battery_chg_dev *bcdev) { }
#endif

static int battery_chg_parse_dt(struct battery_chg_dev *bcdev)
{
	struct device_node *node = bcdev->dev->of_node;
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	int i, rc, len;
	u32 prev, val;

	bcdev->wls_not_supported = of_property_read_bool(node,
			"qcom,wireless-charging-not-supported");

	of_property_read_string(node, "qcom,wireless-fw-name",
				&bcdev->wls_fw_name);

	of_property_read_u32(node, "qcom,shutdown-voltage",
				&bcdev->shutdown_volt_mv);

	bcdev->ship_mode_immediate = of_property_read_bool(node, "qcom,ship-mode-immediate");

	rc = read_property_id(bcdev, pst, BATT_CHG_CTRL_LIM_MAX);
	if (rc < 0) {
		pr_err("Failed to read prop BATT_CHG_CTRL_LIM_MAX, rc=%d\n",
			rc);
		return rc;
	}

	rc = of_property_count_elems_of_size(node, "qcom,thermal-mitigation",
						sizeof(u32));
	if (rc <= 0) {

		rc = of_property_read_u32(node, "qcom,thermal-mitigation-step",
						&val);

		if (rc < 0)
			return 0;

		if (val < 500000 || val >= pst->prop[BATT_CHG_CTRL_LIM_MAX]) {
			pr_err("thermal_fcc_step %d is invalid\n", val);
			return -EINVAL;
		}

		bcdev->thermal_fcc_step = val;
		len = pst->prop[BATT_CHG_CTRL_LIM_MAX] / bcdev->thermal_fcc_step;

		/*
		 * FCC values must be above 500mA.
		 * Since len is truncated when calculated, check and adjust len so
		 * that the above requirement is met.
		 */
		if (pst->prop[BATT_CHG_CTRL_LIM_MAX] - (bcdev->thermal_fcc_step * len) < 500000)
			len = len - 1;
	} else {
		bcdev->thermal_fcc_step = 0;
		len = rc;
		prev = pst->prop[BATT_CHG_CTRL_LIM_MAX];

		for (i = 0; i < len; i++) {
			rc = of_property_read_u32_index(node, "qcom,thermal-mitigation",
				i, &val);
			if (rc < 0)
				return rc;

			if (val > prev) {
				pr_err("Thermal levels should be in descending order\n");
				bcdev->num_thermal_levels = -EINVAL;
				return 0;
			}

			prev = val;
		}

		bcdev->thermal_levels = devm_kcalloc(bcdev->dev, len + 1,
						sizeof(*bcdev->thermal_levels),
						GFP_KERNEL);
		if (!bcdev->thermal_levels)
			return -ENOMEM;

		/*
		 * Element 0 is for normal charging current. Elements from index 1
		 * onwards is for thermal mitigation charging currents.
		 */

		bcdev->thermal_levels[0] = pst->prop[BATT_CHG_CTRL_LIM_MAX];

		rc = of_property_read_u32_array(node, "qcom,thermal-mitigation",
					&bcdev->thermal_levels[1], len);
		if (rc < 0) {
			pr_err("Error in reading qcom,thermal-mitigation, rc=%d\n", rc);
			return rc;
		}
	}

	bcdev->num_thermal_levels = len;
	bcdev->thermal_fcc_ua = pst->prop[BATT_CHG_CTRL_LIM_MAX];

	return 0;
}

static int battery_chg_reboot_notify(struct notifier_block *nb, unsigned long code,
		void *unused)
{
	struct battery_charger_notify_msg msg_notify = { { 0 } };
	struct battery_chg_dev *bcdev = container_of(nb, struct battery_chg_dev,
						     reboot_notifier);
	int rc;

	msg_notify.hdr.owner = MSG_OWNER_BC;
	msg_notify.hdr.type = MSG_TYPE_NOTIFY;
	msg_notify.hdr.opcode = bcdev->config->opcode;

	rc = battery_chg_write(bcdev, &msg_notify, sizeof(msg_notify), BC_WAIT_TIME_MS);
	if (rc < 0)
		pr_err("Failed to send shutdown notification rc=%d\n", rc);

	if (!bcdev->ship_mode_en)
		return NOTIFY_DONE;

	if (code == SYS_POWER_OFF && !bcdev->ship_mode_immediate)
		battery_chg_ship_mode(bcdev);

	return NOTIFY_DONE;
}

static void panel_event_notifier_callback(enum panel_event_notifier_tag tag,
			struct panel_event_notification *notification, void *data)
{
	struct battery_chg_dev *bcdev = data;

	if (!notification) {
		pr_debug("Invalid panel notification\n");
		return;
	}

	pr_debug("panel event received, type: %d\n", notification->notif_type);
	switch (notification->notif_type) {
	case DRM_PANEL_EVENT_BLANK:
		battery_chg_notify_disable(bcdev);
		break;
	case DRM_PANEL_EVENT_UNBLANK:
		battery_chg_notify_enable(bcdev);
		break;
	default:
		pr_debug("Ignore panel event: %d\n", notification->notif_type);
		break;
	}
}

static int battery_chg_register_panel_notifier(struct battery_chg_dev *bcdev)
{
	struct device_node *np = bcdev->dev->of_node;
	struct device_node *pnode;
	struct drm_panel *panel, *active_panel = NULL;
	void *cookie = NULL;
	int i, count, rc;

	count = of_count_phandle_with_args(np, "qcom,display-panels", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		pnode = of_parse_phandle(np, "qcom,display-panels", i);
		if (!pnode)
			return -ENODEV;

		panel = of_drm_find_panel(pnode);
		of_node_put(pnode);
		if (!IS_ERR(panel)) {
			active_panel = panel;
			break;
		}
	}

	if (!active_panel) {
		rc = PTR_ERR(panel);
		if (rc != -EPROBE_DEFER)
			dev_err(bcdev->dev, "Failed to find active panel, rc=%d\n", rc);
		return rc;
	}

	cookie = panel_event_notifier_register(
			PANEL_EVENT_NOTIFICATION_PRIMARY,
			PANEL_EVENT_NOTIFIER_CLIENT_BATTERY_CHARGER,
			active_panel,
			panel_event_notifier_callback,
			(void *)bcdev);
	if (IS_ERR(cookie)) {
		rc = PTR_ERR(cookie);
		dev_err(bcdev->dev, "Failed to register panel event notifier, rc=%d\n", rc);
		return rc;
	}

	pr_debug("register panel notifier successful\n");
	bcdev->notifier_cookie = cookie;
	return 0;
}

static int
battery_chg_get_max_charge_cntl_limit(struct thermal_cooling_device *tcd,
					unsigned long *state)
{
	struct battery_chg_dev *bcdev = tcd->devdata;

	*state = bcdev->num_thermal_levels;

	return 0;
}

static int
battery_chg_get_cur_charge_cntl_limit(struct thermal_cooling_device *tcd,
					unsigned long *state)
{
	struct battery_chg_dev *bcdev = tcd->devdata;

	*state = bcdev->curr_thermal_level;

	return 0;
}

static int
battery_chg_set_cur_charge_cntl_limit(struct thermal_cooling_device *tcd,
					unsigned long state)
{
	struct battery_chg_dev *bcdev = tcd->devdata;

	return battery_psy_set_charge_current(bcdev, (int)state);
}

static const struct thermal_cooling_device_ops battery_tcd_ops = {
	.get_max_state = battery_chg_get_max_charge_cntl_limit,
	.get_cur_state = battery_chg_get_cur_charge_cntl_limit,
	.set_cur_state = battery_chg_set_cur_charge_cntl_limit,
};

#ifdef CONFIG_QTI_CHARGER_NFC_VREG
int nfc_regulator_en(struct bharger_regulator *vreg, bool enable)
{
	struct battery_chg_dev *bcdev;
	struct battery_enable_w_boost_req_msg req_msg = { { 0 } };
	int uv;

	bcdev = container_of(vreg, struct battery_chg_dev, nfc_vreg);
	req_msg.client = BOOST_NFC;
	req_msg.hdr.owner = MSG_OWNER_BC;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = BC_CHG_CTRL_EN_W_BOOST;

	uv = vreg->min_uv;
	if (vreg->min_uv < 0 && enable)
		uv = DEFAULT_NFC_MIN_VOLTAGE;
	else if (!enable)
		uv = 0;
	req_msg.boost_microvolt = uv;

	if (enable) {
		req_msg.enable = BOOST_ENABLE;
		req_msg.mode = PWM_FF_MODE;
	} else {
		req_msg.enable = BOOST_DISABLE;
		req_msg.mode = OFF_MODE;
	}

	return battery_chg_write(bcdev, &req_msg, sizeof(req_msg), BC_WAIT_TIME_MS);
}
#endif

static int register_extcon_conn_type(struct battery_chg_dev *bcdev)
{
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];
	int rc;

	if (!bcdev->micro_usb)
		return 0;

	rc = read_property_id(bcdev, pst, USB_CONNECTOR_TYPE);
	if (rc < 0) {
		dev_err(bcdev->dev, "Failed to read prop USB_CONNECTOR_TYPE, rc=%d\n",
			rc);
		return rc;
	}

	if (pst->prop[USB_CONNECTOR_TYPE] != USB_CONNECTOR_TYPE_MICRO_USB)
		return 0;

	bcdev->connector_type = USB_CONNECTOR_TYPE_MICRO_USB;
	bcdev->usb_prev_mode = EXTCON_NONE;

	bcdev->extcon = devm_extcon_dev_allocate(bcdev->dev,
						bcdev_usb_extcon_cable);
	if (IS_ERR(bcdev->extcon)) {
		rc = PTR_ERR(bcdev->extcon);
		dev_err(bcdev->dev, "Failed to allocate extcon device rc=%d\n",
			rc);
		return rc;
	}

	rc = devm_extcon_dev_register(bcdev->dev, bcdev->extcon);
	if (rc < 0) {
		dev_err(bcdev->dev, "Failed to register extcon device rc=%d\n",
			rc);
		return rc;
	}

	rc = extcon_set_property_capability(bcdev->extcon, EXTCON_USB,
					    EXTCON_PROP_USB_SS);
	if (rc < 0) {
		dev_err(bcdev->dev, "Failed to set USB property capability, rc=%d\n",
			rc);
		return rc;
	}

	rc = extcon_set_property_capability(bcdev->extcon,
					    EXTCON_USB_HOST, EXTCON_PROP_USB_SS);
	if (rc < 0)
		dev_err(bcdev->dev, "Failed to set USB_HOST property capability, rc=%d\n",
			rc);

	return rc;
}

static int battery_chg_suspend(struct platform_device *pdev,
					pm_message_t state)
{
	struct battery_chg_dev *bcdev = platform_get_drvdata(pdev);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_LENOVO];

	dev_info(bcdev->dev, "battery_chg_suspend\n");
	mutex_lock(&bcdev->suspend_lock);
	bcdev->is_suspended = true;
	mutex_unlock(&bcdev->suspend_lock);

	write_property_id(bcdev, pst, HQ_POWER_SUPPLY_PROP_FG_MONITOR_CMD, 0);

	return 0;
}

static int battery_chg_resume(struct platform_device *pdev)
{
	struct battery_chg_dev *bcdev = platform_get_drvdata(pdev);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_LENOVO];

	dev_info(bcdev->dev, "battery_chg_resume\n");
	mutex_lock(&bcdev->suspend_lock);
	bcdev->is_suspended = false;
	mutex_unlock(&bcdev->suspend_lock);

	write_property_id(bcdev, pst, HQ_POWER_SUPPLY_PROP_FG_MONITOR_CMD, 1);

	return 0;
}


static int battery_chg_probe(struct platform_device *pdev)
{
	struct battery_chg_dev *bcdev;
	struct device *dev = &pdev->dev;
	struct pmic_glink_client_data client_data = { };
	struct thermal_cooling_device *tcd;
	struct psy_state *pst;
	int rc, i;

	bcdev = devm_kzalloc(&pdev->dev, sizeof(*bcdev), GFP_KERNEL);
	if (!bcdev)
		return -ENOMEM;

	bcdev->config = of_device_get_match_data(&pdev->dev);

	if (!bcdev->config)
		return -ENODEV;

	bcdev->psy_list[PSY_TYPE_BATTERY].map = battery_prop_map;
	bcdev->psy_list[PSY_TYPE_BATTERY].prop_count = BATT_PROP_MAX;
	bcdev->psy_list[PSY_TYPE_BATTERY].opcode_get = BC_BATTERY_STATUS_GET;
	bcdev->psy_list[PSY_TYPE_BATTERY].opcode_set = BC_BATTERY_STATUS_SET;
	bcdev->psy_list[PSY_TYPE_USB].map = usb_prop_map;
	bcdev->psy_list[PSY_TYPE_USB].prop_count = USB_PROP_MAX;
	bcdev->psy_list[PSY_TYPE_USB].opcode_get = BC_USB_STATUS_GET(USB_1_PORT_ID);
	bcdev->psy_list[PSY_TYPE_USB].opcode_set = BC_USB_STATUS_SET(USB_1_PORT_ID);
	bcdev->psy_list[PSY_TYPE_WLS].map = wls_prop_map;
	bcdev->psy_list[PSY_TYPE_WLS].prop_count = WLS_PROP_MAX;
	bcdev->psy_list[PSY_TYPE_WLS].opcode_get = BC_WLS_STATUS_GET;
	bcdev->psy_list[PSY_TYPE_WLS].opcode_set = BC_WLS_STATUS_SET;
	bcdev->psy_list[PSY_TYPE_LENOVO].map = hq_prop_map;
	bcdev->psy_list[PSY_TYPE_LENOVO].prop_count = HQ_POWER_SUPPLY_PROPERTIES_MAX;
	bcdev->psy_list[PSY_TYPE_LENOVO].opcode_get = BC_LENOVO_PROP_GET;
	bcdev->psy_list[PSY_TYPE_LENOVO].opcode_set = BC_LENOVO_PROP_SET;
	bcdev->usb_active[USB_1_PORT_ID] = true;

	rc = of_property_read_u32(dev->of_node, "qcom,multiport-usb", &bcdev->num_usb_ports);
	if (rc < 0)
		bcdev->num_usb_ports = 1;

	if (bcdev->num_usb_ports > NUM_USB_PORTS) {
		dev_err(dev, "Invalid num_usb_ports %d, maximum is %d\n",
				bcdev->num_usb_ports, NUM_USB_PORTS);
		return -EINVAL;
	}

	if (bcdev->num_usb_ports == 2) {
		bcdev->psy_list[PSY_TYPE_USB_2].map = usb_prop_map;
		bcdev->psy_list[PSY_TYPE_USB_2].prop_count = USB_PROP_MAX;
		bcdev->psy_list[PSY_TYPE_USB_2].opcode_get = BC_USB_STATUS_GET(USB_2_PORT_ID);
		bcdev->psy_list[PSY_TYPE_USB_2].opcode_set = BC_USB_STATUS_SET(USB_2_PORT_ID);
	}

	for (i = 0; i < PSY_TYPE_MAX; i++) {
		if (i == PSY_TYPE_USB_2 && bcdev->num_usb_ports < 2)
			continue;
		bcdev->psy_list[i].prop =
			devm_kcalloc(&pdev->dev, bcdev->psy_list[i].prop_count,
					sizeof(u32), GFP_KERNEL);
		if (!bcdev->psy_list[i].prop)
			return -ENOMEM;
	}

	bcdev->psy_list[PSY_TYPE_BATTERY].model =
		devm_kzalloc(&pdev->dev, MAX_STR_LEN, GFP_KERNEL);
	if (!bcdev->psy_list[PSY_TYPE_BATTERY].model)
		return -ENOMEM;

	if (bcdev->config->batt_psy_desc == &x1e80100_batt_psy_desc) {
		bcdev->psy_list[PSY_TYPE_BATTERY].manufacturer =
			devm_kzalloc(&pdev->dev, MAX_STR_LEN, GFP_KERNEL);
		if (!bcdev->psy_list[PSY_TYPE_BATTERY].manufacturer)
			return -ENOMEM;
	}

	bcdev->lenovo_chg_initialized = false;
	bcdev->is_suspended = false;
	mutex_init(&bcdev->rw_lock);
	mutex_init(&bcdev->suspend_lock);
	init_rwsem(&bcdev->state_sem);
	init_completion(&bcdev->ack);
	init_completion(&bcdev->fw_buf_ack);
	init_completion(&bcdev->fw_update_ack);
	INIT_DELAYED_WORK(&bcdev->lenovo_chg_monitor_work,
			  lenovo_chg_monitor_work);
	INIT_WORK(&bcdev->subsys_up_work, battery_chg_subsys_up_work);
	INIT_WORK(&bcdev->usb_type_work, battery_chg_update_usb_type_work);
	INIT_WORK(&bcdev->battery_check_work, battery_chg_check_status_work);
	INIT_DELAYED_WORK(&bcdev->device_info_work, devices_get_informs_work);
	bcdev->charge_wakelock =
		wakeup_source_register(NULL, "charge_wakelock");
	bcdev->dev = dev;

	rc = battery_chg_register_panel_notifier(bcdev);
	if (rc < 0)
		return rc;

	client_data.id = MSG_OWNER_BC;
	client_data.name = "battery_charger";
	client_data.msg_cb = battery_chg_callback;
	client_data.priv = bcdev;
	client_data.state_cb = battery_chg_state_cb;

	bcdev->client = pmic_glink_register_client(dev, &client_data);
	if (IS_ERR(bcdev->client)) {
		rc = PTR_ERR(bcdev->client);
		if (rc != -EPROBE_DEFER)
			dev_err(dev, "Error in registering with pmic_glink %d\n",
				rc);
		goto reg_error;
	}

	down_write(&bcdev->state_sem);
	atomic_set(&bcdev->state, PMIC_GLINK_STATE_UP);
	/*
	 * This should be initialized here so that battery_chg_callback
	 * can run successfully when battery_chg_parse_dt() starts
	 * reading BATT_CHG_CTRL_LIM_MAX parameter and waits for a response.
	 */
	bcdev->initialized = true;
	up_write(&bcdev->state_sem);

	bcdev->reboot_notifier.notifier_call = battery_chg_reboot_notify;
	bcdev->reboot_notifier.priority = 255;
	register_reboot_notifier(&bcdev->reboot_notifier);

	rc = battery_chg_parse_dt(bcdev);
	if (rc < 0) {
		dev_err(dev, "Failed to parse dt rc=%d\n", rc);
		goto error;
	}

	bcdev->restrict_fcc_ua = DEFAULT_RESTRICT_FCC_UA;
	platform_set_drvdata(pdev, bcdev);
	bcdev->fake_soc = -EINVAL;
	bcdev->micro_usb = of_property_read_bool(bcdev->dev->of_node, "qcom,micro-usb");
	rc = battery_chg_init_psy(bcdev);
	if (rc < 0)
		goto error;

	bcdev->battery_class.name = "qcom-battery";

	if (of_device_is_compatible(dev->of_node, "qcom,pmw6100-battery-charger")) {
		bcdev->battery_class.class_groups = battery_class_pmw6100_groups;

	} else if (bcdev->wls_not_supported) {
		if (bcdev->num_usb_ports == 2)
			bcdev->battery_class.class_groups = battery_class_no_wls_usb_2_groups;
		else
			bcdev->battery_class.class_groups = battery_class_no_wls_groups;
	} else {
		if (bcdev->num_usb_ports == 2)
			bcdev->battery_class.class_groups = battery_class_usb_2_groups;
		else
			bcdev->battery_class.class_groups = battery_class_groups;
	}

#ifdef CONFIG_QTI_CHARGER_NFC_VREG
	rc = nfc_init_regulator(bcdev->dev, &bcdev->nfc_vreg);
	if (rc < 0) {
		dev_err(bcdev->dev, "Couldn't initialize nfc regulator rc=%d\n", rc);
		goto error;
	}
#endif

	rc = class_register(&bcdev->battery_class);
	if (rc < 0) {
		dev_err(dev, "Failed to create battery_class rc=%d\n", rc);
		goto error;
	}
	rc = hq_battery_sysfs_create_group(bcdev);
	if (rc < 0) {
		dev_err(dev, "Failed to create battery class rc=%d\n", rc);
		goto error;
	}
	rc = hq_usb_sysfs_create_group(bcdev);
	if (rc < 0) {
		dev_err(dev, "Failed to create usb class rc=%d\n", rc);
		goto error;
	}
	pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	tcd = devm_thermal_of_cooling_device_register(dev, dev->of_node,
			(char *)pst->psy->desc->name, bcdev, &battery_tcd_ops);
	if (IS_ERR_OR_NULL(tcd)) {
		rc = PTR_ERR_OR_ZERO(tcd);
		dev_err(dev, "Failed to register thermal cooling device rc=%d\n",
			rc);
		class_unregister(&bcdev->battery_class);
		goto error;
	}

	bcdev->wls_fw_update_time_ms = WLS_FW_UPDATE_TIME_MS;
	battery_chg_add_debugfs(bcdev);
	bcdev->notify_en = false;
	battery_chg_notify_enable(bcdev);
	device_init_wakeup(bcdev->dev, true);
	rc = register_extcon_conn_type(bcdev);
	if (rc < 0) {
		if (bcdev->connector_type == USB_CONNECTOR_TYPE_MICRO_USB) {
			dev_err(dev, "Failed to register extcon for micro USB, rc=%d\n",
				rc);
			goto error;
		} else {
			dev_warn(dev, "Failed to register extcon rc=%d\n", rc);
		}
	}

	if (bcdev->connector_type == USB_CONNECTOR_TYPE_MICRO_USB) {
		bcdev->typec_class = qti_typec_class_init(bcdev->dev);
		if (IS_ERR_OR_NULL(bcdev->typec_class)) {
			rc = PTR_ERR_OR_ZERO(bcdev->typec_class);
			dev_err(dev, "Failed to init typec class err=%d\n", rc);
			goto error;
		}
	}

	schedule_work(&bcdev->usb_type_work);

	rc = get_charge_control_en(bcdev);
	if (rc < 0)
		pr_debug("Failed to read charge_control_en, rc = %d\n", rc);
	queue_delayed_work_on(32, system_wq, &bcdev->lenovo_chg_monitor_work,
			      5000);
	queue_delayed_work_on(32, system_wq, &bcdev->device_info_work, 3750);
	bcdev->battery_abnormal = false;
	bcdev->fake_abnormal_level = -EINVAL;
	gbcdev = bcdev;

	return 0;
error:
	down_write(&bcdev->state_sem);
	atomic_set(&bcdev->state, PMIC_GLINK_STATE_DOWN);
	bcdev->initialized = false;
	up_write(&bcdev->state_sem);

	cancel_work_sync(&bcdev->usb_type_work);
	cancel_work_sync(&bcdev->subsys_up_work);
	cancel_work_sync(&bcdev->battery_check_work);
	complete(&bcdev->ack);
	unregister_reboot_notifier(&bcdev->reboot_notifier);
	pmic_glink_unregister_client(bcdev->client);
reg_error:
	if (bcdev->notifier_cookie)
		panel_event_notifier_unregister(bcdev->notifier_cookie);
	return rc;
}

static void battery_chg_remove(struct platform_device *pdev)
{
	struct battery_chg_dev *bcdev = platform_get_drvdata(pdev);

	down_write(&bcdev->state_sem);
	atomic_set(&bcdev->state, PMIC_GLINK_STATE_DOWN);
	bcdev->initialized = false;
	up_write(&bcdev->state_sem);

	if (bcdev->notifier_cookie)
		panel_event_notifier_unregister(bcdev->notifier_cookie);

	device_init_wakeup(bcdev->dev, false);
	debugfs_remove_recursive(bcdev->debugfs_dir);
	class_unregister(&bcdev->battery_class);
	cancel_work_sync(&bcdev->subsys_up_work);
	cancel_work_sync(&bcdev->usb_type_work);
	cancel_work_sync(&bcdev->battery_check_work);
	unregister_reboot_notifier(&bcdev->reboot_notifier);
	pmic_glink_unregister_client(bcdev->client);
	if (bcdev->typec_class)
		qti_typec_class_deinit(bcdev->typec_class);
}

static struct battery_charger_config default_config = {
	.opcode = BC_SHUTDOWN_NOTIFY,
	.batt_psy_desc = &batt_psy_desc,
};

static struct battery_charger_config canoe_config = {
	.opcode = BC_SHUTDOWN_NOTIFY_V2,
	.batt_psy_desc = &batt_psy_desc,
};

static struct battery_charger_config x1e80100_config = {
	.opcode = BC_SHUTDOWN_NOTIFY,
	.batt_psy_desc = &x1e80100_batt_psy_desc,
};

static const struct of_device_id battery_chg_match_table[] = {
	{ .compatible = "qcom,battery-charger", .data = (void *)&default_config},
	{ .compatible = "qcom,pmw6100-battery-charger", .data = (void *)&default_config},
	{ .compatible = "qcom,canoe-battery-charger", .data = (void *)&canoe_config},
	{ .compatible = "qcom,x1e80100-battery-charger", .data = (void *)&x1e80100_config},
	{}
};

static struct platform_driver battery_chg_driver = {
	.driver = {
		.name = "qti_battery_charger",
		.of_match_table = battery_chg_match_table,
	},
	.probe = battery_chg_probe,
	.remove = battery_chg_remove,
	.suspend = battery_chg_suspend,
	.resume = battery_chg_resume,
};
module_platform_driver(battery_chg_driver);

MODULE_DESCRIPTION("QTI Glink battery charger driver");
MODULE_LICENSE("GPL");
