/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Sustained interrupt round-trip cost: the ISR re-triggers the
 * benchmark interrupt from within its own handler so that interrupts
 * are serviced back to back (tail-chained where the hardware supports
 * it). The reported average is the cost of one full entry + ISR body +
 * exit round trip; its inverse is the maximum sustainable interrupt
 * rate. Requires the sw-irq trigger backend.
 */

#include <zephyr/kernel.h>

#include "bench.h"
#include "trigger.h"

static volatile bool done;
static volatile uint32_t isr_count;
static volatile timing_t end_timestamp;

static void throughput_handler(void)
{
	isr_count++;

	if (isr_count < CONFIG_INT_BENCH_NUM_ITERATIONS) {
		bench_trigger();
	} else {
		end_timestamp = timing_counter_get();
		done = true;
	}
}

void int_bench_throughput(void)
{
	timing_t start;
	timing_t finish;

	isr_count = 0U;
	done = false;

	bench_trigger_set_handler(throughput_handler);

	start = timing_counter_get();
	bench_trigger();

	while (!done) {
	}

	bench_trigger_set_handler(NULL);

	finish = end_timestamp;
	bench_report_avg("int.throughput.round_trip", "Back to back interrupt round trip",
			 timing_cycles_get(&start, &finish), CONFIG_INT_BENCH_NUM_ITERATIONS);
}
