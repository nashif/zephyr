/*
 * Copyright (c) 2026 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief Zephyr Process Model test suite
 *
 * Exercises the full lifecycle and correctness of the process subsystem:
 *
 *   - test_load_unload         – load then unload without running
 *   - test_spawn_join          – full spawn → join → unload cycle
 *   - test_state_machine       – verify state transitions
 *   - test_invalid_args        – NULL/bad-arg rejection
 *   - test_arg_forwarding      – arg reaches the extension
 *   - test_concurrent_processes – two processes run in parallel
 *   - test_reload              – descriptor can be reused after unload
 *   - test_kill                – z_process_kill stops a running process
 *   - test_missing_entry_sym   – loading with unknown symbol returns -ENOENT
 */

#include <zephyr/kernel.h>
#include <zephyr/process/process.h>
#include <zephyr/ztest.h>
#include <stdint.h>

/* ─────────────────────────────────────────────────────────────────────────
 * Embedded extension binaries (generated at build time)
 * ─────────────────────────────────────────────────────────────────────── */

static const uint8_t basic_elf[] = {
#include <proc_basic.inc>
};

static const uint8_t args_elf[] = {
#include <proc_args.inc>
};

static const uint8_t counter_elf[] = {
#include <proc_counter.inc>
};

/* ─────────────────────────────────────────────────────────────────────────
 * Shared test fixtures
 * ─────────────────────────────────────────────────────────────────────── */

/* Each test that needs a stack gets one of these */
Z_PROCESS_STACK_DEFINE(test_stack_0, CONFIG_PROCESS_STACK_SIZE_DEFAULT);
Z_PROCESS_STACK_DEFINE(test_stack_1, CONFIG_PROCESS_STACK_SIZE_DEFAULT);
Z_PROCESS_STACK_DEFINE(test_stack_2, CONFIG_PROCESS_STACK_SIZE_DEFAULT);

/* Semaphore used by the counter extension */
K_SEM_DEFINE(counter_sem, 0, 10);

/* Result slot used by the args extension  */
static volatile uint32_t args_result;

/* ─────────────────────────────────────────────────────────────────────────
 * Tests
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * @brief Load an extension without running it, then unload.
 */
ZTEST(process_lifecycle, test_load_unload)
{
	struct z_process proc;

	int ret = z_process_load(&proc, "basic",
				 basic_elf, sizeof(basic_elf),
				 test_stack_0, sizeof(test_stack_0),
				 NULL);

	zassert_ok(ret, "z_process_load failed: %d", ret);
	zassert_equal(z_process_get_state(&proc), Z_PROCESS_STATE_LOADED,
		      "expected LOADED state");

	z_process_unload(&proc);
	zassert_equal(z_process_get_state(&proc), Z_PROCESS_STATE_UNLOADED,
		      "expected UNLOADED state after unload");
}

/**
 * @brief Full spawn → join → unload cycle with the basic extension.
 */
ZTEST(process_lifecycle, test_spawn_join)
{
	struct z_process proc;

	int ret = z_process_spawn(&proc, "basic",
				  basic_elf, sizeof(basic_elf),
				  test_stack_0, sizeof(test_stack_0),
				  NULL);

	zassert_ok(ret, "z_process_spawn failed: %d", ret);
	zassert_equal(z_process_get_state(&proc), Z_PROCESS_STATE_RUNNING,
		      "expected RUNNING state");

	ret = z_process_join(&proc, K_SECONDS(5));
	zassert_ok(ret, "z_process_join failed: %d", ret);
	zassert_equal(z_process_get_state(&proc), Z_PROCESS_STATE_DEAD,
		      "expected DEAD state after join");
	zassert_equal(z_process_exit_code(&proc), 0,
		      "expected exit code 0");

	z_process_unload(&proc);
}

/**
 * @brief State machine: verify all transitions are correct.
 */
ZTEST(process_lifecycle, test_state_machine)
{
	struct z_process proc;

	/* initial state is implicitly UNLOADED (zeroed struct) */
	memset(&proc, 0, sizeof(proc));
	zassert_equal(z_process_get_state(&proc), Z_PROCESS_STATE_UNLOADED,
		      "freshly zeroed descriptor should be UNLOADED");

	zassert_ok(z_process_load(&proc, "sm",
				  basic_elf, sizeof(basic_elf),
				  test_stack_0, sizeof(test_stack_0),
				  NULL));
	zassert_equal(z_process_get_state(&proc), Z_PROCESS_STATE_LOADED);

	zassert_ok(z_process_start(&proc));
	zassert_equal(z_process_get_state(&proc), Z_PROCESS_STATE_RUNNING);

	zassert_ok(z_process_join(&proc, K_SECONDS(5)));
	zassert_equal(z_process_get_state(&proc), Z_PROCESS_STATE_DEAD);

	z_process_unload(&proc);
	zassert_equal(z_process_get_state(&proc), Z_PROCESS_STATE_UNLOADED);
}

/**
 * @brief API functions must reject NULL and invalid arguments gracefully.
 */
ZTEST(process_lifecycle, test_invalid_args)
{
	struct z_process proc;

	/* NULL proc pointer */
	zassert_equal(z_process_load(NULL, "x", basic_elf, sizeof(basic_elf),
				     test_stack_0, sizeof(test_stack_0), NULL),
		      -EINVAL, "NULL proc should return -EINVAL");

	/* NULL ELF data */
	zassert_equal(z_process_load(&proc, "x", NULL, 128,
				     test_stack_0, sizeof(test_stack_0), NULL),
		      -EINVAL, "NULL elf_data should return -EINVAL");

	/* zero ELF size */
	zassert_equal(z_process_load(&proc, "x", basic_elf, 0,
				     test_stack_0, sizeof(test_stack_0), NULL),
		      -EINVAL, "zero elf_size should return -EINVAL");

	/* NULL name */
	zassert_equal(z_process_load(&proc, NULL, basic_elf, sizeof(basic_elf),
				     test_stack_0, sizeof(test_stack_0), NULL),
		      -EINVAL, "NULL name should return -EINVAL");

	/* start/join/kill on unloaded (zeroed) descriptor */
	memset(&proc, 0, sizeof(proc));
	zassert_equal(z_process_start(&proc), -EINVAL,
		      "starting unloaded proc should return -EINVAL");
	zassert_equal(z_process_join(&proc, K_NO_WAIT), -EINVAL,
		      "joining unloaded proc should return -EINVAL");
	zassert_equal(z_process_kill(&proc), -EINVAL,
		      "killing unloaded proc should return -EINVAL");
}

/**
 * @brief The arg value passed via opts reaches the extension entry function.
 *
 * proc_args_ext writes 0xDEADBEEF into the pointer it receives as arg.
 * We verify the host-side variable is updated after the process exits.
 */
ZTEST(process_lifecycle, test_arg_forwarding)
{
	struct z_process proc;

	args_result = 0;

	struct z_process_opts opts = Z_PROCESS_OPTS_DEFAULT;

	opts.arg = (void *)&args_result;

	int ret = z_process_spawn(&proc, "args",
				  args_elf, sizeof(args_elf),
				  test_stack_0, sizeof(test_stack_0),
				  &opts);

	zassert_ok(ret, "spawn failed: %d", ret);

	ret = z_process_join(&proc, K_SECONDS(5));
	zassert_ok(ret, "join failed: %d", ret);

	/*
	 * NOTE: When running in user mode the extension cannot write to
	 * args_result because it is a kernel BSS variable that is not mapped
	 * into the process domain.  This test is meaningful in supervisor mode
	 * (CONFIG_USERSPACE=n) or when the caller explicitly adds the result
	 * slot to the process domain via a custom partition.
	 *
	 * The test assertion is conditioned accordingly.
	 */
#ifndef CONFIG_USERSPACE
	zassert_equal(args_result, 0xDEADBEEFU,
		      "arg not forwarded to extension (got 0x%08x)",
		      args_result);
#endif

	z_process_unload(&proc);
}

/**
 * @brief Two processes run concurrently; each signals a semaphore N times.
 *
 * After both have finished, the semaphore count should equal 2 * N_ITER.
 */
ZTEST(process_lifecycle, test_concurrent_processes)
{
	struct z_process proc0, proc1;
	const int N_ITER = 5;

	k_sem_reset(&counter_sem);

	struct z_process_opts opts = Z_PROCESS_OPTS_DEFAULT;

	opts.arg = &counter_sem;

#ifdef CONFIG_USERSPACE
	/* Grant both threads access to the semaphore kernel object */
	k_object_access_grant(&counter_sem, &proc0.thread);
	k_object_access_grant(&counter_sem, &proc1.thread);
#endif

	zassert_ok(z_process_spawn(&proc0, "ctr0",
				   counter_elf, sizeof(counter_elf),
				   test_stack_1, sizeof(test_stack_1),
				   &opts));

	zassert_ok(z_process_spawn(&proc1, "ctr1",
				   counter_elf, sizeof(counter_elf),
				   test_stack_2, sizeof(test_stack_2),
				   &opts));

	zassert_ok(z_process_join(&proc0, K_SECONDS(5)));
	zassert_ok(z_process_join(&proc1, K_SECONDS(5)));

	int count = k_sem_count_get(&counter_sem);

	zassert_equal(count, 2 * N_ITER,
		      "expected %d semaphore signals, got %d",
		      2 * N_ITER, count);

	z_process_unload(&proc0);
	z_process_unload(&proc1);
}

/**
 * @brief A process descriptor can be reloaded and reused after unload.
 */
ZTEST(process_lifecycle, test_reload)
{
	struct z_process proc;

	for (int i = 0; i < 3; i++) {
		int ret = z_process_spawn(&proc, "reload",
					  basic_elf, sizeof(basic_elf),
					  test_stack_0, sizeof(test_stack_0),
					  NULL);

		zassert_ok(ret, "iteration %d: spawn failed: %d", i, ret);
		zassert_ok(z_process_join(&proc, K_SECONDS(5)),
			   "iteration %d: join failed", i);
		z_process_unload(&proc);
	}
}

/**
 * @brief z_process_kill stops the process thread.
 *
 * We load a process whose extension sleeps for a long time, then kill it
 * before it would finish naturally.
 */
ZTEST(process_lifecycle, test_kill)
{
	/*
	 * The counter extension sleeps 10 ms per iteration × 5 iterations =
	 * 50 ms total.  Kill after 20 ms to interrupt it mid-run.
	 */
	struct z_process proc;
	struct z_process_opts opts = Z_PROCESS_OPTS_DEFAULT;

	opts.arg = NULL; /* counter extension handles NULL sem gracefully */

	zassert_ok(z_process_spawn(&proc, "kill_target",
				   counter_elf, sizeof(counter_elf),
				   test_stack_0, sizeof(test_stack_0),
				   &opts));

	zassert_equal(z_process_get_state(&proc), Z_PROCESS_STATE_RUNNING);

	/* Let it run briefly, then kill */
	k_msleep(20);
	zassert_ok(z_process_kill(&proc), "kill failed");
	zassert_equal(z_process_get_state(&proc), Z_PROCESS_STATE_DEAD,
		      "expected DEAD state after kill");

	z_process_unload(&proc);
}

/**
 * @brief Loading with an unknown entry symbol returns -ENOENT.
 */
ZTEST(process_lifecycle, test_missing_entry_sym)
{
	struct z_process proc;
	struct z_process_opts opts = Z_PROCESS_OPTS_DEFAULT;

	opts.entry_sym = "this_symbol_does_not_exist";

	int ret = z_process_load(&proc, "nosym",
				 basic_elf, sizeof(basic_elf),
				 test_stack_0, sizeof(test_stack_0),
				 &opts);

	zassert_equal(ret, -ENOENT,
		      "expected -ENOENT for unknown symbol, got %d", ret);
	/* Descriptor should be fully cleaned up even on error */
	zassert_equal(z_process_get_state(&proc), Z_PROCESS_STATE_UNLOADED,
		      "state should be UNLOADED after failed load");
}

/* ─────────────────────────────────────────────────────────────────────────
 * Test suite registration
 * ─────────────────────────────────────────────────────────────────────── */

ZTEST_SUITE(process_lifecycle, NULL, NULL, NULL, NULL, NULL);
