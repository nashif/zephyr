/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/printk.h>

struct k_eventflag evflag;

void main(void)
{
	u32_t flag;

	k_eventflag_init(&evflag);
	k_eventflag_set(&evflag, 0x100001U);
	k_eventflag_set(&evflag, 0x100010U);

	flag = k_eventflag_get(&evflag);

	printk("flags 0x%x\n", flag);

	k_eventflag_clear(&evflag, 0);
	flag = k_eventflag_get(&evflag);

	printk("flags after clearing 0x%x\n", flag);

}
