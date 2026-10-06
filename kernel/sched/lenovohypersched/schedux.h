#undef TRACE_SYSTEM
#define TRACE_SYSTEM schedux

#if !defined(_TRACE_SCHEDUX_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SCHEDUX_H

#include <linux/tracepoint.h>

/* Stock defines two independent events (no shared event class). */
TRACE_EVENT(dynamic_ux_set,
	TP_PROTO(struct task_struct *p, int type, int ux_state, s64 dynamic_ux, int depth),
	TP_ARGS(p, type, ux_state, dynamic_ux, depth),
	TP_STRUCT__entry(
		__field(int, pid)
		__array(char, comm, TASK_COMM_LEN)
		__field(int, type)
		__field(int, ux_state)
		__field(u64, dynamic_ux)
		__field(int, depth)
	),
	TP_fast_assign(
		__entry->pid = p->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->type = type;
		__entry->ux_state = ux_state;
		__entry->dynamic_ux = dynamic_ux;
		__entry->depth = depth;
	),
	TP_printk("pid=%d comm=%s dynamic_type=%d ux_state=%d dynamic_ux=%llx ux_depth=%d",
		__entry->pid, __entry->comm, __entry->type, __entry->ux_state, __entry->dynamic_ux, __entry->depth)
);

TRACE_EVENT(dynamic_ux_unset,
	TP_PROTO(struct task_struct *p, int type, int ux_state, s64 dynamic_ux, int depth),
	TP_ARGS(p, type, ux_state, dynamic_ux, depth),
	TP_STRUCT__entry(
		__field(int, pid)
		__array(char, comm, TASK_COMM_LEN)
		__field(int, type)
		__field(int, ux_state)
		__field(u64, dynamic_ux)
		__field(int, depth)
	),
	TP_fast_assign(
		__entry->pid = p->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->type = type;
		__entry->ux_state = ux_state;
		__entry->dynamic_ux = dynamic_ux;
		__entry->depth = depth;
	),
	TP_printk("pid=%d comm=%s dynamic_type=%d ux_state=%d dynamic_ux=%llx ux_depth=%d",
		__entry->pid, __entry->comm, __entry->type, __entry->ux_state, __entry->dynamic_ux, __entry->depth)
);

#endif /* _TRACE_SCHEDUX_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE schedux

#include <trace/define_trace.h>
