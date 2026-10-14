// SPDX-License-Identifier: GPL-2.0
/*
 * Lenovo Hyper Scheduler Kernel Module - RWSEM lock ordering helpers
 *
 * Author: hyperscheduler@lenovo.com
 * Description: hyper schedule init
 */

#include "lhsched.h"



bool rwsem_list_add(struct task_struct *task, struct rwsem_waiter *waiter,
		    struct list_head *head)
{
	struct rwsem_waiter *cur;
	bool hyper = test_task_hyper(task);

	if (!waiter || !head)
		return false;

	if (hyper_log_ctl & 8)
		printk(KERN_ERR "rwsem_list_add  task-id=%d\n", task->pid);

	if (hyper) {
		list_for_each_entry(cur, head, list) {
			if (!test_task_hyper(cur->task)) {
				list_add((struct list_head *)waiter, cur->list.prev);
				return true;
			}
		}
	}

	list_add_tail((struct list_head *)waiter, head);
	return true;
}


void rwsem_dynamic_hyper_enqueue(struct task_struct *trigger,
				struct task_struct *waiter_task,
				struct task_struct *owner,
				atomic_long_t *count)
{
	bool trigger_hyper = test_task_hyper(trigger);
	long old;

	if (hyper_log_ctl & 8)
		printk(KERN_ERR "rwsem_dynamic_hyper_ux_enqueuetask-id=%d\n", trigger->pid);

	if ((unsigned long)owner < 2 || !waiter_task || !trigger_hyper)
		return;

	if (test_task_hyper(owner) || !count || (atomic_long_read(count) & 0x80))
		return;

	if (owner->policy != SCHED_NORMAL && owner->policy != SCHED_BATCH)
		return;

	dynamic_hyper_enqueue(owner, 1, LENOVO_TASK(trigger)->lsa->hyper_ux_depth);

	old = atomic_long_read(count);
	while (!(old & 0x80)) {
		if (atomic_long_try_cmpxchg(count, &old, old | 0x80))
			break;
	}
}


void rwsem_dynamic_hyper_dequeue(atomic_long_t *count, struct task_struct *task)
{
	long old;

	if (hyper_log_ctl & 8)
		printk(KERN_ERR "rwsem_dynamic_hyper_ux_dequeue....task-id=%d\n", task->pid);

	if (!count || !task || !(atomic_long_read(count) & 0x80))
		return;

	dynamic_hyper_dequeue(task, 1);

	old = atomic_long_read(count);
	while (old & 0x80) {
		if (atomic_long_try_cmpxchg(count, &old, old & ~0x80L))
			break;
	}
}

