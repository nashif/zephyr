/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Interrupt handling benchmark.
 *
 * Measures the individual components of interrupt handling (entry
 * latency, exit latency with and without rescheduling, latency after a
 * critical section, dynamic connection cost and sustained round-trip
 * cost) using a portable trigger backend abstraction; see README.rst.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/tc_util.h>
#include <zephyr/timing/timing.h>

#include "bench.h"
#include "trigger.h"

int bench_error_count;

int main(void)
{
	timing_init();
	timing_start();

	bench_calibrate();

#ifdef CONFIG_INT_BENCH_TRIGGER_SW_IRQ
	printk("Interrupt benchmark: trigger=sw-irq (line %u), %u iterations\n",
	       bench_trigger_irq_line(), CONFIG_INT_BENCH_NUM_ITERATIONS);
#else
	printk("Interrupt benchmark: trigger=irq_offload (exit paths only), %u iterations\n",
	       CONFIG_INT_BENCH_NUM_ITERATIONS);
#endif

	if (bench_trigger_init() != 0) {
		printk("Failed to initialize trigger backend\n");
		bench_error_count++;
	}

#ifdef CONFIG_INT_BENCH_SCENARIO_ENTRY
	int_bench_entry();
#endif

#ifdef CONFIG_INT_BENCH_SCENARIO_EXIT
	int_bench_exit();
#endif

#ifdef CONFIG_INT_BENCH_SCENARIO_LOCKED
	int_bench_locked();
#endif

#ifdef CONFIG_INT_BENCH_SCENARIO_THROUGHPUT
	int_bench_throughput();
#endif

#ifdef CONFIG_INT_BENCH_SCENARIO_DYNAMIC
	int_bench_dynamic();
#endif

	timing_stop();

	TC_END_REPORT((bench_error_count == 0) ? TC_PASS : TC_FAIL);

	return 0;
}
