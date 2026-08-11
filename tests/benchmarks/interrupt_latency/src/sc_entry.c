/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Interrupt entry latency: time from the software write that raises
 * the interrupt to the first timestamp taken inside the ISR. Requires
 * the sw-irq trigger backend (a real asynchronous interrupt).
 */

#include <zephyr/kernel.h>

#include "bench.h"
#include "trigger.h"

static volatile bool fired;
static volatile timing_t isr_timestamp;

static void entry_handler(void)
{
	isr_timestamp = timing_counter_get();
	fired = true;
}

void int_bench_entry(void)
{
	timing_t start;
	timing_t finish;

	bench_samples_reset();
	bench_trigger_set_handler(entry_handler);

	for (uint32_t i = 0U; i < CONFIG_INT_BENCH_NUM_ITERATIONS; i++) {
		fired = false;

		start = timing_counter_get();
		bench_trigger();

		while (!fired) {
		}

		finish = isr_timestamp;
		bench_sample_add(bench_adjust(timing_cycles_get(&start, &finish)));
	}

	bench_trigger_set_handler(NULL);

	bench_report("int.entry.trigger_to_isr", "Trigger write to ISR entry");
}
