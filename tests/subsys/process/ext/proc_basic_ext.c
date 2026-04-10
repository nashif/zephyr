/*
 * Copyright (c) 2026 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file proc_basic_ext.c
 * @brief Minimal extension: just prints a string and returns.
 *
 * Used by test_load_unload and test_spawn_join to verify the basic
 * load → start → run → join → unload lifecycle.
 */

#include <zephyr/kernel.h>
#include <zephyr/llext/symbol.h>

void process_main(void *arg)
{
	ARG_UNUSED(arg);
	printk("proc_basic: running\n");
}

LL_EXTENSION_SYMBOL(process_main);
