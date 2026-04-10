/*
 * Copyright (c) 2026 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/llext/buf_loader.h>
#include <zephyr/llext/llext.h>
#include <zephyr/logging/log.h>
#include <zephyr/process/process.h>

LOG_MODULE_REGISTER(process, CONFIG_PROCESS_LOG_LEVEL);

/* -------------------------------------------------------------------------
 * Internal helpers
 * -----------------------------------------------------------------------*/

/*
 * process_do_run() - core execution path, always called in supervisor mode.
 *
 * For supervisor-mode processes this is the whole execution.
 * For user-mode processes this is reached from process_user_entry_fn()
 * which runs in user mode — but note that process_user_entry_fn is
 * declared below and only calls the extension's entry function; all
 * kernel-side plumbing (bringup/teardown, state machine) stays here.
 */
static void process_do_run(struct z_process *proc)
{
	proc->entry_fn(proc->opts.arg);
	proc->exit_code = 0;
}

#ifdef CONFIG_USERSPACE
/*
 * process_user_entry_fn() - thread entry used when opts.user_mode is true.
 *
 * k_thread_user_mode_enter() calls this after dropping privileges.  At this
 * point the current thread has access only to its own stack and the memory
 * partitions in its domain (the LLEXT regions added by z_process_load).
 *
 * We receive entry_fn and arg directly in p1/p2 to avoid touching the
 * struct z_process (which lives in privileged memory) from user mode.
 */
static FUNC_NORETURN void process_user_entry_fn(void *p1, void *p2, void *p3)
{
	llext_entry_fn_t fn = (llext_entry_fn_t)p1;
	void *arg = p2;

	ARG_UNUSED(p3);

	fn(arg);

	/* The thread will be cleaned up by the kernel when this returns. */
	k_thread_abort(k_current_get());
	CODE_UNREACHABLE;
}
#endif /* CONFIG_USERSPACE */

/*
 * process_thread_fn() - supervisor trampoline, used as the Zephyr thread
 * entry point for every process.
 *
 * Calls the extension's init_array, runs the entry function (possibly after
 * dropping to user mode), then records the exit state.
 *
 * Note: teardown (fini_array) is performed in z_process_unload() so that it
 * always runs in supervisor mode regardless of the process privilege level.
 */
static void process_thread_fn(void *p1, void *p2, void *p3)
{
	struct z_process *proc = (struct z_process *)p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* Run the extension's .preinit_array / .init_array */
	int ret = llext_bringup(proc->ext);

	if (ret != 0) {
		LOG_ERR("process '%s': bringup failed (%d)", proc->name, ret);
		proc->exit_code = ret;
		proc->state = Z_PROCESS_STATE_DEAD;
		return;
	}

#ifdef CONFIG_USERSPACE
	if (proc->opts.user_mode) {
		/*
		 * Drop privileges.  Pass entry_fn and arg as p1/p2 so the
		 * user-side handler does not have to dereference proc (which
		 * is not in the process memory domain).
		 *
		 * k_thread_user_mode_enter() is FUNC_NORETURN; the thread
		 * state transition to DEAD is handled below via k_thread_join
		 * in z_process_join(), not through a callback.
		 */
		k_thread_user_mode_enter(process_user_entry_fn,
					 (void *)proc->entry_fn,
					 proc->opts.arg,
					 NULL);
		/* NOT REACHED */
	}
#endif /* CONFIG_USERSPACE */

	/* Supervisor mode: run directly and record exit */
	process_do_run(proc);
	proc->state = Z_PROCESS_STATE_DEAD;
}

/* -------------------------------------------------------------------------
 * Public API
 * -----------------------------------------------------------------------*/

int z_process_load(struct z_process *proc, const char *name,
		   const void *elf_data, size_t elf_size,
		   k_thread_stack_t *stack, size_t stack_size,
		   const struct z_process_opts *opts)
{
	if (proc == NULL || name == NULL || elf_data == NULL ||
	    elf_size == 0 || stack == NULL || stack_size == 0) {
		return -EINVAL;
	}

	/* Initialise descriptor */
	memset(proc, 0, sizeof(*proc));
	strncpy(proc->name, name, CONFIG_PROCESS_NAME_MAX_LEN);
	proc->name[CONFIG_PROCESS_NAME_MAX_LEN] = '\0';

	/* Apply options (or defaults) */
	if (opts != NULL) {
		proc->opts = *opts;
	} else {
		struct z_process_opts defaults = Z_PROCESS_OPTS_DEFAULT;

		proc->opts = defaults;
	}
	if (proc->opts.stack_size == 0) {
		proc->opts.stack_size = CONFIG_PROCESS_STACK_SIZE_DEFAULT;
	}
	if (proc->opts.entry_sym == NULL) {
		proc->opts.entry_sym = Z_PROCESS_ENTRY_SYM;
	}

	proc->stack      = stack;
	proc->stack_size = stack_size;

	/* ------------------------------------------------------------------
	 * Load the ELF via LLEXT
	 * ----------------------------------------------------------------*/
	struct llext_buf_loader buf_loader =
		LLEXT_TEMPORARY_BUF_LOADER((const uint8_t *)elf_data, elf_size);
	const struct llext_load_param ldr_parm = LLEXT_LOAD_PARAM_DEFAULT;

	int ret = llext_load(&buf_loader.loader, proc->name, &proc->ext,
			     &ldr_parm);
	if (ret != 0) {
		LOG_ERR("process '%s': llext_load failed (%d)", proc->name,
			ret);
		return ret;
	}

	/* ------------------------------------------------------------------
	 * Resolve entry function symbol
	 * ----------------------------------------------------------------*/
	proc->entry_fn = (llext_entry_fn_t)llext_find_sym(
		&proc->ext->exp_tab, proc->opts.entry_sym);
	if (proc->entry_fn == NULL) {
		LOG_ERR("process '%s': entry symbol '%s' not found",
			proc->name, proc->opts.entry_sym);
		llext_unload(&proc->ext);
		return -ENOENT;
	}

	/* ------------------------------------------------------------------
	 * Set up memory domain (requires CONFIG_USERSPACE)
	 * ----------------------------------------------------------------*/
#ifdef CONFIG_USERSPACE
	ret = k_mem_domain_init(&proc->domain, 0, NULL);
	if (ret != 0) {
		LOG_ERR("process '%s': k_mem_domain_init failed (%d)",
			proc->name, ret);
		llext_unload(&proc->ext);
		return ret;
	}

	ret = llext_add_domain(proc->ext, &proc->domain);
	if (ret != 0) {
		LOG_ERR("process '%s': llext_add_domain failed (%d)",
			proc->name, ret);
		k_mem_domain_deinit(&proc->domain);
		llext_unload(&proc->ext);
		return ret;
	}
#endif /* CONFIG_USERSPACE */

	proc->state = Z_PROCESS_STATE_LOADED;
	LOG_INF("process '%s': loaded (entry='%s')", proc->name,
		proc->opts.entry_sym);
	return 0;
}

int z_process_start(struct z_process *proc)
{
	if (proc == NULL || proc->state != Z_PROCESS_STATE_LOADED) {
		return -EINVAL;
	}

	/*
	 * Create the thread suspended so we can assign it to the memory
	 * domain before it is scheduled.
	 */
	k_thread_create(&proc->thread, proc->stack, proc->stack_size,
			process_thread_fn, proc, NULL, NULL,
			proc->opts.priority, 0, K_FOREVER);

	k_thread_name_set(&proc->thread, proc->name);

#ifdef CONFIG_USERSPACE
	if (proc->opts.user_mode) {
		/*
		 * Add the thread to the domain *before* it runs so that the
		 * architecture layer can configure the MPU/MMU accordingly.
		 */
		k_mem_domain_add_thread(&proc->domain, &proc->thread);
	}
#endif /* CONFIG_USERSPACE */

	proc->state = Z_PROCESS_STATE_RUNNING;

	/* Release the thread to the scheduler */
	k_thread_start(&proc->thread);

	LOG_DBG("process '%s': started (tid=%p, prio=%d, user=%d)",
		proc->name, (void *)&proc->thread, proc->opts.priority,
		(int)proc->opts.user_mode);
	return 0;
}

int z_process_spawn(struct z_process *proc, const char *name,
		    const void *elf_data, size_t elf_size,
		    k_thread_stack_t *stack, size_t stack_size,
		    const struct z_process_opts *opts)
{
	int ret = z_process_load(proc, name, elf_data, elf_size, stack,
				 stack_size, opts);
	if (ret != 0) {
		return ret;
	}

	return z_process_start(proc);
}

int z_process_join(struct z_process *proc, k_timeout_t timeout)
{
	if (proc == NULL) {
		return -EINVAL;
	}
	if (proc->state != Z_PROCESS_STATE_RUNNING &&
	    proc->state != Z_PROCESS_STATE_DEAD) {
		return -EINVAL;
	}

	if (proc->state == Z_PROCESS_STATE_DEAD) {
		return 0;
	}

	int ret = k_thread_join(&proc->thread, timeout);

	if (ret == 0) {
		proc->state = Z_PROCESS_STATE_DEAD;
	}
	return ret;
}

int z_process_kill(struct z_process *proc)
{
	if (proc == NULL || proc->state != Z_PROCESS_STATE_RUNNING) {
		return -EINVAL;
	}

	k_thread_abort(&proc->thread);
	proc->exit_code = -ECANCELED;
	proc->state     = Z_PROCESS_STATE_DEAD;

	LOG_INF("process '%s': killed", proc->name);
	return 0;
}

void z_process_unload(struct z_process *proc)
{
	if (proc == NULL || proc->ext == NULL) {
		return;
	}

	if (proc->state == Z_PROCESS_STATE_RUNNING) {
		LOG_WRN("process '%s': unloading while still running; "
			"call z_process_kill() first",
			proc->name);
		z_process_kill(proc);
	}

	/*
	 * Call the extension's .fini_array (C++ destructors, etc.) in
	 * supervisor mode.  This is always safe because at this point the
	 * process thread has already exited.
	 */
	int ret = llext_teardown(proc->ext);

	if (ret != 0) {
		LOG_WRN("process '%s': teardown failed (%d)", proc->name,
			ret);
	}

#ifdef CONFIG_USERSPACE
	k_mem_domain_deinit(&proc->domain);
#endif

	llext_unload(&proc->ext);

	LOG_INF("process '%s': unloaded", proc->name);

	/* Reset the descriptor so it can be reused */
	memset(proc, 0, sizeof(*proc));
	proc->state = Z_PROCESS_STATE_UNLOADED;
}
