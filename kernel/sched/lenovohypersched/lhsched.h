/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Lenovo Hyper Scheduler Kernel Module - shared declarations
 *
 * Author: hyperscheduler@lenovo.com
 * Description: hyper schedule init
 *
 * The module is compiled from several translation units (lhsched_*.c); this
 * header carries the types, globals and prototypes shared between them.
 */

#ifndef LHSCHED_H
#define LHSCHED_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/cputime.h>
#include <linux/sched/task.h>
#include <linux/sched/stat.h>
#include <uapi/linux/sched/types.h>
#include <linux/hrtimer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/tracepoint.h>
#include <linux/jiffies.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/rwsem.h>
#include <trace/hooks/dtask.h>

#include "../sched.h"

/* Forward declarations and internal lock waiter definitions */
enum rwsem_waiter_type {
	RWSEM_WAITING_FOR_WRITE,
	RWSEM_WAITING_FOR_READ
};

struct rwsem_waiter {
	struct list_head list;
	struct task_struct *task;
	enum rwsem_waiter_type type;
	unsigned long timeout;
	bool handoff_set;
};

struct mutex_waiter {
	struct list_head list;
	struct task_struct *task;
	struct ww_acquire_ctx *ww_ctx;
};

extern void set_next_entity(struct cfs_rq *cfs_rq, struct sched_entity *se);

/* Data structures matching stock reverse engineering */

struct lenovo_static_attribute {
	int static_hyper_ux;
	int hyper_ux_depth;
	atomic64_t dynamic_hyper_ux;
	u64 enqueue_time;
	u64 dynamic_hyper_ux_start;
};

struct lenovo_task_struct;

struct lenovo_dynamic_attribute {
	raw_spinlock_t lock;
	int saved_policy;
	int saved_prio;
	u64 dyn_switched_flag;
	int normal_hyper_ux_prio;
	u64 sleeping_reset_work[4];
	u8 hyper_ux_boost[5];
	struct lenovo_task_struct *ltsk;
};

struct lenovo_task_struct {
	struct list_head hyper_entry;
	struct lenovo_static_attribute *lsa;
	struct lenovo_dynamic_attribute *lda;
};

struct lenovo_rq {
	struct list_head hyper_ux_thread_list;
	int num_hyper_ux_tasks;
	int hyper_ux_throttled;
	u64 hyper_ux_time;
	u64 hyper_ux_runtime;
	raw_spinlock_t hyper_ux_runtime_lock;
};

struct lenovo_ux_bandwidth {
	raw_spinlock_t hyper_ux_runtime_lock;
	u64 hyper_ux_period;
	u64 hyper_ux_runtime;
	u64 hyper_ux_max_runtime;
	struct hrtimer hyper_ux_period_timer;
	int hyper_ux_period_active;
};

#define LENOVO_TASK(p) ((struct lenovo_task_struct *)&(p)->android_oem_data1[0])
#define LENOVO_RQ(rq)  ((struct lenovo_rq *)&(rq)->android_oem_data1[0])

/* Global variables matching stock */
extern unsigned int hyper_log_ctl;
extern int hyper_max_dynamic_granularity;
extern unsigned int hyper_min_migration_delay;
extern unsigned int hyper_min_sched_delay_granularity;
extern bool hyper_module_modprobe_finish;
extern struct mutex hyper_proc_node_mutex;
extern unsigned int sysctl_sched_migration_cost_lenovo;
extern struct lenovo_ux_bandwidth def_ux_bandwidth;

/* Task-struct memory management */
int init_lenovo_static_task_struct(void *ltsk);
int init_lenovo_dynamic_task_struct(void *ltsk);
void init_lenovo_task_struct(struct task_struct *p);
void reinit_lenovo_task_struct(void *ltsk);

/* Testing task UX properties */
bool test_dynamic_hyper(struct task_struct *p, int type);
bool test_task_hyper(struct task_struct *p);

/* Dynamic queue operations */
void dynamic_hyper_enqueue(struct task_struct *p, int type, int incoming_depth);
void dynamic_hyper_dequeue(struct task_struct *p, int type);

/* Runqueue enqueue / dequeue hooks */
void enqueue_hyper_thread(struct rq *rq, struct task_struct *p);
void dequeue_hyper_thread(struct rq *rq, struct task_struct *p);
void sched_enqueue_hyper_thread(struct rq *rq, struct task_struct *p);
void sched_dequeue_hyper_thread(struct rq *rq, struct task_struct *p);

/* Candidate selection */
void pick_hyper_thread(struct rq *rq, struct task_struct **p,
		       struct sched_entity **se, bool *repick, bool simple);
void android_rvh_replace_next_task_fair_handler(void *unused, struct rq *rq,
						struct task_struct **p,
						struct sched_entity **se,
						bool *repick, bool simple,
						struct task_struct *prev);

/* Tracepoint probes */
void probe_android_vh_dup_task_struct(void *unused, struct task_struct *tsk,
				      struct task_struct *orig);
void probe_android_vh_free_task(void *unused, struct task_struct *p);
void probe_android_rvh_wake_up_new_task(void *unused, struct task_struct *p);
void probe_android_rvh_try_to_wake_up(void *unused, struct task_struct *p);
void probe_android_vh_mutex_wait_start(void *unused, struct mutex *lock);
void probe_android_vh_alter_mutex_list_add(void *unused, struct mutex *lock,
					   struct mutex_waiter *waiter,
					   struct list_head *list,
					   bool *already_on_list);
void probe_android_vh_mutex_unlock_slowpath(void *unused, struct mutex *lock);
void probe_android_vh_alter_rwsem_list_add(void *unused,
					   struct rwsem_waiter *waiter,
					   struct rw_semaphore *sem,
					   bool *already_on_list);
void probe_android_vh_rwsem_wake(void *unused, struct rw_semaphore *sem);
void probe_android_vh_rwsem_wake_finish(void *unused, struct rw_semaphore *sem);
void probe_android_vh_sched_stat_runtime(void *unused, struct task_struct *tsk,
					 u64 runtime, u64 vruntime);
int register_lenovo_extra_vendor_hook(void);

/* Mutex / RWSEM lock ordering */
bool mutex_list_add(struct task_struct *task, struct mutex_waiter *waiter,
		    struct list_head *head, struct mutex *lock);
void mutex_dynamic_hyper_enqueue(struct mutex *lock, struct task_struct *waiting);
void mutex_dynamic_hyper_dequeue(struct mutex *lock, struct task_struct *task);
bool rwsem_list_add(struct task_struct *task, struct rwsem_waiter *waiter,
		    struct list_head *head);
void rwsem_dynamic_hyper_enqueue(struct task_struct *trigger,
				struct task_struct *waiter_task,
				struct task_struct *owner,
				atomic_long_t *count);
void rwsem_dynamic_hyper_dequeue(atomic_long_t *count, struct task_struct *task);

/* /proc/lenovo_hyperschedule node operations */
int lenovo_hypernode_proc_init(void);

/* Priority attributes and inheritance */
int sched_setattr_directly(struct task_struct *p, struct sched_attr *attr);
int sched_setattr_dynamic(struct task_struct *p, struct sched_attr *attr);
int set_hyper_prio(struct task_struct *p, u32 prio);
int get_max_hyper_boost(struct task_struct *p);
int backup_sched_attr(struct task_struct *p);
int restore_sched_attr(struct task_struct *p);
void request_hyper_boost(struct task_struct *p, struct lenovo_dynamic_attribute *lda,
			 u32 prio, u32 type);
void clear_hyper_boost(struct task_struct *p, struct lenovo_dynamic_attribute *lda,
		       u32 type);
void set_cgroup_hyper_prio(struct task_struct *p);
void dynamic_prio_clear(void *ltsk);

/* Bandwidth and Runtime accounting */
void init_lenovo_rq_bandwidth(struct lenovo_ux_bandwidth *b, u64 period, u64 runtime);
void init_lenovo_rq_data(void *rq_data);
void sched_hyper_stat_runtime_handle(struct task_struct *p, u64 delta_exec);

#endif /* LHSCHED_H */
