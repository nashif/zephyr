/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _KERNEL_TRACE_H
#define _KERNEL_TRACE_H

#include <kernel.h>

/* Below IDs are used with systemview, not final to the zephyr tracing API */
#define SYS_TRACE_ID_OFFSET                  (32u)

#define SYS_TRACE_ID_MUTEX_INIT              (1u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_MUTEX_UNLOCK            (2u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_MUTEX_LOCK              (3u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_SEMA_INIT               (4u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_SEMA_GIVE               (5u + SYS_TRACE_ID_OFFSET)
#define SYS_TRACE_ID_SEMA_TAKE               (6u + SYS_TRACE_ID_OFFSET)


#if CONFIG_TRACING
void z_sys_trace_idle(void);
void z_sys_trace_isr_enter(void);
void z_sys_trace_isr_exit_to_scheduler(void);
void z_sys_trace_thread_switched_in(void);
#endif

#ifdef CONFIG_SEGGER_SYSTEMVIEW
#include "tracing_sysview.h"
#else

#define sys_trace_thread_switched_out()

#define sys_trace_thread_switched_in()

#define sys_trace_thread_priority_set(thread)

#define sys_trace_thread_create(thread)

#define sys_trace_thread_abort(thread)

#define sys_trace_thread_suspend(thread)

#define sys_trace_thread_resume(thread)

#define sys_trace_thread_ready(thread)

#define sys_trace_thread_pend(thread)

#define sys_trace_thread_info(thread)

#define sys_trace_isr_enter()

#define sys_trace_isr_exit()

#define sys_trace_isr_exit_to_scheduler()

#define sys_trace_void(id)

#define sys_trace_end_call(id)

#define z_sys_trace_idle()

#define z_sys_trace_isr_enter()

#define z_sys_trace_isr_exit_to_scheduler()

#define z_sys_trace_thread_switched_in()

#endif
#endif
