/*
 * Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_TRACE_MBOX_SYSVIEW_H
#define ZEPHYR_TRACE_MBOX_SYSVIEW_H
#include <zephyr/kernel.h>
#include <tracing_sysview_ids.h>
#ifdef __cplusplus
extern "C" {
#endif

#define sys_port_trace_k_mbox_async_put_enter(mbox, sem) \
    SEGGER_SYSVIEW_RecordU32x2(TID_MBOX_ASYNC_PUT, (uint32_t)(uintptr_t)mbox, (uint32_t)(uintptr_t)sem)

#define sys_port_trace_k_mbox_async_put_exit(mbox, sem) \
    SEGGER_SYSVIEW_RecordEndCall(TID_MBOX_ASYNC_PUT)

#define sys_port_trace_k_mbox_get_blocking(mbox, timeout) \
    SEGGER_SYSVIEW_RecordU32(TID_MBOX_GET, (uint32_t)(uintptr_t)mbox)

#define sys_port_trace_k_mbox_get_enter(mbox, timeout) \
    SEGGER_SYSVIEW_RecordU32x2(TID_MBOX_GET, (uint32_t)(uintptr_t)mbox, (uint32_t)timeout.ticks)

#define sys_port_trace_k_mbox_get_exit(mbox, timeout, ret) \
    SEGGER_SYSVIEW_RecordEndCallU32(TID_MBOX_GET, (uint32_t)ret)

#define sys_port_trace_k_mbox_init(k_mbox) \
    SEGGER_SYSVIEW_RecordU32(TID_MBOX_INIT, (uint32_t)(uintptr_t)k_mbox)

#define sys_port_trace_k_mbox_message_put_blocking(mbox, timeout) \
    SEGGER_SYSVIEW_RecordU32(TID_MBOX_MESSAGE_PUT, (uint32_t)(uintptr_t)mbox)

#define sys_port_trace_k_mbox_message_put_enter(mbox, timeout) \
    SEGGER_SYSVIEW_RecordU32x2(TID_MBOX_MESSAGE_PUT, (uint32_t)(uintptr_t)mbox, (uint32_t)timeout.ticks)

#define sys_port_trace_k_mbox_message_put_exit(mbox, timeout, ret) \
    SEGGER_SYSVIEW_RecordEndCallU32(TID_MBOX_MESSAGE_PUT, (uint32_t)ret)

#define sys_port_trace_k_mbox_put_enter(mbox, timeout) \
    SEGGER_SYSVIEW_RecordU32x2(TID_MBOX_PUT, (uint32_t)(uintptr_t)mbox, (uint32_t)timeout.ticks)

#define sys_port_trace_k_mbox_put_exit(mbox, timeout, ret) \
    SEGGER_SYSVIEW_RecordEndCallU32(TID_MBOX_PUT, (uint32_t)ret)

#ifdef __cplusplus
}
#endif
#endif /* ZEPHYR_TRACE_MBOX_SYSVIEW_H */
