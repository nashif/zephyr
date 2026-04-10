/*
 * Copyright (c) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>

#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACK_SIZE)
#define NOTIFY_VAL 0xDEADBEEFU

static struct k_thread thelper;
static K_THREAD_STACK_DEFINE(shelper, STACK_SIZE);

/* -------------------------------------------------------------------------
 * Helper thread functions
 * -------------------------------------------------------------------------
 */

/** Sends a SET_BITS notification to the thread passed as p1 */
static void helper_set_bits(void *p1, void *p2, void *p3)
{
	k_tid_t target = (k_tid_t)p1;

	k_sleep(K_MSEC(50));
	k_thread_notify(target, NOTIFY_VAL, K_THREAD_NOTIFY_SET_BITS);
}

/** Sends an INCREMENT notification to the thread passed as p1 */
static void helper_increment(void *p1, void *p2, void *p3)
{
	k_tid_t target = (k_tid_t)p1;

	k_sleep(K_MSEC(50));
	k_thread_notify_give(target);
}

static void helper_increment_twice(void *p1, void *p2, void *p3)
{
	k_tid_t target = (k_tid_t)p1;

	k_sleep(K_MSEC(50));
	k_thread_notify_give(target);
	k_thread_notify_give(target);
}

/** Sends an overwrite notification to the thread passed as p1 */
static void helper_overwrite(void *p1, void *p2, void *p3)
{
	k_tid_t target = (k_tid_t)p1;

	k_sleep(K_MSEC(50));
	k_thread_notify(target, 0xCAFECAFEU,
			K_THREAD_NOTIFY_SET_VALUE_OVERWRITE);
}

static void start_helper(k_thread_entry_t fn, void *arg)
{
	k_thread_create(&thelper, shelper, STACK_SIZE,
			fn, arg, NULL, NULL,
			K_PRIO_PREEMPT(1), 0, K_NO_WAIT);
}

/* -------------------------------------------------------------------------
 * Tests
 * -------------------------------------------------------------------------
 */

/**
 * @brief Basic wait and notify with SET_BITS action
 *
 * @ingroup kernel_thread_notify_tests
 */
ZTEST(thread_notify, test_notify_set_bits)
{
	uint32_t val = 0U;

	k_thread_notify_clear();

	start_helper(helper_set_bits, k_current_get());

	int ret = k_thread_notify_wait(0U, 0U, &val, K_MSEC(500));

	k_thread_join(&thelper, K_FOREVER);

	zassert_equal(ret, 0, "expected 0 got %d", ret);
	zassert_equal(val, NOTIFY_VAL,
		      "expected 0x%08X got 0x%08X", NOTIFY_VAL, val);
}

/**
 * @brief Notification received before wait (already pending)
 *
 * @ingroup kernel_thread_notify_tests
 */
ZTEST(thread_notify, test_notify_already_pending)
{
	uint32_t val = 0U;

	k_thread_notify_clear();

	/* Post before we call wait */
	k_thread_notify(k_current_get(), 0x42U, K_THREAD_NOTIFY_SET_BITS);

	int ret = k_thread_notify_wait(0U, UINT32_MAX, &val, K_NO_WAIT);

	zassert_equal(ret, 0, "expected 0 got %d", ret);
	zassert_equal(val, 0x42U, "expected 0x42 got 0x%08X", val);

	/* bits_clear_on_exit=UINT32_MAX: notification value should be 0 now */
	zassert_equal(k_thread_notify_value_get(k_current_get()), 0U,
		      "notify value should be cleared");
}

/**
 * @brief Timeout when no notification arrives
 *
 * @ingroup kernel_thread_notify_tests
 */
ZTEST(thread_notify, test_notify_timeout)
{
	k_thread_notify_clear();

	int ret = k_thread_notify_wait(0U, 0U, NULL, K_MSEC(50));

	zassert_equal(ret, -EAGAIN, "expected -EAGAIN got %d", ret);
}

/**
 * @brief K_NO_WAIT when nothing is pending
 *
 * @ingroup kernel_thread_notify_tests
 */
ZTEST(thread_notify, test_notify_no_wait_empty)
{
	k_thread_notify_clear();

	int ret = k_thread_notify_wait(0U, 0U, NULL, K_NO_WAIT);

	zassert_equal(ret, -EAGAIN, "expected -EAGAIN got %d", ret);
}

/**
 * @brief k_thread_notify_take / k_thread_notify_give lightweight semaphore
 *
 * @ingroup kernel_thread_notify_tests
 */
ZTEST(thread_notify, test_notify_give_take)
{
	k_thread_notify_clear();

	start_helper(helper_increment, k_current_get());

	int ret = k_thread_notify_take(false, K_MSEC(500));

	k_thread_join(&thelper, K_FOREVER);

	zassert_equal(ret, 0, "expected 0 got %d", ret);
}

/**
 * @brief Multiple gives before take: count is correctly decremented
 *
 * @ingroup kernel_thread_notify_tests
 */
ZTEST(thread_notify, test_notify_take_pending_count)
{
	k_thread_notify_clear();

	/* Give twice before we take */
	k_thread_notify_give(k_current_get());
	k_thread_notify_give(k_current_get());

	zassert_equal(k_thread_notify_take(false, K_NO_WAIT), 0,
		      "first take");
	zassert_equal(k_thread_notify_take(false, K_NO_WAIT), 0,
		      "second take");
	zassert_equal(k_thread_notify_take(false, K_NO_WAIT), -EAGAIN,
		      "third take should timeout");
}

/**
 * @brief SET_VALUE without overwrite fails when already pending
 *
 * @ingroup kernel_thread_notify_tests
 */
ZTEST(thread_notify, test_notify_set_value_no_overwrite_busy)
{
	k_thread_notify_clear();

	k_thread_notify(k_current_get(), 0x1U, K_THREAD_NOTIFY_SET_VALUE);
	int ret = k_thread_notify(k_current_get(), 0x2U,
				  K_THREAD_NOTIFY_SET_VALUE);

	zassert_equal(ret, -EBUSY, "expected -EBUSY got %d", ret);

	/* Value should still be the original 0x1 */
	zassert_equal(k_thread_notify_value_get(k_current_get()), 0x1U,
		      "value should be unchanged");

	/* Drain the pending notification */
	k_thread_notify_take(true, K_NO_WAIT);
}

/**
 * @brief SET_VALUE_OVERWRITE unconditionally replaces the value
 *
 * @ingroup kernel_thread_notify_tests
 */
ZTEST(thread_notify, test_notify_set_value_overwrite)
{
	uint32_t val = 0U;

	k_thread_notify_clear();

	start_helper(helper_overwrite, k_current_get());

	int ret = k_thread_notify_wait(0U, UINT32_MAX, &val, K_MSEC(500));

	k_thread_join(&thelper, K_FOREVER);

	zassert_equal(ret, 0, "expected 0 got %d", ret);
	zassert_equal(val, 0xCAFECAFEU,
		      "expected 0xCAFECAFE got 0x%08X", val);
}

/**
 * @brief bits_clear_on_entry masks the value before waiting
 *
 * @ingroup kernel_thread_notify_tests
 */
ZTEST(thread_notify, test_notify_bits_clear_on_entry)
{
	uint32_t val = 0U;

	k_thread_notify_clear();

	/* Pre-set a value, then clear some bits on entry */
	k_thread_notify(k_current_get(), 0xFFU, K_THREAD_NOTIFY_SET_BITS);

	/* Clear lower nibble on entry, collect remaining bits */
	int ret = k_thread_notify_wait(0x0FU, 0U, &val, K_NO_WAIT);

	zassert_equal(ret, 0, "expected 0 got %d", ret);
	zassert_equal(val, 0xF0U,
		      "expected 0xF0 got 0x%08X", val);
}

/**
 * @brief k_thread_notify_clear resets both value and pending count
 *
 * @ingroup kernel_thread_notify_tests
 */
ZTEST(thread_notify, test_notify_clear)
{
	k_thread_notify(k_current_get(), 0xABCDU, K_THREAD_NOTIFY_SET_BITS);
	k_thread_notify_give(k_current_get());

	k_thread_notify_clear();

	zassert_equal(k_thread_notify_value_get(k_current_get()), 0U,
		      "value should be 0 after clear");
	zassert_equal(k_thread_notify_take(false, K_NO_WAIT), -EAGAIN,
		      "pending count should be 0 after clear");
}

/**
 * @brief NULL thread pointer returns -EINVAL
 *
 * @ingroup kernel_thread_notify_tests
 */
ZTEST(thread_notify, test_notify_null_thread)
{
	int ret = k_thread_notify(NULL, 0U, K_THREAD_NOTIFY_INCREMENT);

	zassert_equal(ret, -EINVAL, "expected -EINVAL got %d", ret);
}

ZTEST_SUITE(thread_notify, NULL, NULL, NULL, NULL, NULL);
