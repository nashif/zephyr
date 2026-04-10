/*
 * Copyright (c) 2026 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file proc_a_ext.c
 * @brief Extension for process A in the isolation sample.
 *
 * Process A counts to 5 and prints its counter value each iteration.
 * It receives a pointer to a k_msgq (passed as arg by the host) so it can
 * submit its results into a shared read-only output channel without touching
 * process B's private memory.
 */

#include <zephyr/kernel.h>
#include <zephyr/llext/symbol.h>

void process_main(void *arg)
{
	struct k_msgq *out = (struct k_msgq *)arg;
	uint32_t counter;

	for (counter = 1; counter <= 5; counter++) {
		printk("Process A: counter=%u\n", counter);
		if (out != NULL) {
			k_msgq_put(out, &counter, K_NO_WAIT);
		}
		k_msleep(100);
	}
}

LL_EXTENSION_SYMBOL(process_main);
