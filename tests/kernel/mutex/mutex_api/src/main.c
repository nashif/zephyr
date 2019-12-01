/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <ztest.h>

#define TIMEOUT 500
#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACKSIZE)

/**TESTPOINT: init via K_MUTEX_DEFINE*/
K_MUTEX_DEFINE(kmutex);
static struct k_mutex mutex, mutex_nested;

static K_THREAD_STACK_DEFINE(tstack, STACK_SIZE);
static struct k_thread tdata;

#define N_THREADS 2
static K_THREAD_STACK_ARRAY_DEFINE(stacks, N_THREADS, STACK_SIZE);
static struct k_thread threads[N_THREADS];

static void thread_init_mutex(void *p1, void *p2, void *p3)
{
	int ret;

	/**TESTPOINT: test k_mutex_init on resource owned by another thread*/
	ret = k_mutex_init(&mutex);
	zassert_equal(ret, -EPERM, "returns %d", ret);
}

void test_k_mutex_init(void)
{
	zassert_true(k_mutex_init(&mutex) == 0, NULL);

	zassert_true(k_mutex_lock(&mutex, K_FOREVER) == 0, NULL);

	/**TESTPOINT: test k_mutex_init on busy resource*/
	zassert_true(k_mutex_init(&mutex) == -EBUSY, NULL);

	k_thread_create(&tdata, tstack, STACK_SIZE,
			thread_init_mutex, NULL, NULL, NULL,
			K_PRIO_PREEMPT(0),
			K_USER | K_INHERIT_PERMS, K_NO_WAIT);

	k_sleep(K_MSEC(500));
	zassert_true(k_mutex_unlock(&mutex) == 0, NULL);
}

void test_k_mutex_reset(void)
{
	int ret;

	ret = k_mutex_reset(&mutex);
	zassert_equal(ret, 0,  "returns %d", ret);

	ret = k_mutex_init(&mutex);
	zassert_equal(ret, 0,  "returns %d", ret);

	ret = k_mutex_lock(&mutex, K_FOREVER);
	zassert_equal(ret, 0,  "returns %d", ret);

	ret = k_mutex_reset(&mutex);
	zassert_equal(ret, -EBUSY,  "returns %d", ret);

	k_mutex_unlock(&mutex);

	ret = k_mutex_reset(&mutex);
	zassert_equal(ret, 0,  "returns %d", ret);

}

static void thread_mutex_unlock(void *pmutex, void *p2, void *p3)
{
	int ret;

	/**TESTPOINT: test k_mutex_unlock on resource owned by another thread*/
	ret = k_mutex_unlock((struct k_mutex *)pmutex);
	zassert_equal(ret, -EPERM, "returns %d", ret);
}

void test_k_mutex_unlock(void)
{
	int ret;

	ret = k_mutex_init(&mutex);
	zassert_equal(ret, 0,  "returns %d", ret);

	ret = k_mutex_lock(&mutex, K_FOREVER);
	zassert_equal(ret, 0,  "returns %d", ret);

	k_thread_create(&tdata, tstack, STACK_SIZE,
			thread_mutex_unlock, &mutex, NULL, NULL,
			K_PRIO_PREEMPT(0),
			K_USER | K_INHERIT_PERMS, K_NO_WAIT);

	k_sleep(K_MSEC(50));
	ret = k_mutex_unlock(&mutex);
	zassert_equal(ret, 0, "returns %d", ret);

	ret = k_mutex_reset(&mutex);
	zassert_equal(ret, 0, "returns %d", ret);
}

/**
 * @brief Basic Mutex Tests
 * @details
 * - create a mutex
 * - try to unlock a mutex that was not locked before
 * - lock the mutex
 *
 */
void test_mutex_basic(void) {
	int ret;

	/* Initialize the mutex */
	zassert_true(k_mutex_init(&mutex) == 0, NULL);

	/* Unlock mutex that was not obtained by anyone */
	ret = k_mutex_unlock(&mutex);
	zassert_true(ret == -EINVAL, "returned %d", ret);

	/* Lock the mutex */
	zassert_true(k_mutex_lock(&mutex, K_NO_WAIT) == 0, NULL);

	/* Unlock the same mutex */
	zassert_true(k_mutex_unlock(&mutex) == 0, NULL);

	/* Reset mutex */
	zassert_true(k_mutex_reset(&mutex) == 0, NULL);
}

static void thread_mutex_timeout(void *pmutex, void *p2, void *p3)
{

	zassert_true(k_mutex_lock((struct k_mutex *)pmutex, K_NO_WAIT) == 0, NULL);
	k_sleep(K_MSEC(500));
	zassert_true(k_mutex_unlock((struct k_mutex *)pmutex) == 0, NULL);
}

/**
 * @brief Test Mutex Timeout
 * @details
 * - initialize a mutex
 * - create a thread that locks a mutex but never releases it
 * - wait for the mutex to be unlocked until timeout.
 */
void test_mutex_timeout(void)
{
	int ret;

	/* Initialize the mutex */
	zassert_true(k_mutex_init(&mutex) == 0, NULL);

	k_thread_create(&tdata, tstack, STACK_SIZE,
			thread_mutex_timeout, &mutex, NULL, NULL,
			K_PRIO_PREEMPT(0),
			K_USER | K_INHERIT_PERMS, K_NO_WAIT);

	k_sleep(K_MSEC(100));

	ret = k_mutex_lock(&mutex, K_NO_WAIT);
	zassert_true(ret ==  -EBUSY, "returned %d", ret);

	ret = k_mutex_lock(&mutex, K_MSEC(300));
	zassert_true(ret ==  -EAGAIN, "returned %d", ret);

	ret = k_mutex_lock(&mutex, K_MSEC(100));
	zassert_true(ret ==  0, "returned %d", ret);

	k_mutex_unlock(&mutex);

	/* Reset mutex */
	zassert_true(k_mutex_reset(&mutex) == 0, NULL);
}

static void recursive_lock(u32_t depth, u32_t control)
{
	static u32_t acq;
	int ret;

	ret = k_mutex_lock(&mutex_nested, K_FOREVER);
	zassert_equal(ret, 0, NULL);

	if (control == depth) {
		zassert_equal(acq, 0, NULL);
	}
	acq++;
	if (depth) {
		recursive_lock(depth - 1, control);

	}
	acq--;

	ret = k_mutex_unlock(&mutex_nested);
	zassert_equal(ret, 0, NULL);
}

static void thread_mutex_nested(void *p1, void *p2, void *p3)
{
	recursive_lock(3, 3);
}


void test_mutex_nested(void)
{
	int ret;

	ret = k_mutex_init(&mutex_nested);
	zassert_equal(ret, 0, NULL);

	ret = k_mutex_lock(&mutex_nested, K_NO_WAIT);
	zassert_equal(ret, 0, NULL);


	k_thread_create(&tdata, tstack, STACK_SIZE,
		thread_mutex_nested, NULL, NULL, NULL,
		K_PRIO_PREEMPT(10),
		K_USER | K_INHERIT_PERMS, K_NO_WAIT);

	recursive_lock(5, 5);

	ret = k_mutex_unlock(&mutex_nested);
	zassert_equal(ret, 0, "returned %d", ret);

	k_sleep(K_MSEC(100));

	ret = k_mutex_unlock(&mutex_nested);
	zassert_equal(ret, -EINVAL, "returned %d", ret);
}


void test_mutex_priority_inversion(void)
{

}

static void thread_low(void *pmutex, void *p2, void *p3)
{
	zassert_true(k_mutex_lock((struct k_mutex *)pmutex, K_NO_WAIT) == 0, NULL);
	k_sleep(500);
	zassert_true(k_mutex_unlock((struct k_mutex *)pmutex) == 0, NULL);
}

static void thread_high(void *pmutex, void *p2, void *p3)
{
	zassert_true(k_mutex_unlock((struct k_mutex *)pmutex) == -EPERM, NULL);
}

void test_mutex_ownership(void)
{
	int ret;

	ret = k_mutex_init(&mutex);
	zassert_true(ret ==  0, "returned %d", ret);

	k_thread_create(&threads[0], stacks[0], STACK_SIZE,
		thread_low, &mutex, NULL, NULL,
		K_PRIO_PREEMPT(10),
		K_USER | K_INHERIT_PERMS, K_NO_WAIT);
	k_sleep(200);
	k_thread_create(&threads[1], stacks[1], STACK_SIZE,
		thread_high, &mutex, NULL, NULL,
		K_PRIO_PREEMPT(8),
		K_USER | K_INHERIT_PERMS, K_NO_WAIT);

	k_sleep(500);
}

/*test case main entry*/
void test_main(void)
{
	k_thread_access_grant(k_current_get(),
			      &tdata,
			      &tstack,
			      &kmutex,
			      &threads[0], &stacks[0],
			      &threads[1], &stacks[1],
			      &mutex,
			      &mutex_nested);

	ztest_test_suite(mutex_api,
			 ztest_1cpu_user_unit_test(test_k_mutex_init),
			 ztest_1cpu_user_unit_test(test_k_mutex_reset),
			 ztest_1cpu_user_unit_test(test_k_mutex_unlock),
			 ztest_1cpu_user_unit_test(test_mutex_basic),
			 ztest_1cpu_user_unit_test(test_mutex_nested),
			 ztest_1cpu_user_unit_test(test_mutex_timeout),
			 ztest_1cpu_user_unit_test(test_mutex_ownership)
			 );
	ztest_run_test_suite(mutex_api);
}
