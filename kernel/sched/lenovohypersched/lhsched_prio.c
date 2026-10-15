// SPDX-License-Identifier: GPL-2.0
/*
 * Lenovo Hyper Scheduler Kernel Module - priority boost helpers
 *
 * Author: hyperscheduler@lenovo.com
 * Description: hyper schedule init
 */

#include "lhsched.h"


static int sched_attr_set_hyper_prio(struct task_struct *p, u32 prio);



int set_hyper_prio(struct task_struct *p, u32 prio)
{
	struct lenovo_task_struct *lt;
	struct lenovo_dynamic_attribute *lda;
	unsigned long flags;
	int ret = 0;

	if (!p)
		return -EINVAL;

	if (prio > 10)
		prio = 10;

	lt = LENOVO_TASK(p);
	if (!lt->lda)
		return -ESRCH;

	lda = lt->lda;
	if (lda->normal_hyper_ux_prio == prio)
		return 0;

	raw_spin_lock_irqsave(&lda->lock, flags);

	if (prio) {
		if (!lda->normal_hyper_ux_prio && backup_sched_attr(p) != 0) {
			raw_spin_unlock_irqrestore(&lda->lock, flags);
			return 0;
		}
		lda->normal_hyper_ux_prio = prio;
		ret = sched_attr_set_hyper_prio(p, prio);
	} else {
		if (!lda->normal_hyper_ux_prio) {
			raw_spin_unlock_irqrestore(&lda->lock, flags);
			return 0;
		}
		lda->normal_hyper_ux_prio = 0;
		ret = restore_sched_attr(p);
	}

	raw_spin_unlock_irqrestore(&lda->lock, flags);

	if (ret)
		printk(KERN_ERR "error setting hyper. ret=%d task=%s(%d) prio=%u",
		       ret, p->comm, p->pid, prio);

	return ret;
}

static inline u32 effective_hyper_prio(struct task_struct *p)
{
	return (p->prio >= 89 && p->prio < 99) ? (99 - p->prio) : 0;
}

static int sched_attr_set_hyper_prio(struct task_struct *p, u32 prio)
{
	struct sched_attr attr = {0};

	attr.sched_policy = SCHED_RR;
	attr.sched_priority = clamp_t(u32, prio, 1U, 10U);
	attr.sched_flags = (p->sched_reset_on_fork ? SCHED_FLAG_RESET_ON_FORK : 0);
	return sched_setattr_nocheck(p, &attr);
}

void clear_hyper_boost(struct task_struct *p, struct lenovo_dynamic_attribute *lda,
		       u32 type)
{
	unsigned long flags;
	int remaining;
	u32 wanted;

	if (!p || !lda || type > 4 || !lda->hyper_ux_boost[type])
		return;

	raw_spin_lock_irqsave(&lda->lock, flags);

	lda->hyper_ux_boost[type] = 0;
	remaining = get_max_hyper_boost(p);
	if (!remaining) {
		lda->dyn_switched_flag &= ~4ULL;
	}

	wanted = max((u32)remaining, (u32)lda->normal_hyper_ux_prio);
	if (wanted != effective_hyper_prio(p)) {
		if (type == 0 && p->on_rq)
			lda->dyn_switched_flag |= 8;
		else
			restore_sched_attr(p);
	}

	raw_spin_unlock_irqrestore(&lda->lock, flags);
}

void request_hyper_boost(struct task_struct *p, struct lenovo_dynamic_attribute *lda,
			 u32 prio, u32 type)
{
	unsigned long flags;

	if (prio < 1 || prio > 10 || !p || !lda || type > 4)
		return;

	if (lda->hyper_ux_boost[type] >= prio || p->rt_priority > 10)
		return;

	raw_spin_lock_irqsave(&lda->lock, flags);

	if (p->rt_priority > 10 || (lda->dyn_switched_flag & 2) || backup_sched_attr(p))
		goto out;

	lda->hyper_ux_boost[type] = (u8)prio;
	lda->dyn_switched_flag |= 4;

	if (effective_hyper_prio(p) >= prio)
		goto out;


	sched_attr_set_hyper_prio(p, prio);

out:
	raw_spin_unlock_irqrestore(&lda->lock, flags);
}

/* Lenovo cpu-cgroup priority, kept in reserved task_group slot 1
 * (6.12 spells the padding __kabi_reserved1, 6.6 android_kabi_reserved1).
 */
u32 cgroup_hyper_prio(struct task_struct *p)
{
	struct cgroup_subsys_state *css;
	u32 prio = 0;

	rcu_read_lock();
	css = task_css(p, cpu_cgrp_id);
	if (css)
		prio = (u32)css_tg(css)->__kabi_reserved1;
	rcu_read_unlock();
	return prio;
}

void set_cgroup_hyper_prio(struct task_struct *p)
{
	u32 prio;
	struct lenovo_task_struct *lt;
	struct lenovo_dynamic_attribute *lda;

	if (!p)
		return;

	prio = cgroup_hyper_prio(p);

	lt = LENOVO_TASK(p);
	if (!lt)
		return;

	lda = lt->lda;
	if (!lda || prio == lda->hyper_ux_boost[4])
		return;

	if (prio < lda->hyper_ux_boost[4])
		clear_hyper_boost(p, lda, 4);

	request_hyper_boost(p, lda, prio, 4);
}
