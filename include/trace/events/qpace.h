/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM qpace

#if !defined(_TRACE_QPACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_QPACE_H
#include <linux/tracepoint.h>

/*
 * start_qpace_urgent_decompress - called when qpace_urgent_decompress trigger
 * @src: Input address of the compressed page
 * @dst: Address to output the decompressed page to
 * @size: size of the compressed input-page
 *
 */
TRACE_EVENT(start_qpace_urgent_decompress,

	 TP_PROTO(void *src, void *dst, unsigned int size),
	 TP_ARGS(src, dst, size),

	 TP_STRUCT__entry(
		 __field(void *, src)
		 __field(void *, dst)
		 __field(unsigned int, size)
	 ),

	 TP_fast_assign(
		 __entry->src = src;
		 __entry->dst = dst;
		 __entry->size = size;
	 ),

	 TP_printk("src=0x%p dst=0x%p size=%d",
		__entry->src, __entry->dst, __entry->size)
);

/*
 * end_qpace_urgent_decompress - called when qpace_urgent_decompress trigger is done
 * @src: Input address of the compressed page
 * @dst: Address to output the decompressed page to
 * @size: size of the compressed input-page
 * @ret: return value
 *
 */
TRACE_EVENT(end_qpace_urgent_decompress,

	 TP_PROTO(void *src, void *dst, unsigned int size, int ret),
	 TP_ARGS(src, dst, size, ret),

	 TP_STRUCT__entry(
		 __field(void *,  src)
		 __field(void *, dst)
		 __field(unsigned int, size)
		 __field(int,  ret)
	 ),

	 TP_fast_assign(
		 __entry->src = src;
		 __entry->dst = dst;
		 __entry->size = size;
		 __entry->ret = ret;
	 ),

	 TP_printk("src=0x%p dst=0x%p size=%d ret=%d",
		 __entry->src, __entry->dst, __entry->size, __entry->ret)
);
#endif /* _TRACE_QPACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
