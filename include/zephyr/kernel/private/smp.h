/*
 * Copyright (c) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_KERNEL_PRIVATE_SMP_H_
#define ZEPHYR_INCLUDE_KERNEL_PRIVATE_SMP_H_

struct k_thread;

/**
 * @internal
 */
#ifdef CONFIG_SOF
void z_smp_thread_init(void *arg, struct k_thread *thread);
void z_smp_thread_swap(void);
#endif

#endif

