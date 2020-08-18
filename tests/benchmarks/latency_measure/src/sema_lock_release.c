/*
 * Copyright (c) 2012-2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * @file measure time for sema lock and release
 *
 * This file contains the test that measures semaphore and mutex lock and
 * release time in the kernel. There is no contention on the sema nor the
 * mutex being tested.
 */

#include <zephyr.h>
#include <timing/timing.h>
#include "utils.h"
#include "timing_info.h"

#include <arch/cpu.h>

/* the number of semaphore give/take cycles */
#define N_TEST_SEMA 1000

/* the number of mutex lock/unlock cycles */
#define N_TEST_MUTEX 1000

static uint32_t timestamp_start;
static uint32_t timestamp_end;

K_SEM_DEFINE(lock_unlock_sema, 0, N_TEST_SEMA);
K_MUTEX_DEFINE(TEST_MUTEX);

/**
 *
 * @brief The function tests semaphore lock/unlock time
 *
 * The routine performs unlock the quite amount of semaphores and then
 * acquires them in order to measure the necessary time.
 *
 * @return 0 on success
 */
int sema_lock_unlock(void)
{
	int i;
	uint32_t diff;

	PRINT_FORMAT(" 3 - Measure average time to signal a semaphore then test"
		     " that semaphore");
	bench_test_start();
	timing_start();

	timestamp_start = timing_counter_get();

	for (i = 0; i < N_TEST_SEMA; i++) {
		k_sem_give(&lock_unlock_sema);
	}

	timestamp_end = timing_counter_get();
	timing_stop();

	if (bench_test_end() == 0) {
		diff = TIMING_INFO_GET_DELTA(timestamp_start, timestamp_end);
		PRINT_FORMAT(" Average semaphore signal time %u tcs = %u"
			     " nsec",
			     diff / N_TEST_SEMA,
			     (uint32_t)timing_cycles_to_ns_avg(diff, N_TEST_SEMA));
	} else {
		error_count++;
		PRINT_OVERFLOW_ERROR();
	}

	bench_test_start();
	timing_start();

	timestamp_start = timing_counter_get();

	for (i = 0; i < N_TEST_SEMA; i++) {
		k_sem_take(&lock_unlock_sema, K_FOREVER);
	}

	timestamp_end = timing_counter_get();
	timing_stop();

	if (bench_test_end() == 0) {
		diff = TIMING_INFO_GET_DELTA(timestamp_start, timestamp_end);
		PRINT_FORMAT(" Average semaphore test time %u tcs = %u "
			     "nsec",
			     diff / N_TEST_SEMA,
			     (uint32_t)timing_cycles_to_ns_avg(diff, N_TEST_SEMA));
	} else {
		error_count++;
		PRINT_OVERFLOW_ERROR();
	}
	return 0;
}

/**
 *
 * @brief Test for the multiple mutex lock/unlock time
 *
 * The routine performs multiple mutex locks and then multiple mutex
 * unlocks to measure the necessary time.
 *
 * @return 0 on success
 */
int mutex_lock_unlock(void)
{
	int i;
	uint32_t diff;

	timing_start();
	PRINT_FORMAT(" 4- Measure average time to lock a mutex then"
		     " unlock that mutex");


	timestamp_start = timing_counter_get();

	for (i = 0; i < N_TEST_MUTEX; i++) {
		k_mutex_lock(&TEST_MUTEX, K_FOREVER);
	}

	timestamp_end = timing_counter_get();

	diff = TIMING_INFO_GET_DELTA(timestamp_start, timestamp_end);
	PRINT_FORMAT(" Average time to lock the mutex %u tcs = %u nsec",
		     diff / N_TEST_MUTEX,
		     (uint32_t)timing_cycles_to_ns_avg(diff, N_TEST_MUTEX));

	timestamp_start = timing_counter_get();

	for (i = 0; i < N_TEST_MUTEX; i++) {
		k_mutex_unlock(&TEST_MUTEX);
	}

	timestamp_end = timing_counter_get();

	diff = TIMING_INFO_GET_DELTA(timestamp_start, timestamp_end);
	PRINT_FORMAT(" Average time to unlock the mutex %u tcs = %u nsec",
		     diff / N_TEST_MUTEX,
		     (uint32_t)timing_cycles_to_ns_avg(diff, N_TEST_MUTEX));
	timing_stop();
	return 0;
}
