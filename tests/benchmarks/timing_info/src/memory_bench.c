/*
 * Copyright (c) 2013-2015 Wind River Systems, Inc.
 * Copyright (c) 2016,2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <zephyr.h>
#include <tc_util.h>
#include <ksched.h>
#include "timing_info.h"

extern char sline[];

void heap_malloc_free_bench(void)
{
	uint64_t total_cycles;

	/* heap malloc*/
	timing_t heap_malloc_start_time = 0U;
	timing_t heap_malloc_end_time = 0U;

	/* heap free*/
	timing_t heap_free_start_time = 0U;
	timing_t heap_free_end_time = 0U;

	uint32_t count = 0;
	uint32_t sum_malloc = 0U;
	uint32_t sum_free = 0U;

	k_sleep(K_MSEC(10));
	while (count++ != 100) {
		heap_malloc_start_time = timing_counter_get();

		void *allocated_mem = k_malloc(10);

		heap_malloc_end_time = timing_counter_get();

		if (allocated_mem == NULL) {
			TC_PRINT("\n Malloc failed at count %d\n", count);
			break;
		}

		heap_free_start_time = timing_counter_get();

		k_free(allocated_mem);

		heap_free_end_time = timing_counter_get();

		sum_malloc += CALCULATE_CYCLES(heap, malloc);
		sum_free += CALCULATE_CYCLES(heap, free);
	}

	total_cycles = sum_malloc / count;
	PRINT_STATS("Heap malloc", total_cycles);

	total_cycles = sum_free / count;
	PRINT_STATS("Heap free", total_cycles);
}
