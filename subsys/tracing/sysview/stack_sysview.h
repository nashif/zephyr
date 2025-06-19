/*
 * Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_TRACE_STACK_SYSVIEW_H
#define ZEPHYR_TRACE_STACK_SYSVIEW_H
#include <zephyr/kernel.h>
#include <tracing_sysview_ids.h>
#ifdef __cplusplus
extern "C" {
#endif

#define sys_port_trace_k_stack_alloc_init_enter(stack) \
    SEGGER_SYSVIEW_RecordU32(TID_STACK_ALLOC_INIT, (uint32_t)(uintptr_t)stack)

#define sys_port_trace_k_stack_alloc_init_exit(stack, ret) \
    SEGGER_SYSVIEW_RecordEndCallU32(TID_STACK_ALLOC_INIT, (uint32_t)ret)

#define sys_port_trace_k_stack_cleanup_enter(stack) \
    SEGGER_SYSVIEW_RecordU32(TID_STACK_CLEANUP, (uint32_t)(uintptr_t)stack)

#define sys_port_trace_k_stack_cleanup_exit(stack, ret) \
    SEGGER_SYSVIEW_RecordEndCallU32(TID_STACK_CLEANUP, (uint32_t)ret)

#define sys_port_trace_k_stack_cleanup_exit(stack, ret) \
    SEGGER_SYSVIEW_RecordEndCallU32(TID_STACK_CLEANUP, (uint32_t)ret)

#define sys_port_trace_k_stack_init(k_stack) \
    SEGGER_SYSVIEW_RecordU32(TID_STACK_INIT, (uint32_t)(uintptr_t)k_stack)

#define sys_port_trace_k_stack_pop_blocking(stack, timeout) \
    SEGGER_SYSVIEW_RecordU32(TID_STACK_POP, (uint32_t)(uintptr_t)stack)

#define sys_port_trace_k_stack_pop_enter(stack, timeout) \
    SEGGER_SYSVIEW_RecordU32x2(TID_STACK_POP, (uint32_t)(uintptr_t)stack, (uint32_t)timeout.ticks)

#define sys_port_trace_k_stack_pop_exit(stack, timeout, ret) \
    SEGGER_SYSVIEW_RecordEndCallU32(TID_STACK_POP, (uint32_t)ret)

#define sys_port_trace_k_stack_push_enter(stack) \
    SEGGER_SYSVIEW_RecordU32(TID_STACK_PUSH, (uint32_t)(uintptr_t)stack)

#define sys_port_trace_k_stack_push_exit(stack, ret) \
    SEGGER_SYSVIEW_RecordEndCallU32(TID_STACK_PUSH, (uint32_t)ret)
#ifdef __cplusplus
}
#endif
#endif /* ZEPHYR_TRACE_STACK_SYSVIEW_H */
