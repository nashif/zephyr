
/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright The Zephyr Project Contributors
 */


#ifdef __cplusplus
extern "C" {
#endif

void sys_trace_thread_info(struct k_thread *thread);

#define sys_port_trace_k_thread_foreach_enter() SEGGER_SYSVIEW_RecordVoid(TID_THREAD_FOREACH)

#define sys_port_trace_k_thread_foreach_exit() SEGGER_SYSVIEW_RecordEndCall(TID_THREAD_FOREACH)

#define sys_port_trace_k_thread_foreach_unlocked_enter()                                           \
	SEGGER_SYSVIEW_RecordVoid(TID_THREAD_FOREACH_UNLOCKED)

#define sys_port_trace_k_thread_foreach_unlocked_exit()                                            \
	SEGGER_SYSVIEW_RecordEndCall(TID_THREAD_FOREACH_UNLOCKED)

#define sys_port_trace_k_thread_create(new_thread)                                                 \
	do {                                                                                       \
		SEGGER_SYSVIEW_OnTaskCreate((uint32_t)(uintptr_t)new_thread);                      \
		sys_trace_thread_info(new_thread);                                                 \
	} while (false)

#define sys_port_trace_k_thread_user_mode_enter()                                                  \
	SEGGER_SYSVIEW_RecordVoid(TID_THREAD_USERMODE_ENTER)

#define sys_port_trace_k_thread_heap_assign(thread, heap)
#define sys_port_trace_k_thread_join_enter(thread, timeout)                                        \
	SEGGER_SYSVIEW_RecordU32x2(TID_THREAD_JOIN, (uint32_t)(uintptr_t)thread,                   \
				   (uint32_t)timeout.ticks)
#define sys_port_trace_k_thread_join_blocking(thread, timeout)
#define sys_port_trace_k_thread_join_exit(thread, timeout, ret)                                    \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_THREAD_JOIN, (int32_t)ret)

#define sys_port_trace_k_thread_sleep_enter(timeout)                                               \
	SEGGER_SYSVIEW_RecordU32(TID_SLEEP, (uint32_t)k_ticks_to_ms_floor32(timeout.ticks))

#define sys_port_trace_k_thread_sleep_exit(timeout, ret)                                           \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_SLEEP, (int32_t)ret)

#define sys_port_trace_k_thread_msleep_enter(ms) SEGGER_SYSVIEW_RecordU32(TID_MSLEEP, (uint32_t)ms)

#define sys_port_trace_k_thread_msleep_exit(ms, ret)                                               \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_MSLEEP, (int32_t)ret)

#define sys_port_trace_k_thread_usleep_enter(us) SEGGER_SYSVIEW_RecordU32(TID_USLEEP, (uint32_t)us)

#define sys_port_trace_k_thread_usleep_exit(us, ret)                                               \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_USLEEP, (int32_t)ret)

#define sys_port_trace_k_thread_busy_wait_enter(usec_to_wait)                                      \
	SEGGER_SYSVIEW_RecordU32(TID_BUSYWAIT, (uint32_t)usec_to_wait)

#define sys_port_trace_k_thread_busy_wait_exit(usec_to_wait)                                       \
	SEGGER_SYSVIEW_RecordEndCall(TID_BUSYWAIT)

#define sys_port_trace_k_thread_yield() SEGGER_SYSVIEW_RecordVoid(TID_THREAD_YIELD)

#define sys_port_trace_k_thread_wakeup(thread)                                                     \
	SEGGER_SYSVIEW_RecordU32(TID_THREAD_WAKEUP, (uint32_t)(uintptr_t)thread)

#define sys_port_trace_k_thread_start(thread)                                                      \
	SEGGER_SYSVIEW_RecordU32(TID_THREAD_START, (uint32_t)(uintptr_t)thread)

#define sys_port_trace_k_thread_abort(thread)                                                      \
	SEGGER_SYSVIEW_RecordU32(TID_THREAD_ABORT, (uint32_t)(uintptr_t)thread)

#define sys_port_trace_k_thread_abort_enter(thread)                                                \
	SEGGER_SYSVIEW_RecordU32(TID_THREAD_ABORT, (uint32_t)(uintptr_t)thread)

#define sys_port_trace_k_thread_abort_exit(thread)                                                 \
	SEGGER_SYSVIEW_RecordEndCall(TID_THREAD_ABORT)

#define sys_port_trace_k_thread_suspend_enter(thread)                                              \
	SEGGER_SYSVIEW_RecordU32(TID_THREAD_SUSPEND, (uint32_t)(uintptr_t)thread)

#define sys_port_trace_k_thread_suspend_exit(thread)                                               \
	SEGGER_SYSVIEW_RecordEndCall(TID_THREAD_SUSPEND)

#define sys_port_trace_k_thread_resume_enter(thread)                                               \
	SEGGER_SYSVIEW_RecordU32(TID_THREAD_RESUME, (uint32_t)(uintptr_t)thread)

#define sys_port_trace_k_thread_resume_exit(thread) SEGGER_SYSVIEW_RecordEndCall(TID_THREAD_RESUME)

#define sys_port_trace_k_thread_sched_lock()

#define sys_port_trace_k_thread_sched_unlock()

#define sys_port_trace_k_thread_name_set(thread, ret) do { \
		SEGGER_SYSVIEW_RecordU32(TID_THREAD_NAME_SET, (uint32_t)(uintptr_t)thread); \
		sys_trace_thread_info(thread);	\
	} while (false)

#define sys_port_trace_k_thread_switched_out() sys_trace_k_thread_switched_out()

#define sys_port_trace_k_thread_switched_in() sys_trace_k_thread_switched_in()

#define sys_port_trace_k_thread_info(thread) sys_trace_k_thread_info(thread)

#define sys_port_trace_k_thread_sched_wakeup(thread)                                               \
	SEGGER_SYSVIEW_RecordU32(TID_THREAD_WAKEUP, (uint32_t)(uintptr_t)thread)

#define sys_port_trace_k_thread_sched_abort(thread)                                                \
	SEGGER_SYSVIEW_RecordU32(TID_THREAD_ABORT, (uint32_t)(uintptr_t)thread)

#define sys_port_trace_k_thread_sched_priority_set(thread, prio)                                   \
	SEGGER_SYSVIEW_RecordU32x2(TID_THREAD_PRIORITY_SET,                                        \
				   SEGGER_SYSVIEW_ShrinkId((uint32_t)thread), prio);

#define sys_port_trace_k_thread_sched_ready(thread)                                                \
	SEGGER_SYSVIEW_OnTaskStartReady((uint32_t)(uintptr_t)thread)

#define sys_port_trace_k_thread_sched_pend(thread)                                                 \
	SEGGER_SYSVIEW_OnTaskStopReady((uint32_t)(uintptr_t)thread, 3 << 3)

#define sys_port_trace_k_thread_sched_resume(thread)

#define sys_port_trace_k_thread_sched_suspend(thread)                                              \
	SEGGER_SYSVIEW_OnTaskStopReady((uint32_t)(uintptr_t)thread, 3 << 3)


#ifdef __cplusplus
}
#endif