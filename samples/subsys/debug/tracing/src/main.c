/*
 * Copyright (c) 2019 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

void main(void)
{
	u8_t count = 0;

	while (1) {
		printk("%c\n", (count++) % 2 ? 'A' : 'B');

		k_sleep(K_SECONDS(2));
	}
}
