/* kticks.h */

/*
 * Copyright (c) 1997-2010, 2014-2015 Wind River Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3) Neither the name of Wind River Systems nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _KTICKS_H
#define _KTICKS_H

#include "microkernel/k_types.h"
#include "microkernel/k_struct.h"

extern K_TIMER *_k_timer_list_head;
extern K_TIMER *_k_timer_list_tail;
extern int64_t _k_sys_clock_tick_count;
extern unsigned int _k_workload_slice;
extern unsigned int _k_workload_ticks;
extern unsigned int _k_workload_ref_time;
extern unsigned int _k_workload_t0;
extern unsigned int _k_workload_t1;
extern volatile unsigned int _k_workload_n0;
extern volatile unsigned int _k_workload_n1;
extern volatile unsigned int _k_workload_i;
extern volatile unsigned int _k_workload_i0;
extern volatile unsigned int _k_workload_delta;
extern volatile unsigned int _k_workload_start_time;
extern volatile unsigned int _k_workload_end_time;

extern void enlist_timer(K_TIMER *T);
extern void delist_timer(K_TIMER *T);
extern void enlist_timeout(struct k_args *P);
extern void delist_timeout(K_TIMER *T);
extern void force_timeout(struct k_args *A);
extern void _k_timer_list_update(int ticks);
extern void _k_timer_alloc(struct k_args *P);
extern void _k_timer_dealloc(struct k_args *P);
extern void _k_timer_start(struct k_args *P);
extern void _k_timer_stop(struct k_args *P);
extern void _k_task_sleep(struct k_args *P);
extern void _k_task_wakeup(struct k_args *P);
extern void _k_time_elapse(struct k_args *P);
extern void _k_node_workload_get(struct k_args *P);
#endif
