/*
 * Copyright (c) 2026 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_PROCESS_PROCESS_H_
#define ZEPHYR_INCLUDE_PROCESS_PROCESS_H_

/**
 * @file
 * @brief Zephyr Process Model
 *
 * The process model provides a simplified API that unifies three Zephyr
 * subsystems into a coherent abstraction for isolated, dynamically-loaded
 * application processes:
 *
 *  - **LLEXT** (Linkable Loadable Extensions): loads ELF binaries at runtime
 *  - **Memory Domains**: gives each process its own hardware-enforced memory
 *    partition so that one process cannot read or write another's memory
 *  - **Userspace** (optional): runs the process thread in unprivileged mode,
 *    restricting direct kernel access to the syscall interface
 *
 * ### Typical usage
 *
 * @code{.c}
 * // 1. Define a stack (static allocation, like any Zephyr thread)
 * Z_PROCESS_STACK_DEFINE(my_stack, 2048);
 *
 * // 2. Declare a process descriptor (may be a global or local variable)
 * struct z_process my_proc;
 *
 * // 3. Load and immediately start the process from an ELF binary in flash
 * extern const uint8_t my_app_elf[];
 * extern const size_t  my_app_elf_size;
 *
 * z_process_spawn(&my_proc, "my_app",
 *                 my_app_elf, my_app_elf_size,
 *                 my_stack, sizeof(my_stack),
 *                 NULL);  // NULL = use defaults
 *
 * // 4. Optionally wait for it to finish
 * z_process_join(&my_proc, K_FOREVER);
 *
 * // 5. Unload and reclaim resources
 * z_process_unload(&my_proc);
 * @endcode
 *
 * ### Extension entry function
 *
 * An extension (the loadable process body) must export a function named
 * @c process_main (or whatever @ref z_process_opts::entry_sym is set to):
 *
 * @code{.c}
 * #include <zephyr/llext/symbol.h>
 *
 * void process_main(void *arg)
 * {
 *     // process code here
 * }
 * LL_EXTENSION_SYMBOL(process_main);
 * @endcode
 *
 * @defgroup process_apis Process Model
 * @since 4.2
 * @version 0.1.0
 * @ingroup os_services
 * @{
 */

#include <zephyr/kernel.h>
#include <zephyr/llext/llext.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Default name of the entry-point symbol inside an extension */
#define Z_PROCESS_ENTRY_SYM "process_main"

/**
 * @brief Process lifecycle states
 */
enum z_process_state {
	/** Process descriptor is initialised but no extension is loaded yet */
	Z_PROCESS_STATE_UNLOADED = 0,
	/** Extension loaded; process is ready to run but not yet started */
	Z_PROCESS_STATE_LOADED,
	/** Process thread is running */
	Z_PROCESS_STATE_RUNNING,
	/** Process thread has exited normally */
	Z_PROCESS_STATE_DEAD,
};

/**
 * @brief Configuration options for loading and running a process
 *
 * Initialise with @ref Z_PROCESS_OPTS_DEFAULT and then override individual
 * fields as needed.
 */
struct z_process_opts {
	/**
	 * Stack size for the process thread in bytes.
	 * 0 means use @kconfig{CONFIG_PROCESS_STACK_SIZE_DEFAULT}.
	 */
	size_t stack_size;

	/**
	 * Thread scheduling priority.
	 * Default: @kconfig{CONFIG_PROCESS_PRIORITY_DEFAULT}.
	 */
	int priority;

	/**
	 * Run the thread in unprivileged (user) mode.
	 * Has no effect when CONFIG_USERSPACE is disabled.
	 * Default: @c true when CONFIG_USERSPACE is enabled.
	 */
	bool user_mode;

	/**
	 * Name of the function to call inside the extension.
	 * The extension must export it with @ref LL_EXTENSION_SYMBOL.
	 * NULL defaults to @ref Z_PROCESS_ENTRY_SYM ("process_main").
	 */
	const char *entry_sym;

	/**
	 * Opaque argument passed to the process entry function.
	 * The extension receives it as the @c void *arg parameter.
	 */
	void *arg;
};

/**
 * @brief Default initialiser for @ref z_process_opts
 *
 * @code{.c}
 * struct z_process_opts opts = Z_PROCESS_OPTS_DEFAULT;
 * opts.priority = 3;
 * @endcode
 */
#define Z_PROCESS_OPTS_DEFAULT                                                 \
	{                                                                      \
		.stack_size = 0,                                               \
		.priority   = CONFIG_PROCESS_PRIORITY_DEFAULT,                 \
		.user_mode  = IS_ENABLED(CONFIG_USERSPACE),                    \
		.entry_sym  = Z_PROCESS_ENTRY_SYM,                            \
		.arg        = NULL,                                            \
	}

/**
 * @brief Process descriptor
 *
 * Holds all runtime state for one loaded process.  Must be allocated by the
 * caller (global, static, or on the heap) and must remain valid from
 * @ref z_process_load through @ref z_process_unload.
 *
 * @note Do not access this structure's fields directly; treat the descriptor
 *       as opaque and use the API functions.
 */
struct z_process {
	/** @cond INTERNAL_HIDDEN */
	char name[CONFIG_PROCESS_NAME_MAX_LEN + 1];
	struct llext *ext;
	llext_entry_fn_t entry_fn;

#ifdef CONFIG_USERSPACE
	struct k_mem_domain domain;
#endif

	struct k_thread thread;
	k_thread_stack_t *stack;
	size_t stack_size;

	volatile enum z_process_state state;
	int exit_code;

	struct z_process_opts opts;
	/** @endcond */
};

/**
 * @brief Define a stack suitable for a process thread
 *
 * This is a convenience alias for @ref K_THREAD_STACK_DEFINE.  Use it to
 * declare a properly-aligned stack at file scope.
 *
 * @param _name  C identifier for the stack variable
 * @param _size  Stack size in bytes
 */
#define Z_PROCESS_STACK_DEFINE(_name, _size) K_THREAD_STACK_DEFINE(_name, _size)

/**
 * @brief Load a process from an ELF image in memory
 *
 * Reads and links the ELF binary, sets up a dedicated memory domain with the
 * extension's code and data regions as partitions, and resolves the entry
 * function symbol inside the extension.
 *
 * The process will be in @ref Z_PROCESS_STATE_LOADED after this call
 * succeeds.  Call @ref z_process_start (or @ref z_process_spawn) to run it.
 *
 * @param proc       Uninitialized process descriptor
 * @param name       Human-readable name (truncated to
 *                   @kconfig{CONFIG_PROCESS_NAME_MAX_LEN} characters)
 * @param elf_data   Pointer to ELF binary data (must remain valid until
 *                   @ref z_process_unload is called when using persistent
 *                   storage, or may be freed after this call when using a
 *                   temporary buffer loader)
 * @param elf_size   Size of the ELF binary in bytes
 * @param stack      Stack memory; define with @ref Z_PROCESS_STACK_DEFINE
 * @param stack_size Size of the stack in bytes
 * @param opts       Load/run options; NULL selects all defaults
 *
 * @retval 0        Success
 * @retval -EINVAL  @p proc, @p elf_data, or @p name is NULL, or @p elf_size
 *                  is 0
 * @retval -ENOMEM  Not enough heap space to load the extension
 * @retval -ENOENT  Entry-function symbol not found in the extension
 * @retval <0       Other error from @ref llext_load or domain initialisation
 */
int z_process_load(struct z_process *proc, const char *name,
		   const void *elf_data, size_t elf_size,
		   k_thread_stack_t *stack, size_t stack_size,
		   const struct z_process_opts *opts);

/**
 * @brief Start a previously loaded process
 *
 * Creates the process thread and schedules it.  The process must be in
 * @ref Z_PROCESS_STATE_LOADED state.
 *
 * @param proc  Loaded process descriptor
 *
 * @retval 0       Success; process is now @ref Z_PROCESS_STATE_RUNNING
 * @retval -EINVAL Process is not in @ref Z_PROCESS_STATE_LOADED state
 */
int z_process_start(struct z_process *proc);

/**
 * @brief Load a process and start it immediately (convenience helper)
 *
 * Equivalent to calling @ref z_process_load followed by @ref z_process_start.
 *
 * @param proc       Uninitialized process descriptor
 * @param name       Human-readable name
 * @param elf_data   Pointer to ELF binary data
 * @param elf_size   Size of the ELF binary in bytes
 * @param stack      Stack memory; define with @ref Z_PROCESS_STACK_DEFINE
 * @param stack_size Size of the stack in bytes
 * @param opts       Load/run options; NULL selects all defaults
 *
 * @retval 0   Success
 * @retval <0  Error from @ref z_process_load or @ref z_process_start
 */
int z_process_spawn(struct z_process *proc, const char *name,
		    const void *elf_data, size_t elf_size,
		    k_thread_stack_t *stack, size_t stack_size,
		    const struct z_process_opts *opts);

/**
 * @brief Wait for a process to finish
 *
 * Blocks the caller until the process thread exits or the timeout expires.
 *
 * @param proc     Running or dead process descriptor
 * @param timeout  Maximum time to wait
 *
 * @retval 0       Process has exited; check @c proc->exit_code for the result
 * @retval -EAGAIN Timeout elapsed before the process exited
 * @retval -EINVAL Process is not in @ref Z_PROCESS_STATE_RUNNING or
 *                 @ref Z_PROCESS_STATE_DEAD state
 */
int z_process_join(struct z_process *proc, k_timeout_t timeout);

/**
 * @brief Force-terminate a running process
 *
 * Aborts the process thread.  Must be followed by @ref z_process_unload to
 * release resources.
 *
 * @param proc  Running process descriptor
 *
 * @retval 0       Success
 * @retval -EINVAL Process is not in @ref Z_PROCESS_STATE_RUNNING state
 */
int z_process_kill(struct z_process *proc);

/**
 * @brief Unload a process and free all resources
 *
 * Calls the extension's fini_array (C++ destructors etc.), unloads the LLEXT
 * memory regions, and resets the process descriptor to
 * @ref Z_PROCESS_STATE_UNLOADED.
 *
 * The process must **not** be in @ref Z_PROCESS_STATE_RUNNING state when this
 * is called.  Use @ref z_process_kill first if necessary.
 *
 * @param proc  Process descriptor in LOADED or DEAD state
 */
void z_process_unload(struct z_process *proc);

/**
 * @brief Return the current state of a process
 *
 * @param proc  Process descriptor (may be in any state)
 * @returns     @ref z_process_state value
 */
static inline enum z_process_state z_process_get_state(const struct z_process *proc)
{
	return proc->state;
}

/**
 * @brief Return the exit code of a completed process
 *
 * Valid only after the process has reached @ref Z_PROCESS_STATE_DEAD.
 *
 * @param proc  Dead process descriptor
 * @returns     Exit code set by the process (0 on normal completion)
 */
static inline int z_process_exit_code(const struct z_process *proc)
{
	return proc->exit_code;
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_PROCESS_PROCESS_H_ */
