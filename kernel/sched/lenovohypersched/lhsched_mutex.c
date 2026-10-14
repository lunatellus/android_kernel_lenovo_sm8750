// SPDX-License-Identifier: GPL-2.0
/*
 * Lenovo Hyper Scheduler Kernel Module - mutex lock ordering helpers
 *
 * Author: hyperscheduler@lenovo.com
 * Description: hyper schedule init
 */

#include "lhsched.h"


/* Mutex / RWSEM lock ordering */
bool mutex_list_add(struct task_struct *task, struct mutex_waiter *waiter,
		    struct list_head *head, struct mutex *lock)
{
	struct mutex_waiter *cur;

	if (!task || !waiter || !head || !lock)
		return false;

	if (!test_task_hyper(task))
		return false;

	list_for_each_entry(cur, head, list) {
		if (!test_task_hyper(cur->task)) {
			list_add((struct list_head *)waiter, cur->list.prev);
			return true;
		}
	}

	list_add_tail((struct list_head *)waiter, head);
	return true;
}


void mutex_dynamic_hyper_enqueue(struct mutex *lock, struct task_struct *waiting)
{
	struct task_struct *owner;

	if (!lock || !test_task_hyper(waiting))
		return;

	owner = (struct task_struct *)(atomic_long_read(&lock->owner) & ~7ULL);
	if (test_dynamic_hyper(owner, 2) || !owner || test_task_hyper(owner))
		return;

	dynamic_hyper_enqueue(owner, 2, LENOVO_TASK(waiting)->lsa->hyper_ux_depth);
}


void mutex_dynamic_hyper_dequeue(struct mutex *lock, struct task_struct *task)
{
	unsigned long raw_owner;
	struct task_struct *owner;

	raw_owner = atomic_long_read(&lock->owner);
	if (!lock)
		return;

	owner = (struct task_struct *)(raw_owner & ~7ULL);
	if (test_dynamic_hyper(owner, 2))
		dynamic_hyper_dequeue(task, 2);
}
