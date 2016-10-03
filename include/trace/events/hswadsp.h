#undef TRACE_SYSTEM
#define TRACE_SYSTEM hswadsp

#if !defined(_TRACE_HSWADSP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HSWADSP_H

#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/tracepoint.h>

struct sst_hsw;
struct sst_hsw_stream;
struct sst_hsw_ipc_stream_free_req;
struct sst_hsw_ipc_volume_req;
struct sst_hsw_ipc_stream_alloc_req;
struct sst_hsw_audio_data_format_ipc;
struct sst_hsw_ipc_stream_info_reply;
struct sst_hsw_ipc_device_config_req;

DECLARE_EVENT_CLASS(sst_irq,

	TP_PROTO(uint32_t status, uint32_t mask),

	TP_ARGS(status, mask),

	TP_STRUCT__entry(
		__field(	unsigned int,	status		)
		__field(	unsigned int,	mask		)
	),

	TP_fast_assign(
		__entry->status = status;
		__entry->mask = mask;
	),

	TP_printk("status 0x%8.8x mask 0x%8.8x",
		(unsigned int)__entry->status, (unsigned int)__entry->mask)
);

DEFINE_EVENT(sst_irq, sst_irq_busy,

	TP_PROTO(unsigned int status, unsigned int mask),

	TP_ARGS(status, mask)

);

DEFINE_EVENT(sst_irq, sst_irq_done,

	TP_PROTO(unsigned int status, unsigned int mask),

	TP_ARGS(status, mask)

);

#endif /* _TRACE_HSWADSP_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
