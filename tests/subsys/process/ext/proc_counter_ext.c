/*
 * Copyright (c) 2026 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file proc_counter_ext.c
 * @brief Extension that increments a semaphore-protected counter.
 *
 * The host passes a pointer to a k_sem as the arg.  The extension gives
 * (signals) the semaphore N_ITERATIONS times.  The host counts how many
 * signals it receives after z_process_join() to verify the extension ran
 * to completion.
 *
 * Used by test_concurrent_processes.
 */

#include <zephyr/kernel.h>
#include <zephyr/llext/symbol.h>

#define N_ITERATIONS 5

void process_main(void *arg)
{
	struct k_sem *sem = (struct k_sem *)arg;

	for (int i = 0; i < N_ITERATIONS; i++) {
		k_sem_give(sem);
		k_msleep(10);
	}
}

LL_EXTENSION_SYMBOL(process_main);
