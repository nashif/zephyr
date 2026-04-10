/*
 * Copyright (c) 2026 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief Isolation sample for the Zephyr Process Model.
 *
 * Shows two processes (A and B) running concurrently.  Each process:
 *   - Lives in its own dedicated memory domain
 *   - Runs in unprivileged (user) mode when CONFIG_USERSPACE is enabled
 *   - Cannot access the other process's memory regions
 *
 * A shared message queue (host-allocated) is passed to both processes via
 * z_process_opts.arg so they can forward their counter values to the host
 * without sharing any writable state between themselves.
 *
 * Usage sketch:
 *
 *   z_process_spawn(&proc_a, "proc_a", elf_a, sz_a, stack_a, ..., &opts_a);
 *   z_process_spawn(&proc_b, "proc_b", elf_b, sz_b, stack_b, ..., &opts_b);
 *   z_process_join(&proc_a, K_FOREVER);
 *   z_process_join(&proc_b, K_FOREVER);
 *   z_process_unload(&proc_a);
 *   z_process_unload(&proc_b);
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/process/process.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Embedded extension ELF binaries */
static const uint8_t proc_a_elf[] = {
#include <proc_a.inc>
};

static const uint8_t proc_b_elf[] = {
#include <proc_b.inc>
};

/* Thread stacks (one per process) */
Z_PROCESS_STACK_DEFINE(stack_a, CONFIG_PROCESS_STACK_SIZE_DEFAULT);
Z_PROCESS_STACK_DEFINE(stack_b, CONFIG_PROCESS_STACK_SIZE_DEFAULT);

/* Process descriptors */
static struct z_process proc_a;
static struct z_process proc_b;

/*
 * Shared output message queue.
 *
 * Both processes put their uint32_t counter values here.  The host reads them
 * out on the main thread to demonstrate that communication via a kernel
 * primitive works even when the processes themselves are isolated.
 *
 * The host grants both process threads access to the queue object via the
 * standard kernel object permission system (k_object_access_grant) before
 * starting the threads.
 */
K_MSGQ_DEFINE(out_q, sizeof(uint32_t), 16, 4);

int main(void)
{
	LOG_INF("Process Model sample: isolation");

	struct z_process_opts opts_a = Z_PROCESS_OPTS_DEFAULT;
	struct z_process_opts opts_b = Z_PROCESS_OPTS_DEFAULT;

	opts_a.arg = &out_q;
	opts_b.arg = &out_q;

	/*
	 * Grant both upcoming threads access to the message queue kernel
	 * object *before* spawning them, so user-mode threads can call
	 * k_msgq_put().
	 */
#ifdef CONFIG_USERSPACE
	k_object_access_grant(&out_q, &proc_a.thread);
	k_object_access_grant(&out_q, &proc_b.thread);
#endif

	int ret;

	ret = z_process_spawn(&proc_a, "proc_a",
			      proc_a_elf, sizeof(proc_a_elf),
			      stack_a, sizeof(stack_a), &opts_a);
	if (ret != 0) {
		LOG_ERR("Failed to spawn proc_a: %d", ret);
		return ret;
	}

	ret = z_process_spawn(&proc_b, "proc_b",
			      proc_b_elf, sizeof(proc_b_elf),
			      stack_b, sizeof(stack_b), &opts_b);
	if (ret != 0) {
		LOG_ERR("Failed to spawn proc_b: %d", ret);
		z_process_kill(&proc_a);
		z_process_unload(&proc_a);
		return ret;
	}

	/* Wait for both processes */
	z_process_join(&proc_a, K_FOREVER);
	z_process_join(&proc_b, K_FOREVER);

	LOG_INF("All processes finished");
	LOG_INF("  proc_a exit code: %d", z_process_exit_code(&proc_a));
	LOG_INF("  proc_b exit code: %d", z_process_exit_code(&proc_b));

	z_process_unload(&proc_a);
	z_process_unload(&proc_b);

	return 0;
}
