/*
 * Copyright (c) 2010-2014 Wind River Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file
 * @brief Nanokernel thread support
 *
 * This module provides general purpose thread support, with applies to both
 * tasks or fibers.
 */

#include <toolchain.h>
#include <sections.h>

#include <nano_private.h>
#include <misc/printk.h>
#include <sys_clock.h>
#include <drivers/system_timer.h>


nano_thread_id_t sys_thread_self_get(void)
{
	return _nanokernel.current;
}

nano_context_type_t sys_execution_context_type_get(void)
{
	if (_IS_IN_ISR())
		return NANO_CTX_ISR;

	if ((_nanokernel.current->flags & TASK) == TASK)
		return NANO_CTX_TASK;

	return NANO_CTX_FIBER;
}

/**
 *
 * @brief Mark thread as essential to system
 *
 * This function tags the running fiber or task as essential to system
 * option; exceptions raised by this thread will be treated as a fatal
 * system error.
 *
 * @return N/A
 */
void _thread_essential_set(void)
{
	_nanokernel.current->flags |= ESSENTIAL;
}

/**
 *
 * @brief Mark thread as not essential to system
 *
 * This function tags the running fiber or task as not essential to system
 * option; exceptions raised by this thread may be recoverable.
 * (This is the default tag for a thread.)
 *
 * @return N/A
 */
void _thread_essential_clear(void)
{
	_nanokernel.current->flags &= ~ESSENTIAL;
}

/**
 *
 * @brief Is the specified thread essential?
 *
 * This routine indicates if the specified thread is an essential system
 * thread.  A NULL thread pointer indicates that the current thread is
 * to be queried.
 *
 * @return Non-zero if specified thread is essential, zero if it is not
 */
int _is_thread_essential(struct tcs *pCtx /* pointer to thread */
					   )
{
	return ((pCtx == NULL) ? _nanokernel.current : pCtx)->flags & ESSENTIAL;
}

/*
 * Don't build sys_thread_busy_wait() for ARM, since intrinsics libraries in
 * current Zephyr SDK use non-Thumb code that isn't supported on Cortex-M CPUs.
 * For the time being any ARM-based application that attempts to use this API
 * will get a link error (which is preferable to a mysterious exception).
 *
 * @param usec_to_wait
 *
 * @return N/A
 */
#ifndef CONFIG_ARM
void sys_thread_busy_wait(uint32_t usec_to_wait)
{
	/* use 64-bit math to prevent overflow when multiplying */
	uint32_t cycles_to_wait = (uint32_t)(
		(uint64_t)usec_to_wait *
		(uint64_t)sys_clock_hw_cycles_per_sec /
		(uint64_t)USEC_PER_SEC
	);
	uint32_t start_cycles = sys_cycle_get_32();

	for (;;) {
		uint32_t current_cycles = sys_cycle_get_32();

		/* this handles the rollover on an unsigned 32-bit value */
		if ((current_cycles - start_cycles) >= cycles_to_wait) {
			break;
		}
	}
}
#endif /* CONFIG_ARM */

#ifdef CONFIG_THREAD_CUSTOM_DATA

/**
 *
 * @brief Set thread's custom data
 *
 * This routine sets the custom data value for the current task or fiber.
 * Custom data is not used by the kernel itself, and is freely available
 * for the thread to use as it sees fit.
 *
 * @param value New to set the thread's custom data to.
 *
 * @return N/A
 */
void sys_thread_custom_data_set(void *value)
{
	_nanokernel.current->custom_data = value;
}

/**
 *
 * @brief Get thread's custom data
 *
 * This function returns the custom data value for the current task or fiber.
 *
 * @return current handle value
 */
void *sys_thread_custom_data_get(void)
{
	return _nanokernel.current->custom_data;
}

#endif /* CONFIG_THREAD_CUSTOM_DATA */

#if defined(CONFIG_THREAD_MONITOR)
/**
 *
 * @brief Thread exit routine
 *
 * This function is invoked when the specified thread is aborted, either
 * normally or abnormally. It is called for the termination of any thread,
 * (fibers and tasks).
 *
 * This routine must be invoked either from a fiber or from a task with
 * interrupts locked to guarantee that the list of threads does not change in
 * mid-operation. It cannot be called from ISR context.
 *
 * @return N/A
 */
void _thread_exit(struct tcs *thread)
{
	/*
	 * Remove thread from the list of threads.  This singly linked list of
	 * threads maintains ALL the threads in the system: both tasks and
	 * fibers regardless of whether they are runnable.
	 */

	if (thread == _nanokernel.threads) {
		_nanokernel.threads = _nanokernel.threads->next_thread;
	} else {
		struct tcs *prev_thread;

		prev_thread = _nanokernel.threads;
		while (thread != prev_thread->next_thread) {
			prev_thread = prev_thread->next_thread;
		}
		prev_thread->next_thread = thread->next_thread;
	}
}
#endif /* CONFIG_THREAD_MONITOR */

/**
 *
 * @brief Common thread entry point function
 *
 * This function serves as the entry point for _all_ threads, i.e. both
 * task and fibers are instantiated such that initial execution starts
 * here.
 *
 * This routine invokes the actual task or fiber entry point function and
 * passes it three arguments.  It also handles graceful termination of the
 * task or fiber if the entry point function ever returns.
 *
 * @param pEntry address of the app entry point function
 * @param parameter1 1st arg to the app entry point function
 * @param parameter2 2nd arg to the app entry point function
 * @param parameter3 3rd arg to the app entry point function
 *
 * @internal
 * The 'noreturn' attribute is applied to this function so that the compiler
 * can dispense with generating the usual preamble that is only required for
 * functions that actually return.
 *
 * @return Does not return
 *
 */
FUNC_NORETURN void _thread_entry(_thread_entry_t pEntry,
					_thread_arg_t parameter1,
					_thread_arg_t parameter2,
					_thread_arg_t parameter3)
{
	/* Execute the "application" entry point function */

	pEntry(parameter1, parameter2, parameter3);

	/* Determine if thread can legally terminate itself via "return" */

	if (_is_thread_essential(NULL)) {
#ifdef CONFIG_NANOKERNEL
		/*
		 * Nanokernel's background task must always be present,
		 * so if it has nothing left to do just let it idle forever
		 */

		while (((_nanokernel.current)->flags & TASK) == TASK) {
			nano_cpu_idle();
		}
#endif /*  CONFIG_NANOKERNEL */

		/* Loss of essential thread is a system fatal error */

		_NanoFatalErrorHandler(_NANO_ERR_INVALID_TASK_EXIT,
				       &_default_esf);
	}

/* Gracefully terminate the currently executing thread */

#ifdef CONFIG_MICROKERNEL
	if (((_nanokernel.current)->flags & TASK) == TASK) {
		extern FUNC_NORETURN void _TaskAbort(void);
		_TaskAbort();
	} else
#endif /* CONFIG_MICROKERNEL */
	{
		fiber_abort();
	}

	/*
	 * Compiler can't tell that fiber_abort() won't return and issues
	 * a warning unless we explicitly tell it that control never gets this
	 * far.
	 */

	CODE_UNREACHABLE;
}
