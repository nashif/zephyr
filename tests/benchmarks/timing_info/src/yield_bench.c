/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <zephyr.h>
#include <tc_util.h>
#include <ksched.h>
#include "timing_info.h"

K_SEM_DEFINE(yield_sem, 0, 1);

/* To time thread creation*/
extern K_THREAD_STACK_DEFINE(my_stack_area, STACK_SIZE);
extern K_THREAD_STACK_DEFINE(my_stack_area_0, STACK_SIZE);
extern struct k_thread my_thread;
extern struct k_thread my_thread_0;

extern char sline[];

extern timing_t thread_sleep_start_time;
extern timing_t thread_sleep_end_time;
timing_t thread_yield_start_time;
timing_t thread_yield_end_time;
static uint32_t count;

void thread_yield0_test(void *p1, void *p2, void *p3);
void thread_yield1_test(void *p1, void *p2, void *p3);

k_tid_t yield0_tid;
k_tid_t yield1_tid;
void yield_bench(void)
{
	/* Thread yield */
	k_sleep(K_MSEC(10));

	yield0_tid = k_thread_create(&my_thread, my_stack_area,
				     STACK_SIZE,
				     thread_yield0_test,
				     NULL, NULL, NULL,
				     0 /*priority*/, 0, K_NO_WAIT);

	yield1_tid = k_thread_create(&my_thread_0, my_stack_area_0,
				     STACK_SIZE,
				     thread_yield1_test,
				     NULL, NULL, NULL,
				     0 /*priority*/, 0, K_NO_WAIT);

	k_sleep(K_MSEC(1000));

	k_thread_abort(yield0_tid);
	k_thread_abort(yield1_tid);

	k_thread_join(yield0_tid, K_FOREVER);
	k_thread_join(yield1_tid, K_FOREVER);

	/* read the time of start of the sleep till the swap happens */
	arch_timing_value_swap_end = 1U;

	thread_sleep_start_time = timing_counter_get();

	k_sleep(K_MSEC(1000));

	thread_sleep_end_time = arch_timing_swap_end;

	uint32_t yield_cycles = CALCULATE_CYCLES(thread, yield) / 2000U;
	uint32_t sleep_cycles = CALCULATE_CYCLES(thread, sleep);

	PRINT_STATS("Thread yield", yield_cycles);

	PRINT_STATS("Thread sleep", sleep_cycles);
}


void thread_yield0_test(void *p1, void *p2, void *p3)
{
	k_sem_take(&yield_sem, K_MSEC(10));

	thread_yield_start_time = timing_counter_get();

	while (count != 1000U) {
		count++;
		k_yield();
	}

	thread_yield_end_time = timing_counter_get();

	k_thread_abort(yield1_tid);
}

void thread_yield1_test(void *p1, void *p2, void *p3)
{
	k_sem_give(&yield_sem);
	while (1) {
		k_yield();
	}
}
