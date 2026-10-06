// SPDX-License-Identifier: GPL-2.0
/*
 * Lenovo Hyper Scheduler Kernel Module - bandwidth and runtime accounting
 *
 * Author: hyperscheduler@lenovo.com
 * Description: hyper schedule init
 */

#include "lhsched.h"


static enum hrtimer_restart sched_hyper_ux_period_timer(struct hrtimer *timer);

struct lenovo_ux_bandwidth def_ux_bandwidth;


/* Bandwidth and Runtime accounting */
void init_lenovo_rq_bandwidth(struct lenovo_ux_bandwidth *b, u64 period, u64 runtime)
{
	b->hyper_ux_period = period;
	b->hyper_ux_runtime = runtime;
	b->hyper_ux_period_active = 0;
	b->hyper_ux_max_runtime = period - (period / 5);
	raw_spin_lock_init(&b->hyper_ux_runtime_lock);
	hrtimer_init(&b->hyper_ux_period_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_HARD);
	b->hyper_ux_period_timer.function = sched_hyper_ux_period_timer;
}


static enum hrtimer_restart sched_hyper_ux_period_timer(struct hrtimer *timer)
{
	struct lenovo_ux_bandwidth *b = container_of(timer, struct lenovo_ux_bandwidth, hyper_ux_period_timer);
	s32 overruns;
	bool idle = true;

	raw_spin_lock(&b->hyper_ux_runtime_lock);
	overruns = hrtimer_forward_now(timer, ns_to_ktime(b->hyper_ux_period));
	if (!overruns) {
		raw_spin_unlock(&b->hyper_ux_runtime_lock);
		return HRTIMER_RESTART;
	}

	do {
		int cpu;
		bool throttled = false;

		raw_spin_unlock(&b->hyper_ux_runtime_lock);
		idle = true;
		for_each_online_cpu(cpu) {
			struct rq_flags rf;
			struct rq *rq = cpu_rq(cpu);
			struct lenovo_rq *lrq = LENOVO_RQ(rq);
			u64 old_runtime, before, credit;

			if (cpu >= 32 || !lrq)
				continue;

			raw_spin_lock(&lrq->hyper_ux_runtime_lock);
			old_runtime = lrq->hyper_ux_runtime;
			if (old_runtime != U64_MAX)
				lrq->hyper_ux_runtime = b->hyper_ux_runtime;
			before = lrq->hyper_ux_time;
			raw_spin_unlock(&lrq->hyper_ux_runtime_lock);
			if (!before)
				continue;

			rq_lock(rq, &rf);
			update_rq_clock(rq);
			if (lrq->hyper_ux_time) {
				raw_spin_lock(&lrq->hyper_ux_runtime_lock);
				credit = old_runtime * (s64)overruns;
				before = lrq->hyper_ux_time;
				lrq->hyper_ux_time = before < credit ? 0 : before - credit;
				if (lrq->hyper_ux_throttled && lrq->hyper_ux_time < old_runtime)
					lrq->hyper_ux_throttled = 0;
				if (before > credit || lrq->num_hyper_ux_tasks)
					idle = false;
				raw_spin_unlock(&lrq->hyper_ux_runtime_lock);
			} else if (lrq->num_hyper_ux_tasks) {
				idle = false;
			}
			if (lrq->hyper_ux_throttled)
				throttled = true;
			rq_unlock(rq, &rf);
		}

		if (!throttled && b->hyper_ux_runtime == U64_MAX)
			idle = true;
		raw_spin_lock(&b->hyper_ux_runtime_lock);
		overruns = hrtimer_forward_now(timer, ns_to_ktime(b->hyper_ux_period));
	} while (overruns);

	if (idle)
		b->hyper_ux_period_active = 0;
	raw_spin_unlock(&b->hyper_ux_runtime_lock);
	return idle ? HRTIMER_NORESTART : HRTIMER_RESTART;
}


void sched_hyper_stat_runtime_handle(struct task_struct *p, u64 delta_exec)
{
	{
	unsigned int cpu = task_cpu(p);
	struct lenovo_task_struct *lt = p ? LENOVO_TASK(p) : NULL;
	struct rq *rq = cpu_rq(cpu);
	struct lenovo_rq *lrq = LENOVO_RQ(rq);

	if (!p || !rq || !lrq || !lt || !lt->lsa)
		return;

	if (lt->lsa->static_hyper_ux == 0 &&
	    atomic64_read(&lt->lsa->dynamic_hyper_ux) == 0)
		return;

	if (list_empty(&lrq->hyper_ux_thread_list))
		return;

	if (lrq->hyper_ux_runtime == U64_MAX)
		return;

	raw_spin_lock(&lrq->hyper_ux_runtime_lock);
	lrq->hyper_ux_time += delta_exec;

	if (lrq->hyper_ux_throttled) {
		resched_curr(rq);
		raw_spin_unlock(&lrq->hyper_ux_runtime_lock);
		goto check_timer;
	}

	if (def_ux_bandwidth.hyper_ux_runtime < def_ux_bandwidth.hyper_ux_period) {
		if (lrq->hyper_ux_time > lrq->hyper_ux_runtime &&
		    lrq->hyper_ux_runtime < def_ux_bandwidth.hyper_ux_max_runtime) {
			int other_cpu, best_cpu = -1;
			s64 max_slack = 0;

			raw_spin_unlock(&lrq->hyper_ux_runtime_lock);

			for_each_online_cpu(other_cpu) {
				struct rq *orq;
				struct lenovo_rq *olrq;

				if (other_cpu == cpu || other_cpu >= 32)
					continue;

				orq = cpu_rq(other_cpu);
				olrq = LENOVO_RQ(orq);

				raw_spin_lock(&olrq->hyper_ux_runtime_lock);
				if (olrq->hyper_ux_runtime != U64_MAX) {
					s64 slack = (s64)(olrq->hyper_ux_runtime - olrq->hyper_ux_time);
					if (slack > max_slack) {
						max_slack = slack;
						best_cpu = other_cpu;
					}
				}
				raw_spin_unlock(&olrq->hyper_ux_runtime_lock);
			}

			raw_spin_lock(&lrq->hyper_ux_runtime_lock);
			if (best_cpu >= 0 && max_slack >= 12000001LL) {
				struct lenovo_rq *donor = LENOVO_RQ(cpu_rq(best_cpu));
				raw_spin_lock(&donor->hyper_ux_runtime_lock);
				{
					u64 amount = (u64)(max_slack >> 1);
					if (lrq->hyper_ux_runtime + amount > def_ux_bandwidth.hyper_ux_max_runtime)
						amount = def_ux_bandwidth.hyper_ux_max_runtime - lrq->hyper_ux_runtime;
					if (lrq->hyper_ux_runtime + amount > lrq->hyper_ux_time) {
						donor->hyper_ux_runtime -= amount;
						lrq->hyper_ux_runtime += amount;
					}
				}
				raw_spin_unlock(&donor->hyper_ux_runtime_lock);
			}
		}

		if (lrq->hyper_ux_runtime != U64_MAX &&
		    lrq->hyper_ux_time > lrq->hyper_ux_runtime) {
			if (def_ux_bandwidth.hyper_ux_runtime != 0) {
				lrq->hyper_ux_throttled = 1;
				resched_curr(rq);
				raw_spin_unlock(&lrq->hyper_ux_runtime_lock);
				goto check_timer;
			} else {
				bool was_throttled = lrq->hyper_ux_throttled;
				lrq->hyper_ux_time = 0;
				if (was_throttled) {
					resched_curr(rq);
					raw_spin_unlock(&lrq->hyper_ux_runtime_lock);
					goto check_timer;
				}
			}
		}
	}

	raw_spin_unlock(&lrq->hyper_ux_runtime_lock);
	return;

check_timer:
	if (def_ux_bandwidth.hyper_ux_runtime != U64_MAX) {
		raw_spin_lock(&def_ux_bandwidth.hyper_ux_runtime_lock);
		if (!def_ux_bandwidth.hyper_ux_period_active) {
			def_ux_bandwidth.hyper_ux_period_active = 1;
			hrtimer_forward_now(&def_ux_bandwidth.hyper_ux_period_timer,
					    ns_to_ktime(def_ux_bandwidth.hyper_ux_period));
			hrtimer_start_expires(&def_ux_bandwidth.hyper_ux_period_timer,
					      HRTIMER_MODE_ABS_PINNED_HARD);
		}
		raw_spin_unlock(&def_ux_bandwidth.hyper_ux_runtime_lock);
	}
	}
}

