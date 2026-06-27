/*
 * Copyright (c) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

/*
 * These are build-only tests: their purpose is to confirm that the tracing
 * instrumentation compiles and links across the different tracing formats and
 * with each per-subsystem tracing option toggled. Touch a few kernel objects
 * so that the corresponding sys_port_trace_* macros are also expanded from
 * application code, not only from inside the kernel.
 */

static K_SEM_DEFINE(sem, 0, 1);
static K_MUTEX_DEFINE(mutex);
static struct k_timer timer;

int main(void)
{
	k_timer_init(&timer, NULL, NULL);

	k_sem_give(&sem);
	(void)k_sem_take(&sem, K_NO_WAIT);

	(void)k_mutex_lock(&mutex, K_NO_WAIT);
	(void)k_mutex_unlock(&mutex);

	return 0;
}
