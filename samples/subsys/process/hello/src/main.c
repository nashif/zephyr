/*
 * Copyright (c) 2026 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief Hello World sample for the Zephyr Process Model.
 *
 * Demonstrates the simplest possible use of the process API:
 *
 *   1. z_process_spawn() – load the ELF and start the thread in one call
 *   2. z_process_join()  – wait for the process to finish
 *   3. z_process_unload() – release all resources
 *
 * The loadable extension (hello_ext.c) is compiled into hello.llext at build
 * time and embedded as a C array via hello.inc.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/process/process.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* The extension binary is embedded as a C array by the build system. */
static const uint8_t hello_elf[] = {
#include <hello.inc>
};

/* Stack for the process thread */
Z_PROCESS_STACK_DEFINE(hello_stack, CONFIG_PROCESS_STACK_SIZE_DEFAULT);

/* Process descriptor */
static struct z_process hello_proc;

int main(void)
{
	LOG_INF("Process Model sample: hello world");

	/*
	 * Configure process options.
	 * Pass the integer 42 as the argument so the extension can print it.
	 */
	struct z_process_opts opts = Z_PROCESS_OPTS_DEFAULT;

	opts.arg = (void *)(uintptr_t)42;

	/*
	 * Load and start the process in one call.
	 * The extension runs in user mode (hardware-isolated) when
	 * CONFIG_USERSPACE is enabled.
	 */
	int ret = z_process_spawn(&hello_proc, "hello",
				  hello_elf, sizeof(hello_elf),
				  hello_stack, sizeof(hello_stack),
				  &opts);
	if (ret != 0) {
		LOG_ERR("z_process_spawn failed: %d", ret);
		return ret;
	}

	/* Wait for the process to finish */
	ret = z_process_join(&hello_proc, K_SECONDS(5));
	if (ret != 0) {
		LOG_ERR("z_process_join failed: %d", ret);
		z_process_kill(&hello_proc);
	} else {
		LOG_INF("Process exited with code %d",
			z_process_exit_code(&hello_proc));
	}

	/* Release LLEXT memory and reset the descriptor */
	z_process_unload(&hello_proc);

	LOG_INF("Done");
	return 0;
}
