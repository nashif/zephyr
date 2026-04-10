/*
 * Copyright (c) 2026 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file proc_b_ext.c
 * @brief Extension for process B in the isolation sample.
 *
 * Process B counts *down* from 5 to 1 and prints its counter value.
 * It is completely isolated from process A: it has its own memory domain
 * and cannot access process A's TEXT, DATA, RODATA, or BSS regions.
 */

#include <zephyr/kernel.h>
#include <zephyr/llext/symbol.h>

void process_main(void *arg)
{
	struct k_msgq *out = (struct k_msgq *)arg;
	uint32_t counter;

	for (counter = 5; counter >= 1; counter--) {
		printk("Process B: counter=%u\n", counter);
		if (out != NULL) {
			k_msgq_put(out, &counter, K_NO_WAIT);
		}
		k_msleep(150);
		if (counter == 0) {
			break; /* prevent underflow on unsigned wrap */
		}
	}
}

LL_EXTENSION_SYMBOL(process_main);
