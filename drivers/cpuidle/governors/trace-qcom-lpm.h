/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#if !defined(_TRACE_QCOM_LPM_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_QCOM_LPM_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM qcom_lpm

#include <linux/tracepoint.h>

TRACE_EVENT(lpm_gov_select,

	TP_PROTO(int idx, s64 qos, s64 sleep, u64 bias_ns, u64 reason),

	TP_ARGS(idx, qos, sleep, bias_ns, reason),

	TP_STRUCT__entry(
			 __field(int, idx)
			 __field(s64, qos)
			 __field(s64, sleep)
			 __field(u64, bias_ns)
			 __field(u64, reason)
	),

	TP_fast_assign(
		       __entry->idx = idx;
		       __entry->qos = qos;
		       __entry->sleep = sleep;
		       __entry->bias_ns = bias_ns;
		       __entry->reason = reason;
	),

	TP_printk("state:%d qos-ns:%lld sleep-ns:%lld bias-ns:%llu reason:%#llx",
		  __entry->idx, __entry->qos, __entry->sleep, __entry->bias_ns,
		  __entry->reason)
);

TRACE_EVENT(gov_pred_select,

	TP_PROTO(u32 predtype, u64 predicted, u64 tmr_time),

	TP_ARGS(predtype, predicted, tmr_time),

	TP_STRUCT__entry(
		__field(u32, predtype)
		__field(u64, predicted)
		__field(u64, tmr_time)
	),

	TP_fast_assign(
		__entry->predtype = predtype;
		__entry->predicted = predicted;
		__entry->tmr_time = tmr_time;
	),

	TP_printk("pred:%u time:%llu tmr-time:%llu",
		  __entry->predtype, __entry->predicted, __entry->tmr_time)
);

TRACE_EVENT(gov_pred_hist,

	TP_PROTO(int idx, s64 residency, int tmr),

	TP_ARGS(idx, residency, tmr),

	TP_STRUCT__entry(
			 __field(int, idx)
			 __field(s64, residency)
			 __field(int, tmr)
	),

	TP_fast_assign(
		       __entry->idx = idx;
		       __entry->residency = residency;
		       __entry->tmr = tmr;
	),

	TP_printk("idx:%d resi:%lld, tmr:%d",
		  __entry->idx, __entry->residency, __entry->tmr)
);

#endif /* _TRACE_QCOM_LPM_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace-qcom-lpm

#include <trace/define_trace.h>
