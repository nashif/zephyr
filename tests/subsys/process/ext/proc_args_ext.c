/*
 * Copyright (c) 2026 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file proc_args_ext.c
 * @brief Extension that writes its arg into a result slot.
 *
 * The host passes a pointer to a uint32_t as the arg.  The extension writes
 * the magic value 0xDEADBEEF into it so the test can verify the arg was
 * delivered correctly.
 *
 * Used by test_arg_forwarding.
 */

#include <zephyr/kernel.h>
#include <zephyr/llext/symbol.h>
#include <stdint.h>

void process_main(void *arg)
{
	uint32_t *result = (uint32_t *)arg;

	if (result != NULL) {
		*result = 0xDEADBEEFU;
	}
}

LL_EXTENSION_SYMBOL(process_main);
