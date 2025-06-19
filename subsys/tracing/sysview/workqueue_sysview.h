/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_TRACE_WORKQUEUE_SYSVIEW_H
#define ZEPHYR_TRACE_WORKQUEUE_SYSVIEW_H

#include <zephyr/kernel.h>
#include <tracing_sysview_ids.h>

#ifdef __cplusplus
extern "C" {
#endif



#define sys_port_trace_k_work_init(work)                                                           \
	SEGGER_SYSVIEW_RecordU32(TID_WORK_INIT, (uint32_t)(uintptr_t)work)

#define sys_port_trace_k_work_submit_to_queue_enter(queue, work)                                   \
	SEGGER_SYSVIEW_RecordU32x2(TID_WORK_SUBMIT_TO_QUEUE, (uint32_t)(uintptr_t)queue,           \
				   (uint32_t)(uintptr_t)work)

#define sys_port_trace_k_work_submit_to_queue_exit(queue, work, ret)                               \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_SUBMIT_TO_QUEUE, (uint32_t)ret)

#define sys_port_trace_k_work_submit_enter(work)                                                   \
	SEGGER_SYSVIEW_RecordU32(TID_WORK_SUBMIT, (uint32_t)(uintptr_t)work)

#define sys_port_trace_k_work_submit_exit(work, ret)                                               \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_SUBMIT, (uint32_t)ret)

#define sys_port_trace_k_work_flush_enter(work)                                                    \
	SEGGER_SYSVIEW_RecordU32(TID_WORK_FLUSH, (uint32_t)(uintptr_t)work)

#define sys_port_trace_k_work_flush_blocking(work, timeout)

#define sys_port_trace_k_work_flush_exit(work, ret)                                                \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_FLUSH, (uint32_t)ret)

#define sys_port_trace_k_work_cancel_enter(work)                                                   \
	SEGGER_SYSVIEW_RecordU32(TID_WORK_CANCEL, (uint32_t)(uintptr_t)work)

#define sys_port_trace_k_work_cancel_exit(work, ret)                                               \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_CANCEL, (uint32_t)ret)

#define sys_port_trace_k_work_cancel_sync_enter(work, sync)                                        \
	SEGGER_SYSVIEW_RecordU32x2(TID_WORK_CANCEL_SYNC, (uint32_t)(uintptr_t)work,                \
				   (uint32_t)(uintptr_t)sync)

#define sys_port_trace_k_work_cancel_sync_blocking(work, sync)

#define sys_port_trace_k_work_cancel_sync_exit(work, sync, ret)                                    \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_CANCEL_SYNC, (uint32_t)ret)

#define sys_port_trace_k_work_queue_init(queue)             \
	SEGGER_SYSVIEW_RecordU32(TID_WORK_QUEUE_INIT,       \
				 (uint32_t)(uintptr_t)queue)

#define sys_port_trace_k_work_queue_start_enter(queue)                                             \
	SEGGER_SYSVIEW_RecordU32(TID_WORK_QUEUE_START, (uint32_t)(uintptr_t)queue)

#define sys_port_trace_k_work_queue_start_exit(queue)                                              \
	SEGGER_SYSVIEW_RecordEndCall(TID_WORK_QUEUE_START)

#define sys_port_trace_k_work_queue_stop_enter(queue, timeout)                                     \
	SEGGER_SYSVIEW_RecordU32x2(TID_WORK_QUEUE_STOP, (uint32_t)(uintptr_t)queue,                \
				   (uint32_t)timeout.ticks)

#define sys_port_trace_k_work_queue_stop_blocking(queue, timeout)                                  \
	SEGGER_SYSVIEW_RecordU32x2(TID_WORK_QUEUE_STOP, (uint32_t)(uintptr_t)queue,                \
				   (uint32_t)timeout.ticks)

#define sys_port_trace_k_work_queue_stop_exit(queue, timeout, ret)                                 \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_QUEUE_STOP, (uint32_t)ret)

#define sys_port_trace_k_work_queue_drain_enter(queue)                                             \
	SEGGER_SYSVIEW_RecordU32(TID_WORK_QUEUE_DRAIN, (uint32_t)(uintptr_t)queue)

#define sys_port_trace_k_work_queue_drain_exit(queue, ret)                                         \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_QUEUE_DRAIN, (uint32_t)ret)

#define sys_port_trace_k_work_queue_unplug_enter(queue)                                            \
	SEGGER_SYSVIEW_RecordU32(TID_WORK_QUEUE_UNPLUG, (uint32_t)(uintptr_t)queue)

#define sys_port_trace_k_work_queue_unplug_exit(queue, ret)                                        \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_QUEUE_UNPLUG, (uint32_t)ret)

#define sys_port_trace_k_work_delayable_init(dwork)                                                \
	SEGGER_SYSVIEW_RecordU32(TID_WORK_DELAYABLE_INIT, (uint32_t)(uintptr_t)dwork)

#define sys_port_trace_k_work_schedule_for_queue_enter(queue, dwork, delay)                        \
	SEGGER_SYSVIEW_RecordU32x3(TID_WORK_SCHEDULE_FOR_QUEUE, (uint32_t)(uintptr_t)queue,        \
				   (uint32_t)(uintptr_t)dwork, (uint32_t)delay.ticks)

#define sys_port_trace_k_work_schedule_for_queue_exit(queue, dwork, delay, ret)                    \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_SCHEDULE_FOR_QUEUE, (uint32_t)ret)

#define sys_port_trace_k_work_schedule_enter(dwork, delay)                                         \
	SEGGER_SYSVIEW_RecordU32x2(TID_WORK_SCHEDULE, (uint32_t)(uintptr_t)dwork,                  \
				   (uint32_t)delay.ticks)

#define sys_port_trace_k_work_schedule_exit(dwork, delay, ret)                                     \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_SCHEDULE, (uint32_t)ret)

#define sys_port_trace_k_work_reschedule_for_queue_enter(queue, dwork, delay)                      \
	SEGGER_SYSVIEW_RecordU32x3(TID_WORK_RESCHEDULE_FOR_QUEUE, (uint32_t)(uintptr_t)queue,      \
				   (uint32_t)(uintptr_t)dwork, (uint32_t)delay.ticks)

#define sys_port_trace_k_work_reschedule_for_queue_exit(queue, dwork, delay, ret)                  \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_RESCHEDULE_FOR_QUEUE, (uint32_t)ret)

#define sys_port_trace_k_work_reschedule_enter(dwork, delay)                                       \
	SEGGER_SYSVIEW_RecordU32x2(TID_WORK_RESCHEDULE, (uint32_t)(uintptr_t)dwork,                \
				   (uint32_t)delay.ticks)

#define sys_port_trace_k_work_reschedule_exit(dwork, delay, ret)                                   \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_RESCHEDULE, (uint32_t)ret)

#define sys_port_trace_k_work_flush_delayable_enter(dwork, sync)                                   \
	SEGGER_SYSVIEW_RecordU32x2(TID_WORK_FLUSH_DELAYABLE, (uint32_t)(uintptr_t)dwork,           \
				   (uint32_t)(uintptr_t)sync)

#define sys_port_trace_k_work_flush_delayable_exit(dwork, sync, ret)                               \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_FLUSH_DELAYABLE, (uint32_t)ret)

#define sys_port_trace_k_work_cancel_delayable_enter(dwork)                                        \
	SEGGER_SYSVIEW_RecordU32(TID_WORK_CANCEL_DELAYABLE, (uint32_t)(uintptr_t)dwork)

#define sys_port_trace_k_work_cancel_delayable_exit(dwork, ret)                                    \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_CANCEL_DELAYABLE, (uint32_t)ret)

#define sys_port_trace_k_work_cancel_delayable_sync_enter(dwork, sync)                             \
	SEGGER_SYSVIEW_RecordU32x2(TID_WORK_CANCEL_DELAYABLE_SYNC, (uint32_t)(uintptr_t)dwork,     \
				   (uint32_t)(uintptr_t)sync)

#define sys_port_trace_k_work_cancel_delayable_sync_exit(dwork, sync, ret)                         \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_CANCEL_DELAYABLE_SYNC, (uint32_t)ret)

#define sys_port_trace_k_work_poll_init_enter(work)                                                \
	SEGGER_SYSVIEW_RecordU32(TID_WORK_POLL_INIT, (uint32_t)(uintptr_t)work)

#define sys_port_trace_k_work_poll_init_exit(work) SEGGER_SYSVIEW_RecordEndCall(TID_WORK_POLL_INIT)

#define sys_port_trace_k_work_poll_submit_to_queue_enter(work_q, work, timeout)                    \
	SEGGER_SYSVIEW_RecordU32x3(TID_WORK_POLL_SUBMIT_TO_QUEUE, (uint32_t)(uintptr_t)work_q,     \
				   (uint32_t)(uintptr_t)work, (uint32_t)timeout.ticks)

#define sys_port_trace_k_work_poll_submit_to_queue_blocking(work_q, work, timeout)

#define sys_port_trace_k_work_poll_submit_to_queue_exit(work_q, work, timeout, ret)                \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_POLL_SUBMIT_TO_QUEUE, (uint32_t)ret)

#define sys_port_trace_k_work_poll_submit_enter(work, timeout)                                     \
	SEGGER_SYSVIEW_RecordU32x2(TID_WORK_POLL_SUBMIT, (uint32_t)(uintptr_t)work,                \
				   (uint32_t)timeout.ticks)

#define sys_port_trace_k_work_poll_submit_exit(work, timeout, ret)                                 \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_POLL_SUBMIT, (uint32_t)ret)

#define sys_port_trace_k_work_poll_cancel_enter(work)                                              \
	SEGGER_SYSVIEW_RecordU32(TID_WORK_POLL_CANCEL, (uint32_t)(uintptr_t)work)

#define sys_port_trace_k_work_poll_cancel_exit(work, ret)                                          \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_WORK_POLL_CANCEL, (uint32_t)ret)


#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_TRACE_WORKQUEUE_SYSVIEW_H */