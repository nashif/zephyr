/*
 * Copyright (c) 2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_KERNEL_INCLUDE_TIMEOUT_Q_H_
#define ZEPHYR_KERNEL_INCLUDE_TIMEOUT_Q_H_

/**
 * @file
 * @brief timeout queue for threads on kernel objects
 */

#include <zephyr/kernel.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_SYS_CLOCK_EXISTS

/* Value written to dticks when timeout is aborted. */
#define TIMEOUT_DTICKS_ABORTED (IS_ENABLED(CONFIG_TIMEOUT_64BIT) ? INT64_MIN : INT32_MIN)

static inline void k_priv_init_timeout(struct _timeout *to)
{
	sys_dnode_init(&to->node);
}

/* Adds the timeout to the queue.
 *
 * @return Absolute tick value when timeout will expire.
 */
k_ticks_t k_priv_add_timeout(struct _timeout *to, _timeout_func_t fn, k_timeout_t timeout);

int k_priv_abort_timeout(struct _timeout *to);

static inline bool k_priv_is_inactive_timeout(const struct _timeout *to)
{
	return !sys_dnode_is_linked(&to->node);
}

static inline bool k_priv_is_aborted_timeout(const struct _timeout *to)
{
	/* When timeout is aborted then dticks is set to special value. */
	return to->dticks == TIMEOUT_DTICKS_ABORTED;
}

static inline void z_init_thread_timeout(struct _thread_base *thread_base)
{
	k_priv_init_timeout(&thread_base->timeout);
}

extern void k_priv_thread_timeout(struct _timeout *timeout);

static inline k_ticks_t k_prive_add_thread_timeout(struct k_thread *thread, k_timeout_t ticks)
{
	return k_priv_add_timeout(&thread->base.timeout, k_priv_thread_timeout, ticks);
}

static inline void k_priv_abort_thread_timeout(struct k_thread *thread)
{
	k_priv_abort_timeout(&thread->base.timeout);
}

static inline bool k_priv_is_aborted_thread_timeout(struct k_thread *thread)
{

	return k_priv_is_aborted_timeout(&thread->base.timeout);
}

int32_t k_priv_get_next_timeout_expiry(void);

k_ticks_t k_priv_timeout_remaining(const struct _timeout *timeout);

#else

/* Stubs when !CONFIG_SYS_CLOCK_EXISTS */
#define z_init_thread_timeout(thread_base) do {} while (false)
#define k_priv_abort_thread_timeout(to) do {} while (false)
#define k_priv_is_aborted_thread_timeout(to) false
#define k_priv_is_inactive_timeout(to) 1
#define k_priv_is_aborted_timeout(to) false
#define k_priv_get_next_timeout_expiry() ((int32_t) K_TICKS_FOREVER)
#define z_set_timeout_expiry(ticks, is_idle) do {} while (false)

static inline k_ticks_t k_prive_add_thread_timeout(struct k_thread *thread, k_timeout_t ticks)
{
	ARG_UNUSED(thread);
	ARG_UNUSED(ticks);
	return 0;
}

#endif /* CONFIG_SYS_CLOCK_EXISTS */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_KERNEL_INCLUDE_TIMEOUT_Q_H_ */
