/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/x86/arch.h>
#include <kernel.h>
#include <sys_clock.h>

static uint64_t tsc_freq;

void timing_x86_init(void)
{
	uint32_t cyc_start = k_cycle_get_32();
	uint64_t tsc_start = z_tsc_read();

	k_busy_wait(10 * USEC_PER_MSEC);

	uint32_t cyc_end = k_cycle_get_32();
	uint64_t tsc_end = z_tsc_read();

	uint64_t cyc_freq = sys_clock_hw_cycles_per_sec();

	/*
	 * cycles are in 32-bit, and delta must be
	 * calculated in 32-bit percision. Or it would
	 * wrapping around in 64-bit.
	 */
	uint64_t dcyc = (uint32_t)cyc_end - (uint32_t)cyc_start;

	uint64_t dtsc = tsc_end - tsc_start;

	tsc_freq = (cyc_freq * dtsc) / dcyc;
}

uint64_t timing_x86_freq_get(void)
{
	return tsc_freq;
}
