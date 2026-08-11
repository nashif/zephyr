/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Interrupt exit latency, two flavors:
 *
 * 1. ISR returns to the thread it interrupted: time from the last
 *    instruction of the ISR body to the interrupted thread resuming.
 * 2. ISR wakes a higher priority thread: time from the last
 *    instruction of the ISR body to the woken thread running (i.e.
 *    interrupt exit plus context switch).
 *
 * Both work with either trigger backend; with irq_offload() they
 * measure the offload trap exit path, as latency_measure does.
 */

#include <zephyr/kernel.h>

#include "bench.h"
#include "trigger.h"

#define WAITER_STACK_SIZE (1024 + CONFIG_TEST_EXTRA_STACK_SIZE)

static K_SEM_DEFINE(wake_sem, 0, 1);
static K_SEM_DEFINE(sync_sem, 0, 1);
static K_THREAD_STACK_DEFINE(waiter_stack, WAITER_STACK_SIZE);
static struct k_thread waiter_thread;

static volatile bool fired;
static volatile timing_t isr_exit_timestamp;

static void resume_handler(void)
{
	fired = true;
	isr_exit_timestamp = timing_counter_get();
}

static void measure_resume_interrupted(void)
{
	timing_t start;
	timing_t finish;

	bench_samples_reset();
	bench_trigger_set_handler(resume_handler);

	for (uint32_t i = 0U; i < CONFIG_INT_BENCH_NUM_ITERATIONS; i++) {
		fired = false;

		bench_trigger();

		while (!fired) {
		}

		finish = timing_counter_get();
		start = isr_exit_timestamp;
		bench_sample_add(bench_adjust(timing_cycles_get(&start, &finish)));
	}

	bench_trigger_set_handler(NULL);

	bench_report("int.exit.resume_interrupted", "ISR exit to interrupted thread");
}

static void resched_handler(void)
{
	k_sem_give(&wake_sem);
	isr_exit_timestamp = timing_counter_get();
}

static void waiter_entry(void *p1, void *p2, void *p3)
{
	uint32_t num_iterations = (uint32_t)(uintptr_t)p1;
	timing_t start;
	timing_t finish;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	for (uint32_t i = 0U; i < num_iterations; i++) {
		k_sem_take(&wake_sem, K_FOREVER);

		finish = timing_counter_get();
		start = isr_exit_timestamp;
		bench_sample_add(bench_adjust(timing_cycles_get(&start, &finish)));

		k_sem_give(&sync_sem);
	}
}

static void measure_resched(void)
{
	int priority = k_thread_priority_get(k_current_get());

	bench_samples_reset();
	bench_trigger_set_handler(resched_handler);

	k_thread_create(&waiter_thread, waiter_stack, K_THREAD_STACK_SIZEOF(waiter_stack),
			waiter_entry, (void *)(uintptr_t)CONFIG_INT_BENCH_NUM_ITERATIONS,
			NULL, NULL, priority - 1, 0, K_NO_WAIT);

	for (uint32_t i = 0U; i < CONFIG_INT_BENCH_NUM_ITERATIONS; i++) {
		bench_trigger();
		k_sem_take(&sync_sem, K_FOREVER);
	}

	k_thread_join(&waiter_thread, K_FOREVER);
	bench_trigger_set_handler(NULL);

	bench_report("int.exit.reschedule", "ISR exit to woken higher prio thread");
}

void int_bench_exit(void)
{
	measure_resume_interrupted();
	measure_resched();
}
