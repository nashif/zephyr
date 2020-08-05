/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_ARCH_X86_TIMING_H_
#define ZEPHYR_INCLUDE_ARCH_X86_TIMING_H_

#include <arch/x86/arch.h>
#include <sys_clock.h>

typedef uint64_t	timing_t;

void timing_x86_init(void);
uint64_t timing_x86_freq_get(void);

static inline void timing_init(void)
{
	timing_x86_init();
}

static inline void timing_start(void)
{
	/* Nothing to do */
}

static inline void timing_stop(void)
{
	/* Nothing to do */
}

static inline uint64_t timing_freq_get(void)
{
	return timing_x86_freq_get();
}

static inline timing_t timing_counter_get()
{
	return z_tsc_read();
}

static inline uint64_t timing_cycles_get(volatile timing_t * const start,
					 volatile timing_t * const end)
{
	return (*end - *start);
}

static inline uint64_t timing_cycles_to_ns(uint64_t cycles)
{
	return ((cycles) * NSEC_PER_SEC / timing_freq_get());
}

#endif /* ZEPHYR_INCLUDE_ARCH_X86_TIMING_H_ */
