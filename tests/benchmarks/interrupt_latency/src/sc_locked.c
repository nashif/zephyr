/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Entry latency after a critical section: the benchmark interrupt is
 * raised while interrupts are locked, the lock is held for
 * CONFIG_INT_BENCH_LOCK_HOLD_US, then the time from irq_unlock() to
 * ISR entry is measured. This is the latency an interrupt experiences
 * when it arrives during a critical section, minus the remaining hold
 * time. Requires the sw-irq trigger backend.
 */

#include <zephyr/kernel.h>

#include "bench.h"
#include "trigger.h"

static volatile bool fired;
static volatile timing_t isr_timestamp;

static void locked_handler(void)
{
	isr_timestamp = timing_counter_get();
	fired = true;
}

void int_bench_locked(void)
{
	timing_t start;
	timing_t finish;
	unsigned int key;

	bench_samples_reset();
	bench_trigger_set_handler(locked_handler);

	for (uint32_t i = 0U; i < CONFIG_INT_BENCH_NUM_ITERATIONS; i++) {
		fired = false;

		key = irq_lock();

		bench_trigger();
		k_busy_wait(CONFIG_INT_BENCH_LOCK_HOLD_US);

		start = timing_counter_get();
		irq_unlock(key);

		while (!fired) {
		}

		finish = isr_timestamp;
		bench_sample_add(bench_adjust(timing_cycles_get(&start, &finish)));
	}

	bench_trigger_set_handler(NULL);

	bench_report("int.locked.unlock_to_isr", "irq_unlock of pended IRQ to ISR entry");
}
