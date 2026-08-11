/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Cost of installing an interrupt handler at runtime with
 * irq_connect_dynamic(). The benchmark line is disabled while the
 * handler is (re)installed because z_isr_install() requires the line
 * to be disabled on most architectures.
 */

#include <zephyr/kernel.h>
#include <zephyr/irq.h>

#include "bench.h"
#include "trigger.h"

void int_bench_dynamic(void)
{
	timing_t start;
	timing_t finish;
	unsigned int line = bench_trigger_irq_line();

	bench_samples_reset();

	irq_disable(line);

	for (uint32_t i = 0U; i < CONFIG_INT_BENCH_NUM_ITERATIONS; i++) {
		start = timing_counter_get();
		(void)irq_connect_dynamic(line, CONFIG_INT_BENCH_IRQ_PRIO,
					  bench_trigger_isr, NULL, 0);
		finish = timing_counter_get();

		bench_sample_add(bench_adjust(timing_cycles_get(&start, &finish)));
	}

	irq_enable(line);

	bench_report("int.dynamic.connect", "irq_connect_dynamic install cost");
}
