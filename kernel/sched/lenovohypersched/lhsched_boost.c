// SPDX-License-Identifier: GPL-2.0
/*
 * Lenovo Hyper Scheduler Kernel Module - dynamic attribute switching
 *
 * Author: hyperscheduler@lenovo.com
 * Description: hyper schedule init
 */

#include "lhsched.h"


static void sleeping_reset_func(struct work_struct *work);



int get_max_hyper_boost(struct task_struct *p)
{
	struct lenovo_task_struct *lt;
	struct lenovo_dynamic_attribute *lda;
	u32 max_boost;
	int i;

	if (!p)
		return -ESRCH;

	lt = LENOVO_TASK(p);
	if (!lt || !lt->lda)
		return -ESRCH;
	lda = lt->lda;
	max_boost = lda->hyper_ux_boost[0];
	for (i = 1; i < 5; i++) {
		if (lda->hyper_ux_boost[i] > max_boost)
			max_boost = lda->hyper_ux_boost[i];
	}
	return (int)max_boost;
}

int backup_sched_attr(struct task_struct *p)
{
	struct lenovo_task_struct *lt;
	unsigned int cpu;

	if (!p)
		return -ESRCH;

	lt = LENOVO_TASK(p);
	if (!lt || !lt->lda)
		return -ESRCH;

	if (p->policy == SCHED_IDLE || p->policy == SCHED_DEADLINE)
		return -EINVAL;

	cpu = task_cpu(p);

	/* Do not switch the currently executing donor's scheduling class. */
	if (task_current_donor(cpu_rq(cpu), p))
		return -EINVAL;

	if (lt->lda->normal_hyper_ux_prio || lt->lda->dyn_switched_flag)
		return 0;

	lt->lda->saved_policy = p->policy;
	lt->lda->saved_prio = p->normal_prio;
	return 0;
}

static int restore_sched_attr_impl(struct task_struct *p)
{
	struct sched_attr attr = {0};
	struct lenovo_task_struct *lt;
	struct lenovo_dynamic_attribute *lda;
	u32 max_boost;
	int i;

	attr.sched_flags = (p->sched_reset_on_fork ? SCHED_FLAG_RESET_ON_FORK : 0);

	if (!p)
		return -ESRCH;

	lt = LENOVO_TASK(p);
	if (!lt || !lt->lda)
		return -ESRCH;

	lda = lt->lda;
	max_boost = lda->hyper_ux_boost[0];
	for (i = 1; i < 5; i++) {
		if (lda->hyper_ux_boost[i] > max_boost)
			max_boost = lda->hyper_ux_boost[i];
	}
	if (lda->normal_hyper_ux_prio > max_boost)
		max_boost = lda->normal_hyper_ux_prio;

	if (max_boost) {
		attr.sched_policy = SCHED_RR;
		attr.sched_priority = max_boost;
	} else {
		attr.sched_policy = lda->saved_policy;
		if (attr.sched_policy == SCHED_NORMAL || attr.sched_policy == SCHED_BATCH)
			attr.sched_nice = lda->saved_prio - 120;
		else if (attr.sched_policy <= SCHED_RR)
			attr.sched_priority = 99 - lda->saved_prio;
		else
			return -EINVAL;
	}

	return sched_setattr_nocheck(p, &attr);
}

int restore_sched_attr(struct task_struct *p)
{
	return restore_sched_attr_impl(p);
}


void dynamic_prio_clear(void *ltsk)
{
	struct lenovo_task_struct *lt = (struct lenovo_task_struct *)ltsk;

	lt->lda->saved_policy = 0xbbad;
	lt->lda->dyn_switched_flag = 0;
	lt->lda->normal_hyper_ux_prio = 0;
	memset(lt->lda->hyper_ux_boost, 0, 5);
}

int init_lenovo_dynamic_task_struct(void *ltsk)
{
	struct lenovo_task_struct *lt = (struct lenovo_task_struct *)ltsk;
	struct lenovo_dynamic_attribute *lda;

	if (!lt || lt->lda)
		return -EINVAL;

	lda = kmalloc(sizeof(struct lenovo_dynamic_attribute), GFP_ATOMIC);
	lt->lda = lda;
	if (!lda) {
		printk(KERN_ERR "hyper malloc dynamic task struct mem failed!\n");
		return -ENOMEM;
	}

	raw_spin_lock_init(&lda->lock);
	lda->saved_policy = 0xbbad;

	INIT_WORK(&lda->sleeping_reset_work, sleeping_reset_func);
	lda->ltsk = lt;

	lda->dyn_switched_flag = 0;
	lda->normal_hyper_ux_prio = 0;
	memset(lda->hyper_ux_boost, 0, sizeof(lda->hyper_ux_boost));

	return 0;
}

static void sleeping_reset_func(struct work_struct *work)
{
	struct lenovo_dynamic_attribute *lda =
		container_of(work, struct lenovo_dynamic_attribute, sleeping_reset_work);
	struct lenovo_task_struct *lt = lda->ltsk;
	struct task_struct *p = lenovo_task_owner(lt);

	raw_spin_lock(&lda->lock);
	if (lda->dyn_switched_flag & 0xb) {
		lda->dyn_switched_flag &= ~0xbULL;
		restore_sched_attr_impl(p);
	}
	raw_spin_unlock(&lda->lock);
	set_cgroup_hyper_prio(p);
	put_task_struct(p);
}

void queue_hyper_prio_work(struct task_struct *p)
{
	struct lenovo_dynamic_attribute *lda = LENOVO_TASK(p)->lda;

	if (!lda)
		return;

	/* Wakeup hooks hold p->pi_lock; sched_setattr_nocheck must run outside it. */
	get_task_struct(p);
	if (!schedule_work(&lda->sleeping_reset_work))
		put_task_struct(p);
}
