/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __QCOM_LPM_H__
#define __QCOM_LPM_H__

#include <linux/cpumask.h>

#define MAX_LPM_CPUS		8
#define MAXSAMPLES		5
#define PRED_RESI_FACT		1
#define MIN_RESI_TIMES		3
#define MAX_PRED_TIMER_ADD	1000
#define PRED_TIMER_ADD		100
#define PRED_PREMATURE_CNT	3
#define PRED_REF_STDDEV		500
#define PRED_ACTIVE_TIME	0
#define IPI_PRED_REF_STDDEV	500
#define CLUST_SMPL_INVLD_TIME	40000
#define CLUST_BIAS_TIME_MSEC	10
#define MAX_CLUSTER_STATES	4

extern u32 premature_resi_div_cpu;
extern u32 pred_active_time;
extern u32 resi_fact;
extern u32 pred_timer_add;
extern u32 pred_premature_cnt;
extern u32 pred_ref_stddev;
extern u32 ipi_pred_ref_stddev;
extern bool optimized_resi;
extern bool bias_disabled;
extern bool cluster_bias_disabled;
extern bool premature_ext_disabled;
extern bool sleep_disabled;
extern bool prediction_disabled;

struct qcom_cluster_node {
	struct lpm_cluster *cluster;
	struct kobject *kobj;
	int state_idx;
	struct kobj_attribute disable_attr;
	struct kobj_attribute usage_attr[MAX_CLUSTER_STATES];
	struct kobj_attribute idle_time_attr[MAX_CLUSTER_STATES];
	struct kobj_attribute rejected_attr[MAX_CLUSTER_STATES];
	struct attribute_group *attr_group;
	struct attribute **attrs;
};

struct history_lpm {
	u32 mode[MAXSAMPLES];
	s64 resi[MAXSAMPLES];
	int nsamp;
	u32 samples_idx;
};

struct history_ipi {
	s64 interval[MAXSAMPLES];
	u32 current_ptr;
	ktime_t cpu_idle_resched_ts;
};

struct lpm_cpu {
	int cpu;
	int enable;
	int last_idx;
	int timer_factor;
	struct notifier_block nb;
	struct cpuidle_driver *drv;
	struct cpuidle_device *dev;
	ktime_t next_wakeup;
	s64 predicted;
	bool history_invalid;
	bool predict_started;
	bool htmr_wkup;
	struct hrtimer histtimer;
	struct hrtimer biastimer;
	struct history_lpm lpm_history;
	struct history_ipi ipi_history;
	ktime_t now;
	ktime_t active_time;
	ktime_t exit_time;
	u64 bias;
	int64_t next_pred_time;
	u32 pred_type;
	u64 select_reason;
	u64 hist_reason;
	bool ipi_pending;
	spinlock_t lock;
	bool cpu_off_invoked;
};

struct cluster_history {
	ktime_t residency;
	ktime_t entry_time;
	int mode;
};

struct lpm_cluster {
	struct device *dev;
	u32 samples_idx;
	bool history_invalid;
	bool htmr_wkup;
	int entry_idx;
	int restrict_idx;
	int nsamp;
	int timer_cpu;
	u64 samples_invalid_time;
	u32 pred_premature_cnt;
	struct cluster_history history[MAXSAMPLES];
	struct generic_pm_domain *genpd;
	struct qcom_cluster_node *dev_node[MAX_CLUSTER_STATES];
	struct kobject *dev_kobj;
	struct notifier_block genpd_nb;
	struct work_struct work;
	struct hrtimer histtimer;
	ktime_t entry_time;
	ktime_t next_wakeup;
	ktime_t __percpu *cpu_next_wakeup;
	ktime_t pred_wakeup;
	ktime_t now;
	u64 pred_residency;
	bool state_allowed[MAX_CLUSTER_STATES];
	struct list_head list;
	spinlock_t lock;
	bool predicted;
	bool initialized;
	bool is_timer_expired;
	bool is_timer_queued;
	bool need_timer_requeue;
	bool use_bias_timer;
	bool pre_timer;
};

struct cluster_governor {
	void (*select)(struct lpm_cpu *cpu_gov);
	void (*enable)(void);
	void (*disable)(void);
	void (*reflect)(struct lpm_cpu *cpu_gov);
};

DECLARE_PER_CPU(struct lpm_cpu, lpm_cpu_data);

int qcom_cluster_lpm_governor_init(void);
void qcom_cluster_lpm_governor_deinit(void);
void clear_cpu_predict_history(void);
int create_global_sysfs_nodes(void);
int create_cluster_sysfs_nodes(struct lpm_cluster *cluster_gov);
void register_cluster_governor_ops(struct cluster_governor *ops);
void remove_global_sysfs_nodes(void);
void remove_cluster_sysfs_nodes(struct lpm_cluster *cluster_gov);
s64 get_cpus_qos(const struct cpumask *mask);

#endif /* __QCOM_LPM_H__ */
