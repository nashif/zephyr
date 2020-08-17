/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/arm/aarch32/arch.h>
#include <kernel.h>
#include <sys_clock.h>
#include <timing/timing.h>

void __weak timing_init(void)
{
}

void __weak timing_start(void)
{
}

void __weak timing_stop(void)
{
}

timing_t __weak timing_counter_get(void)
{
	return k_cycle_get_32();
}

uint64_t __weak timing_cycles_get(volatile timing_t *const start,
				  volatile timing_t *const end)
{
	return (*end - *start);
}


uint64_t __weak timing_freq_get(void)
{
	return (sys_clock_hw_cycles_per_sec() / 1000000);
}

uint64_t __weak timing_cycles_to_ns(uint64_t cycles)
{
	return (uint32_t)k_cyc_to_ns_floor64(cycles);
}

uint32_t __weak timing_freq_get_mhz(void)
{
	return (uint32_t)(timing_freq_get() / 1000000);
}
