/*
 * Copyright (c) 2017 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <timestamp.h>
#include <kernel_internal.h>
#include <timing/timing.h>

#define CALCULATE_CYCLES(profile, name)					\
	(timing_cycles_get(						\
	 &(profile##_##name##_start_time),				\
	 &(profile##_##name##_end_time)))

/* Stack size for all the threads created in this benchmark */
#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACKSIZE)
#define PRINT_STATS(x, y)	PRINT_F(x, (uint32_t)y, (uint32_t)timing_cycles_to_ns(y))

/******************************************************************************/
/* PRINT_F
 * Macro to print a formatted output string. fprintf is used when
 * Assumed that sline character array of SLINE_LEN + 1 characters
 * is defined in the main file
 */

/* #define CSV_FORMAT_OUTPUT */
/* printf format defines. */
#ifdef CSV_FORMAT_OUTPUT
#define FORMAT "%-45s,%8u,%8u\n"
#else
#define FORMAT "%-45s:%8u cycles , %8u ns\n"
#endif
#include <stdio.h>

#define PRINT_F(...)						     \
	{							     \
		snprintf(sline, 254, FORMAT, ##__VA_ARGS__); \
		TC_PRINT("%s", sline);			     \
	}

/******************************************************************************/
/* Function prototypes */
void system_thread_bench(void);
void yield_bench(void);
void heap_malloc_free_bench(void);
void semaphore_bench(void);
void mutex_bench(void);
void msg_passing_bench(void);
void userspace_bench(void);

#ifdef CONFIG_USERSPACE
#include <syscall_handler.h>
__syscall int k_dummy_syscall(void);
__syscall uint32_t userspace_read_timer_value(void);
__syscall int validation_overhead_syscall(void);
#include <syscalls/timing_info.h>
#endif	/* CONFIG_USERSPACE */
