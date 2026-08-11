/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_TESTS_BENCHMARKS_INTERRUPT_LATENCY_SRC_BENCH_H_
#define ZEPHYR_TESTS_BENCHMARKS_INTERRUPT_LATENCY_SRC_BENCH_H_

#include <zephyr/kernel.h>
#include <zephyr/timing/timing.h>

/*
 * Sample recording and reporting.
 *
 * Each scenario resets the sample buffer, records one cycle count per
 * iteration and finally reports min/avg/max/p99 statistics as one line
 * per statistic in the same "metric - description : cycles , ns" format
 * used by the latency_measure benchmark, so that twister can harvest
 * the values with a "record:" regex.
 */
void bench_samples_reset(void);
void bench_sample_add(uint64_t cycles);
void bench_report(const char *metric, const char *description);
void bench_report_avg(const char *metric, const char *description,
		      uint64_t total_cycles, uint32_t count);

/*
 * Measure the average overhead of one timing_counter_get() call so
 * that scenarios can subtract it from measured deltas.
 */
void bench_calibrate(void);
uint64_t bench_adjust(uint64_t cycles);

/* Scenario entry points */
void int_bench_entry(void);
void int_bench_exit(void);
void int_bench_locked(void);
void int_bench_dynamic(void);
void int_bench_throughput(void);

extern int bench_error_count;

#endif /* ZEPHYR_TESTS_BENCHMARKS_INTERRUPT_LATENCY_SRC_BENCH_H_ */
