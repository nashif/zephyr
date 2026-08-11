/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include "bench.h"

static uint32_t samples[CONFIG_INT_BENCH_NUM_ITERATIONS];
static uint32_t sample_count;
static uint64_t timestamp_overhead;

void bench_calibrate(void)
{
	timing_t start;
	timing_t finish;
	uint64_t sum = 0ULL;
	const uint32_t loops = 1000U;

	for (uint32_t i = 0U; i < loops; i++) {
		start = timing_counter_get();
		finish = timing_counter_get();
		sum += timing_cycles_get(&start, &finish);
	}

	timestamp_overhead = sum / loops;
}

uint64_t bench_adjust(uint64_t cycles)
{
	return (cycles > timestamp_overhead) ? (cycles - timestamp_overhead) : 0ULL;
}

void bench_samples_reset(void)
{
	sample_count = 0U;
}

void bench_sample_add(uint64_t cycles)
{
	if (sample_count < ARRAY_SIZE(samples)) {
		samples[sample_count] = (cycles > UINT32_MAX) ? UINT32_MAX : (uint32_t)cycles;
		sample_count++;
	}
}

static void sort_samples(void)
{
	/* Shell sort; small fixed data set, no libc dependency */
	for (uint32_t gap = sample_count / 2U; gap > 0U; gap /= 2U) {
		for (uint32_t i = gap; i < sample_count; i++) {
			uint32_t val = samples[i];
			uint32_t j = i;

			while ((j >= gap) && (samples[j - gap] > val)) {
				samples[j] = samples[j - gap];
				j -= gap;
			}
			samples[j] = val;
		}
	}
}

static void print_line(const char *metric, const char *stat, const char *description,
		       uint64_t cycles, uint64_t nsec)
{
	char summary[96];

	snprintk(summary, sizeof(summary), "%s.%s - %s (%s)", metric, stat, description, stat);
	printk("%-94s:%8u cycles , %8u ns :\n", summary, (uint32_t)cycles, (uint32_t)nsec);
}

void bench_report(const char *metric, const char *description)
{
	uint64_t sum = 0ULL;
	uint32_t min;
	uint32_t max;
	uint32_t p99;

	if (sample_count == 0U) {
		printk("%s - %s: no samples recorded\n", metric, description);
		bench_error_count++;
		return;
	}

	sort_samples();

	min = samples[0];
	max = samples[sample_count - 1U];
	p99 = samples[((sample_count * 99U) / 100U < sample_count) ?
		      ((sample_count * 99U) / 100U) : (sample_count - 1U)];

	for (uint32_t i = 0U; i < sample_count; i++) {
		sum += samples[i];
	}

	print_line(metric, "min", description, min, timing_cycles_to_ns(min));
	print_line(metric, "avg", description, sum / sample_count,
		   timing_cycles_to_ns_avg(sum, sample_count));
	print_line(metric, "max", description, max, timing_cycles_to_ns(max));
	print_line(metric, "p99", description, p99, timing_cycles_to_ns(p99));
}

void bench_report_avg(const char *metric, const char *description,
		      uint64_t total_cycles, uint32_t count)
{
	if (count == 0U) {
		bench_error_count++;
		return;
	}

	print_line(metric, "avg", description, total_cycles / count,
		   timing_cycles_to_ns_avg(total_cycles, count));
}
