/*
 * Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_TRACE_SEMA_SYSVIEW_H
#define ZEPHYR_TRACE_SEMA_SYSVIEW_H
#include <zephyr/kernel.h>
#include <tracing_sysview_ids.h>
#ifdef __cplusplus
extern "C" {
#endif


#define sys_port_trace_k_sem_init(sem, ret)                                                        \
	SEGGER_SYSVIEW_RecordU32x2(TID_SEMA_INIT, (uint32_t)(uintptr_t)sem, (int32_t)ret)

#define sys_port_trace_k_sem_give_enter(sem)                                                       \
	SEGGER_SYSVIEW_RecordU32(TID_SEMA_GIVE, (uint32_t)(uintptr_t)sem)

#define sys_port_trace_k_sem_give_exit(sem) SEGGER_SYSVIEW_RecordEndCall(TID_SEMA_GIVE)

#define sys_port_trace_k_sem_take_enter(sem, timeout)                                              \
	SEGGER_SYSVIEW_RecordU32x2(TID_SEMA_TAKE, (uint32_t)(uintptr_t)sem, (uint32_t)timeout.ticks)

#define sys_port_trace_k_sem_take_blocking(sem, timeout)

#define sys_port_trace_k_sem_take_exit(sem, timeout, ret)                                          \
	SEGGER_SYSVIEW_RecordEndCallU32(TID_SEMA_TAKE, (int32_t)ret)

#define sys_port_trace_k_sem_reset(sem)                                                            \
	SEGGER_SYSVIEW_RecordU32(TID_SEMA_RESET, (uint32_t)(uintptr_t)sem)

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_TRACE_SEMA_SYSVIEW_H */