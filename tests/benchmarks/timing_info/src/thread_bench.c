/*
 * Copyright (c) 2013-2015 Wind River Systems, Inc.
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Measure time
 *
 */
#include <kernel.h>
#include <zephyr.h>
#include <tc_util.h>
#include <ksched.h>
#include "timing_info.h"
char sline[256];
/* FILE *output_file = stdout; */

/* location of the time stamps*/
extern uint32_t arch_timing_value_swap_end;
extern uint64_t arch_timing_value_swap_common;

volatile uint64_t thread_abort_current_end_time;
volatile uint64_t thread_abort_current_start_time;

/* Thread suspend*/
volatile uint64_t thread_suspend_start_time;
volatile uint64_t thread_suspend_end_time;

/* Thread resume*/
volatile uint64_t thread_resume_start_time;
volatile uint64_t thread_resume_end_time;

/* Thread sleep*/
volatile uint64_t thread_sleep_start_time;
volatile uint64_t thread_sleep_end_time;

/*For benchmarking msg queues*/
k_tid_t producer_tid;
k_tid_t consumer_tid;

/* To time thread creation*/
K_THREAD_STACK_DEFINE(my_stack_area, STACK_SIZE);
K_THREAD_STACK_DEFINE(my_stack_area_0, STACK_SIZE);
struct k_thread my_thread;
struct k_thread my_thread_0;

uint64_t dummy_time;
uint64_t start_time;
uint64_t test_end_time;

void test_thread_entry(void *p, void *p1, void *p2)
{
	static int i;

	i++;
}


void thread_swap_test(void *p1, void *p2, void *p3)
{
	arch_timing_value_swap_end = 1U;
	TIMING_INFO_PRE_READ();
	thread_abort_current_start_time = TIMING_INFO_OS_GET_TIME();
	k_thread_abort(_current);
}

void thread_suspend_test(void *p1, void *p2, void *p3)
{
	TIMING_INFO_PRE_READ();
	thread_suspend_start_time = TIMING_INFO_OS_GET_TIME();
	k_thread_suspend(_current);

	/* comes to this line once its resumed*/
	TIMING_INFO_PRE_READ();
	thread_resume_end_time = TIMING_INFO_OS_GET_TIME();
}

void system_thread_bench(void)
{
	uint32_t total_cycles;

	/*Thread create*/
	uint64_t thread_create_start_time;
	uint64_t thread_create_end_time;

	/*Thread cancel*/
	uint64_t thread_abort_nonrun_start_time;
	uint64_t thread_abort_nonrun_end_time;

	/* to measure context switch time */
	k_tid_t ctx_tid = k_thread_create(&my_thread_0, my_stack_area_0,
					  STACK_SIZE, thread_swap_test,
					  NULL, NULL, NULL,
					  -1 /*priority*/, 0, K_NO_WAIT);

	k_sleep(K_MSEC(1));
	thread_abort_current_end_time = (arch_timing_value_swap_common);
	arch_timing_swap_end = arch_timing_value_swap_common;

	uint32_t total_swap_cycles =
		arch_timing_swap_end -
		arch_timing_swap_start;

	/* Interrupt latency*/
	uint64_t local_end_intr_time = arch_timing_irq_end;
	uint64_t local_start_intr_time = arch_timing_irq_start;

	k_thread_abort(ctx_tid);
	k_thread_join(ctx_tid, K_FOREVER);

	/*******************************************************************/

	/* thread create */
	TIMING_INFO_PRE_READ();
	thread_create_start_time = TIMING_INFO_OS_GET_TIME();

	k_tid_t my_tid = k_thread_create(&my_thread, my_stack_area,
					 STACK_SIZE,
					 thread_swap_test,
					 NULL, NULL, NULL,
					 5 /*priority*/, 0, K_FOREVER);

	TIMING_INFO_PRE_READ();
	thread_create_end_time = TIMING_INFO_OS_GET_TIME();

	/* aborting a non-running thread */
	TIMING_INFO_PRE_READ();
	thread_abort_nonrun_start_time = TIMING_INFO_OS_GET_TIME();
	k_thread_abort(my_tid);

	TIMING_INFO_PRE_READ();
	thread_abort_nonrun_end_time = TIMING_INFO_OS_GET_TIME();

	k_thread_join(my_tid, K_FOREVER);

	/* Thread suspend */
	k_tid_t sus_res_tid = k_thread_create(&my_thread, my_stack_area,
					      STACK_SIZE,
					      thread_suspend_test,
					      NULL, NULL, NULL,
					      -1 /*priority*/, 0, K_NO_WAIT);

	TIMING_INFO_PRE_READ();
	thread_suspend_end_time = TIMING_INFO_OS_GET_TIME();

	/* At this point test for resume*/
	k_thread_resume(sus_res_tid);

	/* calculation for resume */
	thread_resume_start_time = thread_suspend_end_time;

	k_thread_abort(sus_res_tid);
	k_thread_join(sus_res_tid, K_FOREVER);

	/*******************************************************************/

	/* Only print lower 32bit of time result */
	PRINT_STATS("Context switch", total_swap_cycles);

	/* Interrupt latency */
	uint32_t intr_latency_cycles =
		local_end_intr_time -
		local_start_intr_time;

	PRINT_STATS("Interrupt latency", intr_latency_cycles);

	/* tick overhead */
	total_cycles =
		arch_timing_tick_end -
		arch_timing_tick_start;

	PRINT_STATS("Tick overhead", total_cycles);

	/* thread creation */
	total_cycles = CALCULATE_CYCLES(thread, create);
	PRINT_STATS("Thread creation", total_cycles);

	/* thread cancel */
	total_cycles = CALCULATE_CYCLES(thread, abort_nonrun);
	PRINT_STATS("Thread abort (non-running)", total_cycles);

	/* thread abort */
	total_cycles = CALCULATE_CYCLES(thread, abort_current);
	PRINT_STATS("Thread abort (_current)", total_cycles);

	/* thread suspend */
	total_cycles = CALCULATE_CYCLES(thread, suspend);
	PRINT_STATS("Thread suspend", total_cycles);

	/* thread resume */
	total_cycles = CALCULATE_CYCLES(thread, resume);
	PRINT_STATS("Thread resume", total_cycles);
}
