/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#if !defined(_TRACE_CLUSTER_LPM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CLUSTER_LPM_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM cluster_lpm

#include <linux/tracepoint.h>

TRACE_EVENT(cluster_pred_select,

	TP_PROTO(int index, s64 next_wakeup, int restrict_idx, int pred, s64 pred_ns),

	TP_ARGS(index, next_wakeup, restrict_idx, pred, pred_ns),

	TP_STRUCT__entry(
		__field(int, index)
		__field(s64, next_wakeup)
		__field(int, restrict_idx)
		__field(int, pred)
		__field(s64, pred_ns)
	),

	TP_fast_assign(
		__entry->index = index;
		__entry->next_wakeup = next_wakeup;
		__entry->restrict_idx = restrict_idx;
		__entry->pred = pred;
		__entry->pred_ns = pred_ns;
	),

	TP_printk("state:%d next-wakeup:%lld restrict-idx:%d pred:%d time:%lld",
		   __entry->index, __entry->next_wakeup, __entry->restrict_idx,
		   __entry->pred, __entry->pred_ns)
);

TRACE_EVENT(cluster_pred_hist,

	TP_PROTO(int idx, s64 resi, int sample, u32 tmr),

	TP_ARGS(idx, resi, sample, tmr),

	TP_STRUCT__entry(
		__field(int, idx)
		__field(s64, resi)
		__field(int, sample)
		__field(u32, tmr)
	),

	TP_fast_assign(
		__entry->idx = idx;
		__entry->resi = resi;
		__entry->sample = sample;
		__entry->tmr = tmr;
	),

	TP_printk("idx:%d resi:%lld sample:%d tmr:%u",  __entry->idx,
		  __entry->resi, __entry->sample, __entry->tmr)
);

TRACE_EVENT(cluster_exit,

	TP_PROTO(int cpu, u32 idx, u32 suspend_param),

	TP_ARGS(cpu, idx, suspend_param),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(u32, idx)
		__field(u32, suspend_param)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->idx = idx;
		__entry->suspend_param = suspend_param;
	),

	TP_printk("first cpu:%d idx:%u suspend_param:0x%x", __entry->cpu,
		  __entry->idx, __entry->suspend_param)
);

TRACE_EVENT(cluster_enter,

	TP_PROTO(int cpu, u32 idx, u32 suspend_param),

	TP_ARGS(cpu, idx, suspend_param),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(u32, idx)
		__field(u32, suspend_param)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->idx = idx;
		__entry->suspend_param = suspend_param;
	),

	TP_printk("last cpu:%d idx:%u suspend_param:0x%x", __entry->cpu,
		  __entry->idx, __entry->suspend_param)
);

#endif /* _TRACE_QCOM_LPM_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace-cluster-lpm

#include <trace/define_trace.h>
