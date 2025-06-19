/*
 * Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_TRACE_MSGQ_SYSVIEW_H
#define ZEPHYR_TRACE_MSGQ_SYSVIEW_H
#include <zephyr/kernel.h>
#include <tracing_sysview_ids.h>
#ifdef __cplusplus
extern "C" {
#endif

#define sys_port_trace_k_msgq_alloc_init_enter(msgq) \
    SEGGER_SYSVIEW_RecordU32(TID_MSGQ_ALLOC_INIT, (uint32_t)(uintptr_t)msgq)

#define sys_port_trace_k_msgq_alloc_init_exit(msgq, ret) \
    SEGGER_SYSVIEW_RecordEndCallU32(TID_MSGQ_ALLOC_INIT, (uint32_t)ret)

#define sys_port_trace_k_msgq_cleanup_enter(msgq) \
    SEGGER_SYSVIEW_RecordU32(TID_MSGQ_CLEANUP, (uint32_t)(uintptr_t)msgq)

#define sys_port_trace_k_msgq_cleanup_exit(msgq, ret) \
    SEGGER_SYSVIEW_RecordEndCallU32(TID_MSGQ_CLEANUP, (uint32_t)ret)

#define sys_port_trace_k_msgq_get_blocking(msgq, timeout) \
    SEGGER_SYSVIEW_RecordU32(TID_MSGQ_GET, (uint32_t)(uintptr_t)msgq)

#define sys_port_trace_k_msgq_get_enter(msgq, timeout) \
    SEGGER_SYSVIEW_RecordU32x2(TID_MSGQ_GET, (uint32_t)(uintptr_t)msgq, (uint32_t)timeout.ticks)

#define sys_port_trace_k_msgq_get_exit(msgq, timeout, ret) \
    SEGGER_SYSVIEW_RecordEndCallU32(TID_MSGQ_GET, (uint32_t)ret)

#define sys_port_trace_k_msgq_init(k_msgq) \
    SEGGER_SYSVIEW_RecordU32(TID_MSGQ_INIT, (uint32_t)(uintptr_t)k_msgq)

#define sys_port_trace_k_msgq_put_blocking(msgq, timeout) \
    SEGGER_SYSVIEW_RecordU32(TID_MSGQ_PUT, (uint32_t)(uintptr_t)msgq)

#define sys_port_trace_k_msgq_put_enter(msgq, timeout) \
    SEGGER_SYSVIEW_RecordU32x2(TID_MSGQ_PUT, (uint32_t)(uintptr_t)msgq, (uint32_t)timeout.ticks)

#define sys_port_trace_k_msgq_put_exit(msgq, timeout, ret) \
    SEGGER_SYSVIEW_RecordEndCallU32(TID_MSGQ_PUT, (uint32_t)ret)

#define sys_port_trace_k_msgq_peek(msgq, ret) \
    SEGGER_SYSVIEW_RecordU32x2(TID_MSGQ_PEEK, (uint32_t)(uintptr_t)msgq, (uint32_t)ret)

#define sys_port_trace_k_msgq_purge(msgq) \
    SEGGER_SYSVIEW_RecordU32(TID_MSGQ_PURGE, (uint32_t)(msgq))

#ifdef __cplusplus
}
#endif
#endif /* ZEPHYR_TRACE_MSGQ_SYSVIEW_H */
