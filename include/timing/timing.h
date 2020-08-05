/*
 * Copyright (c) 2020 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_TIMING_TIMING_H_
#define ZEPHYR_INCLUDE_TIMING_TIMING_H_

#ifdef CONFIG_TIMING_FUNCTIONS

#if defined(CONFIG_X86)
#include <arch/x86/timing.h>
#endif

/**
 * @fn static inline void timing_init(void);
 * @brief Initialize the timing subsystem.
 *
 * Perform the necessary steps to initialize the timing subsystem.
 */

/**
 * @fn static inline void timing_start(void);
 * @brief Signal the start of the timing information gathering.
 *
 * Signal to the timing subsystem that timing information
 * will be gathered from this point forward.
 */

/**
 * @fn static inline void timing_stop(void);
 * @brief Signal the end of the timing information gathering.
 *
 * Signal to the timing subsystem that timing information
 * is no longer being gathered from this point forward.
 */

/**
 * @fn static inline uint64_t timing_freq_get(void);
 * @brief Get frequency of counter used (in Hz).
 *
 * @return Frequency of counter used for timing in Hz.
 */

/**
 * @fn static inline timing_t timing_counter_get();
 * @brief Return timing counter.
 *
 * @return Timing counter.
 */

/**
 * @fn static inline uint64_t timing_cycles_get(volatile timing_t * const start, volatile timing_t * const end);
 * @brief Get number of cycles between @p start and @p end.
 *
 * For some architectures or SoCs, the raw numbers from counter
 * need to be scaled to obtain actual number of cycles.
 *
 * @param start Pointer to counter at start of a measured execution.
 * @param stop Pointer to counter at stop of a measured execution.
 * @return Number of cycles between start and end.
 */

/**
 * @fn static inline uint64_t timing_cycles_to_ns(uint64_t cycles);
 * @brief Convert number of @p cycles into nanoseconds.
 *
 * @param cycles Number of cycles
 * @return Converted time value
 */

/**
 * @brief Get frequency of counter used (in MHz).
 *
 * @return Frequency of counter used for timing in MHz.
 */
static inline uint32_t timing_freq_get_mhz(void)
{
	return (uint32_t)(timing_freq_get() / 1000000);
}

#endif /* CONFIG_TIMING_FUNCTIONS */

#endif /* ZEPHYR_INCLUDE_TIMING_TIMING_H_ */
