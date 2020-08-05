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

#if 0

#if defined(CONFIG_NRF_RTC_TIMER)

/* To get current count of timer, first 1 need to be written into
 * Capture Register and Current Count will be copied into corresponding
 * current count register.
 */
#define TIMING_INFO_PRE_READ()        (NRF_TIMER2->TASKS_CAPTURE[0] = 1)
#define TIMING_INFO_OS_GET_TIME()     (NRF_TIMER2->CC[0])
#define TIMING_INFO_GET_TIMER_VALUE() (TIMING_INFO_OS_GET_TIME())

#elif defined(CONFIG_SOC_SERIES_MEC1501X)
#define TIMING_INFO_PRE_READ()
#define TIMING_INFO_OS_GET_TIME()     (B32TMR1_REGS->CNT)
#define TIMING_INFO_GET_TIMER_VALUE() (TIMING_INFO_OS_GET_TIME())

#elif defined(CONFIG_ARC)
#define TIMING_INFO_PRE_READ()
#define TIMING_INFO_OS_GET_TIME()     (k_cycle_get_32())
#define TIMING_INFO_GET_TIMER_VALUE() (z_arc_v2_aux_reg_read(_ARC_V2_TMR0_COUNT))

#elif defined(CONFIG_NIOS2)
#include "altera_avalon_timer_regs.h"
#define TIMING_INFO_PRE_READ()         \
	(IOWR_ALTERA_AVALON_TIMER_SNAPL(TIMER_0_BASE, 10))

#define NIOS2_SUBTRACT_CLOCK_CYCLES(val)     \
	((IORD_ALTERA_AVALON_TIMER_PERIODH(TIMER_0_BASE)	\
	  << 16 |						\
	  (IORD_ALTERA_AVALON_TIMER_PERIODL(TIMER_0_BASE)))	\
	 - ((uint32_t)val))

#define TIMING_INFO_OS_GET_TIME()      (NIOS2_SUBTRACT_CLOCK_CYCLES(\
	((uint32_t)IORD_ALTERA_AVALON_TIMER_SNAPH(TIMER_0_BASE) << 16)\
	| ((uint32_t)IORD_ALTERA_AVALON_TIMER_SNAPL(TIMER_0_BASE))))

#define TIMING_INFO_GET_TIMER_VALUE()  (TIMING_INFO_OS_GET_TIME())

#else
#define TIMING_INFO_PRE_READ()
#define TIMING_INFO_OS_GET_TIME()      (k_cycle_get_32())
#define TIMING_INFO_GET_TIMER_VALUE()  (k_cycle_get_32())
#endif

#endif

#if 0
/******************************************************************************/
/* NRF RTC TIMER runs ar very slow rate (32KHz), So in order to measure
 * Kernel starts a dedicated timer to measure kernel stats.
 */
#if defined(CONFIG_NRF_RTC_TIMER)
#define NANOSECS_PER_SEC (1000000000)
#define CYCLES_PER_SEC   (16000000/(1 << NRF_TIMER2->PRESCALER))

#define CYCLES_TO_NS(x)        ((x) * (NANOSECS_PER_SEC/CYCLES_PER_SEC))
#define PRINT_STATS(x, y) \
	PRINT_F(x, (y * ((SystemCoreClock) / CYCLES_PER_SEC)), \
		CYCLES_TO_NS(y))

/* Configure Timer parameters */
static inline void benchmark_timer_init(void)
{
	NRF_TIMER2->TASKS_CLEAR = 1;	/* Clear Timer */
	NRF_TIMER2->MODE = 0;		/* Timer Mode */
	NRF_TIMER2->PRESCALER = 0;	/* 16M Hz */
	NRF_TIMER2->BITMODE = 3;	/* 32 - bit */
}

/* Stop the timer */
static inline void benchmark_timer_stop(void)
{
	NRF_TIMER2->TASKS_STOP = 1;	/* Stop Timer */
}

/*Start the timer */
static inline void benchmark_timer_start(void)
{
	NRF_TIMER2->TASKS_START = 1;	/* Start Timer */
}

/* Get Core Frequency in MHz */
static inline uint32_t get_core_freq_MHz(void)
{
	return SystemCoreClock/1000000;
}

#elif defined(CONFIG_SOC_SERIES_MEC1501X)

#define NANOSECS_PER_SEC	(1000000000)
#define CYCLES_PER_SEC		(48000000)
#define CYCLES_TO_NS(x)		((x) * (NANOSECS_PER_SEC/CYCLES_PER_SEC))
#define PRINT_STATS(x, y)	PRINT_F(x, y, CYCLES_TO_NS(y))

/* Configure Timer parameters */
static inline void benchmark_timer_init(void)
{
	/* Setup counter */
	B32TMR1_REGS->CTRL =
		MCHP_BTMR_CTRL_ENABLE |
		MCHP_BTMR_CTRL_AUTO_RESTART |
		MCHP_BTMR_CTRL_COUNT_UP;

	B32TMR1_REGS->PRLD = 0;		/* Preload */
	B32TMR1_REGS->CNT = 0;		/* Counter value */

	B32TMR1_REGS->IEN = 0;		/* Disable interrupt */
	B32TMR1_REGS->STS = 1;		/* Clear interrupt */
}

/* Stop the timer */
static inline void benchmark_timer_stop(void)
{
	B32TMR1_REGS->CTRL &= ~MCHP_BTMR_CTRL_START;
}

/* Start the timer */
static inline void benchmark_timer_start(void)
{
	B32TMR1_REGS->CTRL |= MCHP_BTMR_CTRL_START;
}

/* 48MHz counter frequency */
static inline uint32_t get_core_freq_MHz(void)
{
	return CYCLES_PER_SEC;
}

#else  /* All other architectures */
/* Done because weak attribute doesn't work on static inline. */
static inline void benchmark_timer_init(void)  {       }
static inline void benchmark_timer_stop(void)  {       }
static inline void benchmark_timer_start(void) {       }

#define CYCLES_TO_NS(x) (uint32_t)k_cyc_to_ns_floor64(x)

/* Get Core Frequency in MHz */
static inline uint32_t get_core_freq_MHz(void)
{
	return  (sys_clock_hw_cycles_per_sec() / 1000000);
}

#define PRINT_STATS(x, y)	PRINT_F(x, y, CYCLES_TO_NS(y))
#endif /* CONFIG_NRF_RTC_TIMER */

#endif

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
