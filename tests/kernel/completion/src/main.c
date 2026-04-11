/*
 * Copyright (c) 2024 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/irq_offload.h>
#include <zephyr/kernel.h>

#define STACK_SIZE (512 + CONFIG_TEST_EXTRA_STACK_SIZE)
#define COMPLETION_TIMEOUT K_MSEC(100)

/**
 * @defgroup kernel_completion_tests Completion objects
 * @ingroup all_tests
 * @{
 * @}
 */

/* ------------------------------------------------------------------ */
/* Helpers shared across tests                                         */
/* ------------------------------------------------------------------ */

static K_THREAD_STACK_DEFINE(tstack, STACK_SIZE);
static struct k_thread tdata;
static struct k_completion test_completion;

/* Thread that calls k_completion_complete() and then exits. */
static void complete_thread(void *p1, void *p2, void *p3)
{
	struct k_completion *c = (struct k_completion *)p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_completion_complete(c);
}

/* Thread that calls k_completion_complete_all() and then exits. */
static void complete_all_thread(void *p1, void *p2, void *p3)
{
	struct k_completion *c = (struct k_completion *)p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_completion_complete_all(c);
}

/* ISR callback that calls k_completion_complete(). */
static void isr_complete(const void *arg)
{
	k_completion_complete((struct k_completion *)arg);
}

/* ISR callback that calls k_completion_complete_all(). */
static void isr_complete_all(const void *arg)
{
	k_completion_complete_all((struct k_completion *)arg);
}

/* ------------------------------------------------------------------ */
/* Test: init / basic wait – complete before wait                      */
/* ------------------------------------------------------------------ */

ZTEST(completion, test_complete_before_wait)
{
	k_completion_init(&test_completion);

	/* Signal first, then wait – must return immediately (no block). */
	k_completion_complete(&test_completion);
	zassert_equal(k_completion_wait(&test_completion, K_NO_WAIT), 0,
		      "expected 0 when completion is already done");
}

/* ------------------------------------------------------------------ */
/* Test: wait before complete (blocking path)                          */
/* ------------------------------------------------------------------ */

ZTEST(completion, test_wait_before_complete)
{
	k_completion_init(&test_completion);

	k_thread_create(&tdata, tstack, STACK_SIZE,
			complete_thread, &test_completion, NULL, NULL,
			K_PRIO_PREEMPT(0), 0, K_MSEC(50));

	zassert_equal(k_completion_wait(&test_completion, K_FOREVER), 0,
		      "wait should succeed after complete()");

	k_thread_join(&tdata, K_FOREVER);
}

/* ------------------------------------------------------------------ */
/* Test: timeout                                                       */
/* ------------------------------------------------------------------ */

ZTEST(completion, test_wait_timeout)
{
	k_completion_init(&test_completion);

	int ret = k_completion_wait(&test_completion, COMPLETION_TIMEOUT);

	zassert_equal(ret, -EAGAIN, "expected -EAGAIN on timeout, got %d", ret);
}

/* ------------------------------------------------------------------ */
/* Test: K_NO_WAIT when not done                                       */
/* ------------------------------------------------------------------ */

ZTEST(completion, test_wait_no_wait_not_done)
{
	k_completion_init(&test_completion);

	int ret = k_completion_wait(&test_completion, K_NO_WAIT);

	zassert_equal(ret, -EBUSY,
		      "expected -EBUSY for K_NO_WAIT on unsignalled completion, got %d", ret);
}

/* ------------------------------------------------------------------ */
/* Test: counter increments – multiple complete() / wait() pairs      */
/* ------------------------------------------------------------------ */

ZTEST(completion, test_counter_increments)
{
	k_completion_init(&test_completion);

	/* Three signals, three successful waits. */
	k_completion_complete(&test_completion);
	k_completion_complete(&test_completion);
	k_completion_complete(&test_completion);

	zassert_equal(k_completion_wait(&test_completion, K_NO_WAIT), 0,
		      "1st wait");
	zassert_equal(k_completion_wait(&test_completion, K_NO_WAIT), 0,
		      "2nd wait");
	zassert_equal(k_completion_wait(&test_completion, K_NO_WAIT), 0,
		      "3rd wait");

	/* Counter should be zero now. */
	zassert_equal(k_completion_wait(&test_completion, K_NO_WAIT), -EBUSY,
		      "counter should be exhausted");
}

/* ------------------------------------------------------------------ */
/* Test: complete_all() wakes all waiters immediately                  */
/* ------------------------------------------------------------------ */

#define N_WAITERS 4

static struct k_completion multi_completion;
static K_THREAD_STACK_ARRAY_DEFINE(multi_stacks, N_WAITERS, STACK_SIZE);
static struct k_thread multi_threads[N_WAITERS];
static atomic_t woken_count;

static void waiter_thread(void *p1, void *p2, void *p3)
{
	struct k_completion *c = (struct k_completion *)p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	if (k_completion_wait(c, K_FOREVER) == 0) {
		atomic_inc(&woken_count);
	}
}

ZTEST(completion, test_complete_all_wakes_all)
{
	k_completion_init(&multi_completion);
	atomic_set(&woken_count, 0);

	/* Spawn N_WAITERS threads, all pending on the same completion. */
	for (int i = 0; i < N_WAITERS; i++) {
		k_thread_create(&multi_threads[i], multi_stacks[i], STACK_SIZE,
				waiter_thread, &multi_completion, NULL, NULL,
				K_PRIO_PREEMPT(0), 0, K_NO_WAIT);
	}

	/* Give threads time to start and pend. */
	k_sleep(K_MSEC(20));

	k_completion_complete_all(&multi_completion);

	/* Join all and verify all woke. */
	for (int i = 0; i < N_WAITERS; i++) {
		k_thread_join(&multi_threads[i], K_FOREVER);
	}

	zassert_equal(atomic_get(&woken_count), N_WAITERS,
		      "all %d waiters should have been woken, got %" PRIdPTR,
		      N_WAITERS, atomic_get(&woken_count));
}

/* ------------------------------------------------------------------ */
/* Test: complete_all() – subsequent waits pass through (UINT_MAX)    */
/* ------------------------------------------------------------------ */

ZTEST(completion, test_complete_all_passthrough)
{
	k_completion_init(&multi_completion);
	k_completion_complete_all(&multi_completion);

	/* Subsequent waits must pass through immediately. */
	for (int i = 0; i < 5; i++) {
		zassert_equal(k_completion_wait(&multi_completion, K_NO_WAIT),
			      0, "wait %d should pass through after complete_all", i);
	}
}

/* ------------------------------------------------------------------ */
/* Test: reset aborts waiters and clears done state                   */
/* ------------------------------------------------------------------ */

static struct k_completion reset_completion;
static K_THREAD_STACK_DEFINE(reset_stack, STACK_SIZE);
static struct k_thread reset_thread;
static int wait_result;

static void waiting_thread(void *p1, void *p2, void *p3)
{
	struct k_completion *c = (struct k_completion *)p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	wait_result = k_completion_wait(c, K_FOREVER);
}

ZTEST(completion, test_reset_aborts_waiters)
{
	k_completion_init(&reset_completion);

	k_thread_create(&reset_thread, reset_stack, STACK_SIZE,
			waiting_thread, &reset_completion, NULL, NULL,
			K_PRIO_PREEMPT(0), 0, K_NO_WAIT);

	k_sleep(K_MSEC(20));
	k_completion_reset(&reset_completion);
	k_thread_join(&reset_thread, K_FOREVER);

	zassert_equal(wait_result, -EAGAIN,
		      "reset should abort waiters with -EAGAIN, got %d", wait_result);

	/* After reset, completion is not done. */
	zassert_equal(k_completion_wait(&reset_completion, K_NO_WAIT), -EBUSY,
		      "completion should not be done after reset");
}

ZTEST(completion, test_reset_clears_done)
{
	k_completion_init(&test_completion);
	k_completion_complete(&test_completion);

	/* Reset should clear the done state. */
	k_completion_reset(&test_completion);
	zassert_equal(k_completion_wait(&test_completion, K_NO_WAIT), -EBUSY,
		      "completion should not be done after reset");
}

/* ------------------------------------------------------------------ */
/* Test: ISR calling complete()                                        */
/* ------------------------------------------------------------------ */

ZTEST(completion, test_complete_from_isr)
{
	k_completion_init(&test_completion);

	irq_offload(isr_complete, &test_completion);

	zassert_equal(k_completion_wait(&test_completion, K_NO_WAIT), 0,
		      "ISR complete() should make wait() succeed");
}

/* ------------------------------------------------------------------ */
/* Test: ISR calling complete_all()                                    */
/* ------------------------------------------------------------------ */

ZTEST(completion, test_complete_all_from_isr)
{
	k_completion_init(&test_completion);

	irq_offload(isr_complete_all, &test_completion);

	zassert_equal(k_completion_wait(&test_completion, K_NO_WAIT), 0,
		      "ISR complete_all() should make wait() succeed");
	/* Second wait – still passes through because done == UINT_MAX. */
	zassert_equal(k_completion_wait(&test_completion, K_NO_WAIT), 0,
		      "second wait after ISR complete_all() should also succeed");
}

/* ------------------------------------------------------------------ */
/* Test: static initialisation with K_COMPLETION_DEFINE               */
/* ------------------------------------------------------------------ */

K_COMPLETION_DEFINE(static_completion);

ZTEST(completion, test_static_define)
{
	/* A statically defined completion starts in the undone state. */
	zassert_equal(k_completion_wait(&static_completion, K_NO_WAIT), -EBUSY,
		      "static completion should start undone");

	k_completion_complete(&static_completion);
	zassert_equal(k_completion_wait(&static_completion, K_NO_WAIT), 0,
		      "static completion should be done after complete()");
}

/* ------------------------------------------------------------------ */
/* Test: reuse after reset                                             */
/* ------------------------------------------------------------------ */

ZTEST(completion, test_reuse_after_reset)
{
	k_completion_init(&test_completion);

	k_completion_complete_all(&test_completion);
	k_completion_reset(&test_completion);

	/* Spawn a thread that will signal; we should block until it does. */
	k_thread_create(&tdata, tstack, STACK_SIZE,
			complete_thread, &test_completion, NULL, NULL,
			K_PRIO_PREEMPT(0), 0, K_MSEC(20));

	zassert_equal(k_completion_wait(&test_completion, K_FOREVER), 0,
		      "completion should work after reset");

	k_thread_join(&tdata, K_FOREVER);
}

/* ------------------------------------------------------------------ */
/* Test: complete_all() then reset then complete_all() again          */
/* ------------------------------------------------------------------ */

ZTEST(completion, test_complete_all_reset_complete_all)
{
	k_completion_init(&test_completion);

	k_completion_complete_all(&test_completion);
	k_completion_reset(&test_completion);
	k_completion_complete_all(&test_completion);

	/* Should pass through again. */
	zassert_equal(k_completion_wait(&test_completion, K_NO_WAIT), 0,
		      "second complete_all after reset should work");
}

/* ------------------------------------------------------------------ */
/* Suite setup / teardown                                              */
/* ------------------------------------------------------------------ */

static void *completion_setup(void)
{
	return NULL;
}

static void completion_before(void *fixture)
{
	ARG_UNUSED(fixture);
	k_completion_init(&test_completion);
}

ZTEST_SUITE(completion, NULL, completion_setup, completion_before, NULL, NULL);
