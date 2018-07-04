/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _KERNEL_TRACE_H
#define _KERNEL_TRACE_H

#include <kernel.h>

static inline void sys_trace_thread_switched_out(void)
{
}

static inline void sys_trace_thread_switched_in(void)
{
}

static inline void sys_trace_thread_priority_get(struct k_thread *thread)
{
}

static inline void sys_trace_thread_priority_set(struct k_thread *thread)
{
}

static inline void sys_trace_thread_create(struct k_thread *thread)
{
}

static inline void sys_trace_thread_abort(struct k_thread *thread)
{
}

static inline void sys_trace_thread_cancel(struct k_thread *thread)
{
}

static inline void sys_trace_thread_suspend(struct k_thread *thread)
{
}

static inline void sys_trace_thread_resume(struct k_thread *thread)
{
}

static inline void sys_trace_thread_ready(struct k_thread *thread)
{
}

static inline void sys_trace_thread_pend(struct k_thread *thread)
{
}

static inline void sys_trace_thread_info(struct k_thread *thread)
{
}

static inline void sys_trace_isr_enter(void)
{
}

static inline void sys_trace_isr_exit(void)
{
}

static inline void sys_trace_isr_exit_to_scheduler(void)
{
}

static inline void sys_trace_idle(void)
{
}

static inline void sys_trace_void(unsigned id)
{
}

static inline void sys_trace_end_call(unsigned id)
{
}

#endif
