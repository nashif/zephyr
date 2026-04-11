/*
 * Copyright (c) 2024 The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Kernel completion objects.
 *
 * A completion object is a lightweight one-shot (or reusable) synchronisation
 * primitive modelled after the Linux kernel completion API.  One or more
 * threads may block on a completion until another thread (or ISR) signals it
 * via k_completion_complete() or k_completion_complete_all().
 *
 * Semantics
 * ---------
 *  - k_completion_complete()     increments the internal counter by one and
 *                                 wakes the oldest pending thread (if any).
 *  - k_completion_complete_all() sets the counter to UINT_MAX and wakes all
 *                                 pending threads;  future k_completion_wait()
 *                                 calls pass through immediately.
 *  - k_completion_wait()         passes through when counter > 0 (decrement,
 *                                 unless UINT_MAX), otherwise pends.
 *  - k_completion_reset()        clears the counter and aborts any waiters
 *                                 with -EAGAIN, letting the object be reused.
 */

#include <zephyr/kernel.h>
#include <zephyr/kernel_structs.h>
#include <zephyr/toolchain.h>
#include <wait_q.h>
#include <ksched.h>
#include <zephyr/init.h>
#include <zephyr/internal/syscall_handler.h>
#include <zephyr/tracing/tracing.h>
#include <zephyr/sys/check.h>
#include <limits.h>

/* Single global spinlock, same strategy as semaphores / condvars. */
static struct k_spinlock lock;

#ifdef CONFIG_OBJ_CORE_COMPLETION
static struct k_obj_type obj_type_completion;
#endif /* CONFIG_OBJ_CORE_COMPLETION */

void z_impl_k_completion_init(struct k_completion *c)
{
	__ASSERT_NO_MSG(!arch_is_in_isr());

	c->done = 0U;
	z_waitq_init(&c->wait_q);

	SYS_PORT_TRACING_OBJ_INIT(k_completion, c, 0);

	k_object_init(c);

	sys_port_track_k_completion_init(c);

#ifdef CONFIG_OBJ_CORE_COMPLETION
	k_obj_core_init_and_link(K_OBJ_CORE(c), &obj_type_completion);
#endif /* CONFIG_OBJ_CORE_COMPLETION */
}

#ifdef CONFIG_USERSPACE
void z_vrfy_k_completion_init(struct k_completion *c)
{
	K_OOPS(K_SYSCALL_OBJ_INIT(c, K_OBJ_COMPLETION));
	z_impl_k_completion_init(c);
}
#include <zephyr/syscalls/k_completion_init_mrsh.c>
#endif /* CONFIG_USERSPACE */

void z_impl_k_completion_complete(struct k_completion *c)
{
	k_spinlock_key_t key = k_spin_lock(&lock);
	struct k_thread *thread;

	SYS_PORT_TRACING_OBJ_FUNC_ENTER(k_completion, complete, c);

	thread = z_unpend_first_thread(&c->wait_q);

	if (thread != NULL) {
		arch_thread_return_value_set(thread, 0);
		z_ready_thread(thread);
		SYS_PORT_TRACING_OBJ_FUNC_EXIT(k_completion, complete, c);
		z_reschedule(&lock, key);
	} else {
		if (c->done != UINT_MAX) {
			c->done++;
		}
		SYS_PORT_TRACING_OBJ_FUNC_EXIT(k_completion, complete, c);
		k_spin_unlock(&lock, key);
	}
}

#ifdef CONFIG_USERSPACE
void z_vrfy_k_completion_complete(struct k_completion *c)
{
	K_OOPS(K_SYSCALL_OBJ(c, K_OBJ_COMPLETION));
	z_impl_k_completion_complete(c);
}
#include <zephyr/syscalls/k_completion_complete_mrsh.c>
#endif /* CONFIG_USERSPACE */

void z_impl_k_completion_complete_all(struct k_completion *c)
{
	struct k_thread *thread;
	k_spinlock_key_t key = k_spin_lock(&lock);
	bool need_resched = false;

	SYS_PORT_TRACING_OBJ_FUNC_ENTER(k_completion, complete_all, c);

	c->done = UINT_MAX;

	while ((thread = z_unpend_first_thread(&c->wait_q)) != NULL) {
		arch_thread_return_value_set(thread, 0);
		z_ready_thread(thread);
		need_resched = true;
	}

	SYS_PORT_TRACING_OBJ_FUNC_EXIT(k_completion, complete_all, c);

	if (need_resched) {
		z_reschedule(&lock, key);
	} else {
		k_spin_unlock(&lock, key);
	}
}

#ifdef CONFIG_USERSPACE
void z_vrfy_k_completion_complete_all(struct k_completion *c)
{
	K_OOPS(K_SYSCALL_OBJ(c, K_OBJ_COMPLETION));
	z_impl_k_completion_complete_all(c);
}
#include <zephyr/syscalls/k_completion_complete_all_mrsh.c>
#endif /* CONFIG_USERSPACE */

int z_impl_k_completion_wait(struct k_completion *c, k_timeout_t timeout)
{
	k_spinlock_key_t key = k_spin_lock(&lock);
	int ret;

	__ASSERT(((arch_is_in_isr() == false) ||
		  K_TIMEOUT_EQ(timeout, K_NO_WAIT)), "");

	SYS_PORT_TRACING_OBJ_FUNC_ENTER(k_completion, wait, c, timeout);

	if (c->done > 0U) {
		if (c->done != UINT_MAX) {
			c->done--;
		}
		SYS_PORT_TRACING_OBJ_FUNC_EXIT(k_completion, wait, c, timeout, 0);
		k_spin_unlock(&lock, key);
		return 0;
	}

	if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
		SYS_PORT_TRACING_OBJ_FUNC_EXIT(k_completion, wait, c, timeout, -EBUSY);
		k_spin_unlock(&lock, key);
		return -EBUSY;
	}

	SYS_PORT_TRACING_OBJ_FUNC_BLOCKING(k_completion, wait, c, timeout);

	ret = z_pend_curr(&lock, key, &c->wait_q, timeout);

	SYS_PORT_TRACING_OBJ_FUNC_EXIT(k_completion, wait, c, timeout, ret);

	return ret;
}

#ifdef CONFIG_USERSPACE
int z_vrfy_k_completion_wait(struct k_completion *c, k_timeout_t timeout)
{
	K_OOPS(K_SYSCALL_OBJ(c, K_OBJ_COMPLETION));
	return z_impl_k_completion_wait(c, timeout);
}
#include <zephyr/syscalls/k_completion_wait_mrsh.c>
#endif /* CONFIG_USERSPACE */

void z_impl_k_completion_reset(struct k_completion *c)
{
	struct k_thread *thread;
	k_spinlock_key_t key = k_spin_lock(&lock);

	SYS_PORT_TRACING_OBJ_FUNC(k_completion, reset, c);

	c->done = 0U;

	while ((thread = z_unpend_first_thread(&c->wait_q)) != NULL) {
		arch_thread_return_value_set(thread, -EAGAIN);
		z_ready_thread(thread);
	}

	z_reschedule(&lock, key);
}

#ifdef CONFIG_USERSPACE
void z_vrfy_k_completion_reset(struct k_completion *c)
{
	K_OOPS(K_SYSCALL_OBJ(c, K_OBJ_COMPLETION));
	z_impl_k_completion_reset(c);
}
#include <zephyr/syscalls/k_completion_reset_mrsh.c>
#endif /* CONFIG_USERSPACE */

#ifdef CONFIG_OBJ_CORE_COMPLETION
static int init_completion_obj_core_list(void)
{
	z_obj_type_init(&obj_type_completion, K_OBJ_TYPE_COMPLETION_ID,
			offsetof(struct k_completion, obj_core));

	STRUCT_SECTION_FOREACH(k_completion, c) {
		k_obj_core_init_and_link(K_OBJ_CORE(c), &obj_type_completion);
	}

	return 0;
}

SYS_INIT(init_completion_obj_core_list, PRE_KERNEL_1,
	 CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);
#endif /* CONFIG_OBJ_CORE_COMPLETION */
