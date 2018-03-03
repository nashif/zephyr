/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <misc/printk.h>
#include <misc/stack.h>
#include <shell/shell.h>
#include <init.h>
#include <debug/object_tracing.h>

#define SHELL_KERNEL "kernel"

static int shell_cmd_version(int argc, char *argv[])
{
	u32_t version = sys_kernel_version_get();

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	printk("Zephyr version %d.%d.%d\n",
	       SYS_KERNEL_VER_MAJOR(version),
	       SYS_KERNEL_VER_MINOR(version),
	       SYS_KERNEL_VER_PATCHLEVEL(version));
	return 0;
}

static int shell_cmd_uptime(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	printk("uptime: %u ms\n", k_uptime_get_32());

	return 0;
}

static int shell_cmd_cycles(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	printk("cycles: %u hw cycles\n", k_cycle_get_32());

	return 0;
}

#if defined(CONFIG_THREAD_MONITOR)
static int shell_cmd_threads(int argc, char *argv[])
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	struct k_thread *thread_list = NULL;
	unsigned int pcnt, unused = 0;

	printk("Threads:\n");

	thread_list   = (struct k_thread *)SYS_THREAD_MONITOR_HEAD;
	printk("  Name                    State Stack         Options Priority\n");
	printk("--------------------------------------------------------\n");

	while (thread_list != NULL) {

		int size = thread_list->stack_info.size;
		unused = stack_unused_space_get(K_THREAD_STACK_BUFFER(thread_list->stack_obj), size);

		/* Calculate the real size reserved for the stack */
		pcnt = ((size - unused) * 100) / size;

		printk("%s %-10s (%p) %d %6d/%-6d 0x%x     %d\n",
		       (thread_list == k_current_get()) ? "*" : " ",
		       thread_list->name ? thread_list->name : "NA",
		       thread_list,
		       thread_list->base.thread_state,
		       (size - unused),
		       size,
		       thread_list->base.user_options,
		       k_thread_priority_get(thread_list));
		thread_list = (struct k_thread *)SYS_THREAD_MONITOR_NEXT(thread_list);
	}
	return 0;
}
#endif


#if defined(CONFIG_INIT_STACKS)
static int shell_cmd_stack(int argc, char *argv[])
{
	k_call_stacks_analyze();
	return 0;
}
#endif

struct shell_cmd kernel_commands[] = {
	{ "version", shell_cmd_version, "show kernel version" },
	{ "uptime", shell_cmd_uptime, "show system uptime in milliseconds" },
	{ "cycles", shell_cmd_cycles, "show system hardware cycles" },
#if defined(CONFIG_THREAD_MONITOR)
	{ "threads", shell_cmd_threads, "show running threads" },
#endif
#if defined(CONFIG_INIT_STACKS)
	{ "stacks", shell_cmd_stack, "show system stacks" },
#endif
	{ NULL, NULL, NULL }
};


SHELL_REGISTER(SHELL_KERNEL, kernel_commands);
