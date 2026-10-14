// SPDX-License-Identifier: GPL-2.0
/*
 * Lenovo Hyper Scheduler Kernel Module
 *
 * Author: hyperscheduler@lenovo.com
 * Description: hyper schedule init
 */


#include "lhsched.h"


#define CREATE_TRACE_POINTS
#include "schedux.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("hyperscheduler@lenovo.com");
MODULE_DESCRIPTION("hyper schedule init");



/* Global variables matching stock */
int hyper_max_dynamic_granularity = 32;
unsigned int hyper_min_migration_delay = 10;
unsigned int hyper_min_sched_delay_granularity;
bool hyper_module_modprobe_finish;
EXPORT_SYMBOL(hyper_module_modprobe_finish);

DEFINE_MUTEX(hyper_proc_node_mutex);
unsigned int sysctl_sched_migration_cost_lenovo;


/* Helper to check if entry is in list */
static inline bool entry_in_list(struct list_head *entry, struct list_head *head)
{
	struct list_head *cur;
	list_for_each(cur, head) {
		if (cur == entry)
			return true;
	}
	return false;
}

/*
 * Stock TU0 layout: the task/list helpers must be first referenced by
 * enqueue_hyper_thread / dequeue_hyper_thread (stock emits their outlined
 * bodies directly after those two functions), so dynamic_hyper_enqueue /
 * dynamic_hyper_dequeue reach them through these wrappers instead of calling
 * get_task_struct / list_add_tail / list_del_init / put_task_struct directly.
 * The wrappers are fully inlined into the dynamic_* call sites (they leave no
 * symbol, as in stock) while the helpers themselves stay outlined exactly
 * where stock has them.
 */
static inline void lhsched_task_ref_add(struct task_struct *p, struct list_head *entry, struct list_head *head);
static inline void lhsched_task_list_del(struct list_head *entry);
static inline void lhsched_task_put(struct task_struct *p);

/* Memory management for task extensions */
int init_lenovo_static_task_struct(void *ltsk)
{
	struct lenovo_task_struct *lt = (struct lenovo_task_struct *)ltsk;
	struct lenovo_static_attribute *lsa;

	lsa = kmalloc(sizeof(struct lenovo_static_attribute), GFP_ATOMIC);
	lt->lsa = lsa;
	if (!lsa)
		return -ENOMEM;

	lsa->static_hyper_ux = 0;
	lsa->hyper_ux_depth = 0;
	atomic64_set(&lsa->dynamic_hyper_ux, 0);
	INIT_LIST_HEAD(&lt->hyper_entry);
	lsa->enqueue_time = 0;
	lsa->dynamic_hyper_ux_start = 0;
	return 0;
}

noinline void init_lenovo_task_struct(struct task_struct *p)
{
	struct lenovo_task_struct *lt = p ? LENOVO_TASK(p) : NULL;
	struct lenovo_static_attribute *lsa;

	if (!p || !lt)
		return;

	if (!lt->lsa) {
		lsa = kzalloc(sizeof(*lsa), GFP_ATOMIC);
		lt->lsa = lsa;
		if (!lsa)
			printk(KERN_ERR "[T%d] init failed, malloc static hyper mem failed!!!\n", p->pid);
		else
			INIT_LIST_HEAD(&lt->hyper_entry);
	}

	if (!lt->lda && init_lenovo_dynamic_task_struct(lt) != 0) {
		printk(KERN_ERR "[T%d] init failed, malloc dynamic hyper mem failed!!!\n", p->pid);
	}
}

/* Testing task UX properties */
bool test_dynamic_hyper(struct task_struct *p, int type)
{
	struct lenovo_task_struct *lt;
	unsigned int shift;

	if (!p)
		return false;

	lt = LENOVO_TASK(p);
	if (!lt->lsa)
		return false;

	shift = (u32)type << 3;
	return ((atomic64_read(&lt->lsa->dynamic_hyper_ux) &
		 (0xffULL << shift)) >> shift) != 0;
}

/* Dynamic queue operations */
void dynamic_hyper_enqueue(struct task_struct *p, int type, int incoming_depth)
{
	struct rq_flags rf = {0};
	struct rq *rq;
	struct lenovo_task_struct *lt;
	struct lenovo_static_attribute *s;
	unsigned int shift;
	int next;

	if (!p || type > 4)
		return;

	lt = LENOVO_TASK(p);
	if (!lt->lsa)
		return;

	rq = task_rq_lock(p, &rf);
	if ((p->policy != SCHED_NORMAL && p->policy != SCHED_BATCH) ||
	    !list_empty(&lt->hyper_entry))
		goto unlock;

	s = lt->lsa;
	shift = (u32)type << 3;
	atomic64_add(1ULL << shift, &s->dynamic_hyper_ux);
	s->dynamic_hyper_ux_start = (u64)(u32)jiffies_to_usecs(jiffies) * 1000;

	next = incoming_depth + 1;
	if (next > s->hyper_ux_depth)
		s->hyper_ux_depth = next;

	if (p->__state != 0)
		goto unlock;

	if (entry_in_list(&lt->hyper_entry, &LENOVO_RQ(rq)->hyper_ux_thread_list))
		goto unlock;

	lhsched_task_ref_add(p, &lt->hyper_entry, &LENOVO_RQ(rq)->hyper_ux_thread_list);
	LENOVO_RQ(rq)->num_hyper_ux_tasks++;
	if (hyper_log_ctl & 2)
		printk(KERN_ERR "dynamic hyper task enqueue task-id=%d\n", p->pid);

	trace_dynamic_ux_set(p, type, p->__state,
			     atomic64_read(&s->dynamic_hyper_ux),
			     s->hyper_ux_depth);

unlock:
	task_rq_unlock(rq, p, &rf);
}

bool test_task_hyper(struct task_struct *p)
{
	struct lenovo_task_struct *lt;

	if (!p)
		return false;

	lt = LENOVO_TASK(p);
	if (!lt->lsa)
		return false;

	if (lt->lsa->static_hyper_ux == 0 &&
	    atomic64_read(&lt->lsa->dynamic_hyper_ux) == 0)
		return false;

	return p->nr_cpus_allowed > 1;
}


void dynamic_hyper_dequeue(struct task_struct *p, int type)
{
	struct rq_flags rf = {0};
	struct rq *rq;
	struct lenovo_task_struct *lt;
	struct lenovo_static_attribute *s;
	unsigned int shift;

	if (!p || type > 4)
		return;

	lt = LENOVO_TASK(p);
	if (!lt->lsa)
		return;

	rq = task_rq_lock(p, &rf);
	s = lt->lsa;

	if (!atomic64_read(&s->dynamic_hyper_ux))
		goto unlock;

	shift = (u32)type << 3;
	atomic64_sub(1ULL << shift, &s->dynamic_hyper_ux);

	if (atomic64_read(&s->dynamic_hyper_ux))
		goto unlock;

	lt->lsa->hyper_ux_depth = 0;

	if (!entry_in_list(&lt->hyper_entry, &LENOVO_RQ(rq)->hyper_ux_thread_list))
		goto unlock;

	if (hyper_log_ctl & 2)
		printk(KERN_ERR "dynamic hyper task dequeue....task-id=%d\n", p->pid);

	lhsched_task_list_del(&lt->hyper_entry);
	LENOVO_RQ(rq)->num_hyper_ux_tasks--;

	trace_dynamic_ux_unset(p, type, p->__state, 0, lt->lsa->hyper_ux_depth);
	lhsched_task_put(p);

unlock:
	task_rq_unlock(rq, p, &rf);
}


/* Runqueue enqueue / dequeue hooks */
void enqueue_hyper_thread(struct rq *rq, struct task_struct *p)
{
	struct lenovo_task_struct *lt = p ? LENOVO_TASK(p) : NULL;
	int leader_static_ux = 0;

	if (!rq || !lt || !lt->lsa || (p->flags & PF_EXITING))
		return;

	if (!list_empty(&lt->hyper_entry))
		return;

	if (LENOVO_RQ(rq)->hyper_ux_throttled)
		return;

	if (p->group_leader && LENOVO_TASK(p->group_leader) &&
	    LENOVO_TASK(p->group_leader)->lsa)
		leader_static_ux = LENOVO_TASK(p->group_leader)->lsa->static_hyper_ux;

	lt->lsa->enqueue_time = rq->clock;

	if (lt->lsa && (lt->lsa->static_hyper_ux != 0 ||
			atomic64_read(&lt->lsa->dynamic_hyper_ux) != 0 ||
			leader_static_ux == 5)) {
		if (hyper_log_ctl & 1)
			printk(KERN_ERR "static_hyper_ux=1 task pid =%d\n", p->pid);

		if (!entry_in_list(&lt->hyper_entry, &LENOVO_RQ(rq)->hyper_ux_thread_list)) {
			get_task_struct(p);
			list_add_tail(&lt->hyper_entry, &LENOVO_RQ(rq)->hyper_ux_thread_list);
			LENOVO_RQ(rq)->num_hyper_ux_tasks++;

			if (hyper_log_ctl & 1)
				printk(KERN_ERR "enqueue static hyper task pid = %d\n", p->pid);
		}
	}
}

void dequeue_hyper_thread(struct rq *rq, struct task_struct *p)
{
	u64 now_usecs;
	struct lenovo_task_struct *lt;
	struct lenovo_static_attribute *s;

	now_usecs = jiffies_to_usecs(jiffies);
	lt = p ? LENOVO_TASK(p) : NULL;
	if (!rq || !lt || !lt->lsa)
		return;

	s = lt->lsa;
	s->enqueue_time = 0;

	if (list_empty(&lt->hyper_entry))
		return;

	if (atomic64_read(&s->dynamic_hyper_ux)) {
		u64 elapsed = (u64)(u32)now_usecs * 1000 - s->dynamic_hyper_ux_start;
		u64 limit = (s64)hyper_max_dynamic_granularity * 1000000;
		if (elapsed > limit)
			atomic64_set(&s->dynamic_hyper_ux, 0);
	}

	if (entry_in_list(&lt->hyper_entry, &LENOVO_RQ(rq)->hyper_ux_thread_list)) {
		s->hyper_ux_depth = 0;
		list_del_init(&lt->hyper_entry);
		LENOVO_RQ(rq)->num_hyper_ux_tasks--;
		put_task_struct(p);

		if (hyper_log_ctl & 1)
			printk(KERN_ERR "dequeue static hyper task pid = %d\n", p->pid);
	}
}

void sched_enqueue_hyper_thread(struct rq *rq, struct task_struct *p)
{
	enqueue_hyper_thread(rq, p);
}

/*
 * Defined after enqueue_hyper_thread/dequeue_hyper_thread on purpose: their
 * bodies are emitted here, so get_task_struct/list_add_tail and
 * list_del_init/put_task_struct get their TU-local outlines at stock's
 * position (right after enqueue/dequeue), not at the earlier
 * dynamic_hyper_* call sites.
 */
static inline void lhsched_task_ref_add(struct task_struct *p, struct list_head *entry, struct list_head *head)
{
	get_task_struct(p);
	list_add_tail(entry, head);
}

static inline void lhsched_task_list_del(struct list_head *entry)
{
	list_del_init(entry);
}

static inline void lhsched_task_put(struct task_struct *p)
{
	put_task_struct(p);
}

void sched_dequeue_hyper_thread(struct rq *rq, struct task_struct *p)
{
	dequeue_hyper_thread(rq, p);
}

/* Candidate selection */
static noinline struct task_struct *pick_first_hyper_thread(struct rq *rq)
{
	struct lenovo_task_struct *lt, *n;
	struct task_struct *priority_task = NULL;
	struct task_struct *earliest_task = NULL;
	unsigned int valid = 0;

	list_for_each_entry_safe(lt, n, &LENOVO_RQ(rq)->hyper_ux_thread_list, hyper_entry) {
		struct task_struct *p = lenovo_task_owner(lt);

		if (unlikely(task_cpu(p) != rq->cpu || !task_on_rq_queued(p) ||
			     p->se.sched_delayed)) {
			if (hyper_log_ctl & 1)
				printk(KERN_WARNING "task(%s,%d,%d) does not belong to cpu%d",
				       p->comm, task_cpu(p), p->policy, rq->cpu);
			list_del_init(&lt->hyper_entry);
			LENOVO_RQ(rq)->num_hyper_ux_tasks--;
			put_task_struct(p);
			continue;
		}

		if (!priority_task ||
		    lt->lsa->static_hyper_ux > LENOVO_TASK(priority_task)->lsa->static_hyper_ux ||
		    (lt->lsa->static_hyper_ux == LENOVO_TASK(priority_task)->lsa->static_hyper_ux &&
		     (s64)(p->se.vruntime - priority_task->se.vruntime) < 0)) {
			priority_task = p;
		}

		if (!earliest_task ||
		    (s64)(p->se.exec_start - earliest_task->se.exec_start) < 0) {
			earliest_task = p;
		}

		if (++valid >= 20)
			break;
	}

	if (earliest_task && earliest_task != priority_task) {
		assert_clock_updated(rq);
		if (rq->clock_task - earliest_task->se.exec_start > 16000000ULL)
			return earliest_task;
	}

	return priority_task;
}

noinline void pick_hyper_thread(struct rq *rq, struct task_struct **p)
{
	struct task_struct *cand;

	if (!rq || !p ||
	    LENOVO_RQ(rq)->hyper_ux_throttled ||
	    list_empty(&LENOVO_RQ(rq)->hyper_ux_thread_list))
		return;

	cand = pick_first_hyper_thread(rq);
	if (!cand || !task_on_rq_queued(cand) || cand->se.sched_delayed ||
	    task_cpu(cand) != rq->cpu ||
	    (cand->on_cpu && cand != rq->donor))
		return;

	/* ACK installs the selected entity and its cgroup hierarchy after the hook. */
	*p = cand;
}

void android_rvh_replace_next_task_fair_handler(void *unused, struct rq *rq,
						struct task_struct **p,
						struct task_struct *prev)
{
	pick_hyper_thread(rq, p);
}
EXPORT_SYMBOL(android_rvh_replace_next_task_fair_handler);

bool lenovo_hyper_task_selected(struct rq *rq, struct task_struct *p)
{
	return p && !LENOVO_RQ(rq)->hyper_ux_throttled &&
		LENOVO_TASK(p)->lsa && !list_empty(&LENOVO_TASK(p)->hyper_entry) &&
		task_cpu(p) == cpu_of(rq) && task_on_rq_queued(p) &&
		!p->se.sched_delayed;
}
EXPORT_SYMBOL_GPL(lenovo_hyper_task_selected);

void probe_android_vh_dup_task_struct(void *unused, struct task_struct *tsk, struct task_struct *orig)
{
	/* dup_task_struct copied the parent's OEM pointers and list links. */
	memset(LENOVO_TASK(tsk), 0, sizeof(struct lenovo_task_struct));
	init_lenovo_task_struct(tsk);
}

void probe_android_vh_alter_mutex_list_add(void *unused, struct mutex *lock,
					   struct mutex_waiter *waiter,
					   struct list_head *list,
					   bool *already_on_list)
{
	if (*already_on_list)
		return;

	if (!lock || !waiter || !list)
		return;

	*already_on_list = mutex_list_add(current, waiter, list, lock);
}

void probe_android_vh_mutex_wait_start(void *unused, struct mutex *lock)
{
	mutex_dynamic_hyper_enqueue(lock, current);
}

void probe_android_vh_mutex_unlock_slowpath(void *unused, struct mutex *lock)
{
	mutex_dynamic_hyper_dequeue(lock, current);
}

void probe_android_vh_alter_rwsem_list_add(void *unused, struct rwsem_waiter *waiter,
					   struct rw_semaphore *sem,
					   bool *already_on_list)
{
	if (*already_on_list)
		return;

	*already_on_list = rwsem_list_add(current, waiter, &sem->wait_list);
}

void probe_android_vh_rwsem_wake(void *unused, struct rw_semaphore *sem)
{
	unsigned long raw_owner;
	struct task_struct *owner;

	raw_owner = (unsigned long)atomic_long_read(&sem->owner);
	owner = (struct task_struct *)(raw_owner & ~3UL);
	if (!owner || (raw_owner & 3UL))
		return;

	if (test_ti_thread_flag(task_thread_info(current), 1))
		return;

	rwsem_dynamic_hyper_enqueue(current, current, owner, &sem->count);
}

void probe_android_vh_rwsem_wake_finish(void *unused, struct rw_semaphore *sem)
{
	rwsem_dynamic_hyper_dequeue(&sem->count, current);
}

void probe_android_vh_free_task(void *unused, struct task_struct *p)
{
	struct lenovo_task_struct *lt;

	if (!p || !LENOVO_TASK(p))
		return;

	lt = LENOVO_TASK(p);
	if (lt->lsa) {
		kfree(lt->lsa);
		lt->lsa = NULL;
	}
	if (lt->lda) {
		kfree(lt->lda);
		lt->lda = NULL;
	}
}

int register_lenovo_extra_vendor_hook(void)
{
	int ret = register_trace_android_rvh_replace_next_task_fair(
			android_rvh_replace_next_task_fair_handler, NULL);
	if (ret)
		printk(KERN_ERR "failed to register_trace_android_rvh_replace_next_task_fair, ret=%d\n", ret);
	return ret;
}


void probe_android_rvh_wake_up_new_task(void *unused, struct task_struct *p)
{
	queue_hyper_prio_work(p);
}

void probe_android_rvh_try_to_wake_up(void *unused, struct task_struct *p)
{
	queue_hyper_prio_work(p);
}

void probe_android_vh_sched_stat_runtime(void *unused, struct task_struct *tsk, u64 runtime)
{
	sched_hyper_stat_runtime_handle(tsk, runtime);
}

void init_lenovo_rq_data(void *rq_data)
{
	struct lenovo_rq *lrq = (struct lenovo_rq *)rq_data;

	if (!lrq)
		return;

	INIT_LIST_HEAD(&lrq->hyper_ux_thread_list);
	lrq->num_hyper_ux_tasks = 0;
	lrq->hyper_ux_throttled = 0;
	lrq->hyper_ux_time = 0;
	lrq->hyper_ux_runtime = def_ux_bandwidth.hyper_ux_runtime;
	raw_spin_lock_init(&lrq->hyper_ux_runtime_lock);
}

static void probe_android_rvh_enqueue_task_fair(void *unused, struct rq *rq, struct task_struct *p, int flags)
{
	enqueue_hyper_thread(rq, p);
}

static void probe_android_rvh_dequeue_task_fair(void *unused, struct rq *rq, struct task_struct *p, int flags)
{
	dequeue_hyper_thread(rq, p);
}

/* Module init and cleanup */
static int __init init_lenovo_hypersched(void)
{
	static int already_inited;
	int cpu;
	struct task_struct *g, *t;
	int ret;

	if (cmpxchg(&already_inited, 0, 1) != 0)
		return 0;

	BUILD_BUG_ON(sizeof(struct lenovo_task_struct) >
		     sizeof(((struct task_struct *)0)->android_oem_data1));
	BUILD_BUG_ON(sizeof(struct lenovo_rq) >
		     sizeof(((struct rq *)0)->android_oem_data1));

	init_lenovo_rq_bandwidth(&def_ux_bandwidth, 1000000000ULL, 200000000ULL);

	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);
		if (!rq) {
			printk("failed to init lenovo rq(%d)", cpu);
			continue;
		}
		init_lenovo_rq_data(LENOVO_RQ(rq));
	}

	printk("init all already exists thread\n");
	if (register_trace_android_vh_dup_task_struct(probe_android_vh_dup_task_struct, NULL))
		goto fail_hooks;
	if (register_trace_android_vh_free_task(probe_android_vh_free_task, NULL))
		goto fail_hooks;
	read_lock(&tasklist_lock);
	for_each_process_thread(g, t) {
		init_lenovo_task_struct(t);
	}
	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);
		init_lenovo_task_struct(rq->idle);
	}
	read_unlock(&tasklist_lock);

	ret = lenovo_hypernode_proc_init();
	if (ret) {
		/* stock: log, then report hook failure without registering any hook */
		printk(KERN_ERR "create hyperschedule proc node failed\n");
		goto fail_hooks;
	}

	if (register_trace_android_rvh_enqueue_task_fair(probe_android_rvh_enqueue_task_fair, NULL))
		goto fail_hooks;
	if (register_trace_android_rvh_dequeue_task_fair(probe_android_rvh_dequeue_task_fair, NULL))
		goto fail_hooks;
	if (register_trace_android_vh_mutex_wait_start(probe_android_vh_mutex_wait_start, NULL))
		goto fail_hooks;
	if (register_trace_android_vh_alter_mutex_list_add(probe_android_vh_alter_mutex_list_add, NULL))
		goto fail_hooks;
	if (register_trace_android_vh_mutex_unlock_slowpath(probe_android_vh_mutex_unlock_slowpath, NULL))
		goto fail_hooks;
	if (register_trace_android_vh_alter_rwsem_list_add(probe_android_vh_alter_rwsem_list_add, NULL))
		goto fail_hooks;
	if (register_trace_android_vh_rwsem_wake(probe_android_vh_rwsem_wake, NULL))
		goto fail_hooks;
	if (register_trace_android_vh_rwsem_wake_finish(probe_android_vh_rwsem_wake_finish, NULL))
		goto fail_hooks;
	if (register_trace_android_rvh_wake_up_new_task(probe_android_rvh_wake_up_new_task, NULL))
		goto fail_hooks;
	if (register_trace_android_rvh_try_to_wake_up(probe_android_rvh_try_to_wake_up, NULL))
		goto fail_hooks;
	if (register_trace_sched_stat_runtime(probe_android_vh_sched_stat_runtime, NULL))
		goto fail_hooks;

	if (register_lenovo_extra_vendor_hook())
		goto fail_hooks;

	WRITE_ONCE(hyper_module_modprobe_finish, true);
	printk(KERN_ERR "register lenovo tubro schedule hooks succ\n");
	return 0;

fail_hooks:
	printk(KERN_ERR "register lenovo tubro schedule hooks failed\n");
	return 0;
}
module_init(init_lenovo_hypersched);


