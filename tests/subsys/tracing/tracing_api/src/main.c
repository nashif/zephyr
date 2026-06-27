/*
 * Copyright (c) 2021 Intel Corporation+
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <tracing_buffer.h>
#include <tracing_core.h>
#include <zephyr/tracing/tracing_format.h>
#if defined(CONFIG_TRACING_BACKEND_UART)
#include "../../../../subsys/tracing/include/tracing_backend.h"
#endif

/**
 * @brief Tests for tracing
 * @defgroup tracing_api_tests Tracing
 * @ingroup all_tests
 * @{
 * @}
 */

/* Check flags */
static bool data_format_found;
static bool raw_data_format_found;
static bool sync_string_format_found;

/*
 * The tracing API hooks are emitted as strings by the test format backend
 * (CONFIG_TRACING_TEST). The list below is verified against the backend output
 * in both synchronous and asynchronous mode: in async mode the tracing thread
 * delivers a single buffer holding everything, while in sync mode each hook is
 * delivered as its own chunk. Marking each entry as it is seen handles both.
 */
static const char *const string_tracked[] = {
	"sys_trace_k_thread_switched_out", "sys_trace_k_thread_switched_in",
	"sys_trace_k_thread_priority_set", "sys_trace_k_thread_sched_set_priority",
	"sys_trace_k_thread_create", "sys_trace_k_thread_start",
	"sys_trace_k_thread_abort", "sys_trace_k_thread_suspend",
	"sys_trace_k_thread_resume", "sys_trace_k_thread_ready",
	"sys_trace_k_thread_sched_ready", "sys_trace_k_thread_sched_abort",
	"sys_trace_k_thread_sched_resume", "sys_trace_k_thread_sched_suspend",
	"sys_trace_k_thread_sleep_enter", "sys_trace_k_thread_sleep_exit",
	"sys_trace_k_thread_abort_enter", "sys_trace_k_thread_abort_exit",
	"sys_trace_k_thread_yield", "sys_trace_k_thread_wakeup",
	"sys_trace_k_thread_pend", "sys_trace_k_thread_info",
	"sys_trace_k_thread_name_set", "sys_trace_k_thread_sched_lock",
	"sys_trace_k_timer_init", "sys_trace_k_thread_join_blocking",
	"sys_trace_k_thread_join_exit", "sys_trace_isr_enter",
	"sys_trace_isr_exit", "sys_trace_idle",
	"sys_trace_k_condvar_broadcast_enter", "sys_trace_k_condvar_broadcast_exit",
	"sys_trace_k_condvar_init", "sys_trace_k_condvar_signal_enter",
	"sys_trace_k_condvar_signal_blocking", "sys_trace_k_condvar_signal_exit",
	"sys_trace_k_condvar_wait_enter", "sys_trace_k_condvar_wait_exit",
	"sys_trace_k_sem_init", "sys_trace_k_sem_give_enter",
	"sys_trace_k_sem_take_enter", "sys_trace_k_sem_take_exit",
	"sys_trace_k_sem_take_blocking", "sys_trace_k_mutex_init",
	"sys_trace_k_mutex_lock_enter", "sys_trace_k_mutex_lock_exit",
	"sys_trace_k_mutex_lock_blocking", "sys_trace_k_mutex_unlock_enter",
	"sys_trace_k_mutex_unlock_exit", "sys_trace_k_timer_start", NULL
};

static bool api_found[ARRAY_SIZE(string_tracked)];
static bool capturing;

#if defined(CONFIG_TRACING_BACKEND_UART)
static void tracing_backends_output(
		const struct tracing_backend *backend,
		uint8_t *data, uint32_t length)
{
	/* Check the output data. */
	if (capturing) {
		for (int i = 0; string_tracked[i] != NULL; i++) {
			if (!api_found[i] &&
			    strstr((char *)data, string_tracked[i]) != NULL) {
				api_found[i] = true;
			}
		}
	}

	if (strstr((char *)data, "tracing_format_data_testing") != NULL) {
		data_format_found = true;
	}
	if (strstr((char *)data, "tracing_format_raw_data_testing") != NULL) {
		raw_data_format_found = true;
	}
	if (strstr((char *)data, "tracing_format_string_testing") != NULL) {
		sync_string_format_found = true;
	}
}

const struct tracing_backend_api tracing_uart_backend_api = {
	.init = NULL,
	.output  = tracing_backends_output
};

TRACING_BACKEND_DEFINE(tracing_backend_uart, tracing_uart_backend_api);
#endif

/**
 * @brief Test tracing APIs
 *
 * @details Call the tracing API hooks one by one and check that each one
 * reaches the self-designed tracing backend. This runs in both synchronous and
 * asynchronous mode.
 *
 * @ingroup tracing_api_tests
 */
ZTEST(tracing_api, test_tracing_sys_api)
{
	int ret = 0, prio = 0;
	size_t stack = 0;
	struct k_mutex mutex;
	struct k_thread thread;
	struct k_condvar condvar;
	struct k_sem sem, sem2;
	struct k_timer timer;
	k_timeout_t timeout = K_MSEC(1);

	tracing_buffer_init();
	memset(api_found, 0, sizeof(api_found));
	capturing = true;
	/* thread api */
	sys_trace_k_thread_switched_out();
	sys_trace_k_thread_switched_in();
	sys_trace_k_thread_priority_set(&thread);
	sys_trace_k_thread_sched_set_priority(&thread, prio);
	sys_trace_k_thread_create(&thread, stack, prio);
	sys_trace_k_thread_start(&thread);
	sys_trace_k_thread_abort(&thread);
	sys_trace_k_thread_suspend(&thread);
	sys_trace_k_thread_resume(&thread);
	sys_trace_k_thread_ready(&thread);
	sys_trace_k_thread_sched_ready(&thread);
	sys_trace_k_thread_sched_abort(&thread);
	sys_trace_k_thread_sched_resume(&thread);
	sys_trace_k_thread_sched_suspend(&thread);
	sys_trace_k_thread_sleep_enter(timeout);
	sys_trace_k_thread_sleep_exit(timeout, ret);
	sys_trace_k_thread_abort_enter(&thread);
	sys_trace_k_thread_abort_exit(&thread);
	sys_trace_k_thread_yield();
	sys_trace_k_thread_wakeup(&thread);
	sys_trace_k_thread_pend(&thread);
	sys_trace_k_thread_info(&thread);
	sys_trace_k_thread_name_set(&thread, ret);
	sys_trace_k_thread_sched_lock();
	sys_port_trace_k_thread_sched_unlock();
	sys_trace_k_thread_join_blocking(&thread, timeout);
	sys_trace_k_thread_join_exit(&thread, timeout, ret);
	/* ISR api */
	sys_trace_isr_enter();
	sys_trace_isr_exit();
	sys_trace_idle();
	/* condvar api */
	sys_trace_k_condvar_broadcast_enter(&condvar);
	sys_trace_k_condvar_broadcast_exit(&condvar, ret);
	sys_trace_k_condvar_init(&condvar, ret);
	sys_trace_k_condvar_signal_enter(&condvar);
	sys_trace_k_condvar_signal_blocking(&condvar);
	sys_trace_k_condvar_signal_exit(&condvar, ret);
	sys_trace_k_condvar_wait_enter(&condvar, timeout);
	sys_trace_k_condvar_wait_exit(&condvar, timeout, ret);
	/* sem api */
	sys_trace_k_sem_init(&sem, ret);
	sys_trace_k_sem_give_enter(&sem);
	sys_trace_k_sem_take_enter(&sem2, timeout);
	sys_trace_k_sem_take_exit(&sem, timeout, ret);
	sys_trace_k_sem_take_blocking(&sem, timeout);
	/* mutex api */
	sys_trace_k_mutex_init(&mutex, ret);
	sys_trace_k_mutex_lock_enter(&mutex, timeout);
	sys_trace_k_mutex_lock_exit(&mutex, timeout, ret);
	sys_trace_k_mutex_lock_blocking(&mutex, timeout);
	sys_trace_k_mutex_unlock_enter(&mutex);
	sys_trace_k_mutex_unlock_exit(&mutex, ret);
	/* timer api */
	sys_trace_k_timer_start(&timer, timeout, timeout);
	sys_trace_k_timer_init(&timer, NULL, NULL);

	/* wait for the tracing thread to flush the buffer (async mode) */
	k_sleep(K_MSEC(100));
	capturing = false;

	for (int i = 0; string_tracked[i] != NULL; i++) {
		zassert_true(api_found[i],
			     "Tracing API not found in backend output: %s",
			     string_tracked[i]);
	}
}

/**
 * @brief Test string format tracing
 *
 * @details Call tracing_format_string() with a known string and check that it
 * reaches the backend, in both synchronous and asynchronous mode.
 *
 * @ingroup tracing_api_tests
 */
ZTEST(tracing_api, test_tracing_string_format)
{
	sync_string_format_found = false;
	tracing_buffer_init();
	tracing_format_string("tracing_format_string_testing");
	k_sleep(K_MSEC(100));

	zassert_true(sync_string_format_found == true, "Failed to check output from backend");
}

/**
 * @brief Test tracing APIS
 *
 * @details Packaged the data by different format as the tracing input,
 * check the output for the self-designed uart backend.
 *
 * @ingroup tracing_api_tests
 */
ZTEST(tracing_api, test_tracing_data_format)
{
	tracing_data_t tracing_data, tracing_raw_data;
	uint8_t data[] = "tracing_format_data_testing";
	uint8_t raw_data[] = "tracing_format_raw_data_testing";

	tracing_buffer_init();
	tracing_data.data = data;
	tracing_data.length = sizeof(data);
	tracing_raw_data.data = raw_data;
	tracing_raw_data.length = sizeof(raw_data);

	tracing_format_data(&tracing_data, 1);
	k_sleep(K_MSEC(100));
	zassert_true(data_format_found == true, "Failed to check output from backend");

	tracing_format_raw_data(tracing_raw_data.data, tracing_raw_data.length);
	k_sleep(K_MSEC(100));
	zassert_true(raw_data_format_found == true, "Failed to check output from backend");
}

/**
 * @brief Test that disabled tracing produces no output
 *
 * @details Disable tracing through the command handler, emit a string and
 * check that nothing reaches the backend, then restore the enabled state.
 *
 * @ingroup tracing_api_tests
 */
ZTEST(tracing_api, test_tracing_disabled_output)
{
	uint8_t dis[] = "disable";
	uint8_t en[] = "enable";

	/* Start from a clean buffer and flag. */
	tracing_buffer_init();
	sync_string_format_found = false;

	tracing_cmd_handle(dis, sizeof(dis));
	zassert_false(is_tracing_enabled(), "Failed to disable tracing");

	tracing_format_string("tracing_format_string_testing");
	k_sleep(K_MSEC(100));
	zassert_false(sync_string_format_found,
		      "Disabled tracing must not emit to the backend");

	/* Restore the enabled state for the remaining tests. */
	tracing_cmd_handle(en, sizeof(en));
	zassert_true(is_tracing_enabled(), "Failed to re-enable tracing");
}

/**
 * @brief Test tracing APIS
 *
 * @details Simulate the host computer command to pass to the function
 * tracing_cmd_handle to detect the tracing behavior.
 *
 * @ingroup tracing_api_tests
 */
ZTEST(tracing_api, test_tracing_cmd_manual)
{
	uint32_t length = 0;
	uint8_t *cmd = NULL;
	uint8_t cmd0[] = " ";
	uint8_t cmd1[] = "disable";
	uint8_t cmd2[] = "enable";
	uint8_t cmd3[] = "garbage";

	length = tracing_cmd_buffer_alloc(&cmd);
	cmd = cmd0;
	zassert_true(sizeof(cmd0) < length, "cmd0 is too long");
	tracing_cmd_handle(cmd, sizeof(cmd0));
	zassert_true(is_tracing_enabled(),
		"Failed to check default status of tracing");

	length = tracing_cmd_buffer_alloc(&cmd);
	cmd = cmd1;
	zassert_true(sizeof(cmd1) < length, "cmd1 is too long");
	tracing_cmd_handle(cmd, sizeof(cmd1));
	zassert_false(is_tracing_enabled(), "Failed to disable tracing");

	length = tracing_cmd_buffer_alloc(&cmd);
	cmd = cmd2;
	zassert_true(sizeof(cmd2) < length, "cmd2 is too long");
	tracing_cmd_handle(cmd, sizeof(cmd2));
	zassert_true(is_tracing_enabled(), "Failed to enable tracing");

	/* An unrecognized command must leave the current state untouched. */
	tracing_cmd_handle(cmd3, sizeof(cmd3));
	zassert_true(is_tracing_enabled(),
		"Unknown command must not change the enabled state");

	tracing_cmd_handle(cmd1, sizeof(cmd1));
	zassert_false(is_tracing_enabled(), "Failed to disable tracing");
	tracing_cmd_handle(cmd3, sizeof(cmd3));
	zassert_false(is_tracing_enabled(),
		"Unknown command must not change the disabled state");

	/* Restore the enabled state for the remaining tests. */
	tracing_cmd_handle(cmd2, sizeof(cmd2));
	zassert_true(is_tracing_enabled(), "Failed to enable tracing");
}

/*
 * The tracing enabled/disabled state is global and persists across tests, so
 * start every test from a known state: tracing enabled and a clean buffer.
 */
static void tracing_api_before(void *fixture)
{
	uint8_t en[] = "enable";

	ARG_UNUSED(fixture);

	tracing_cmd_handle(en, sizeof(en));
	tracing_buffer_init();
}

ZTEST_SUITE(tracing_api, NULL, NULL, tracing_api_before, NULL, NULL);
