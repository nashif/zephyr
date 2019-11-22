/*
 * Copyright (c) 2019 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <wait_q.h>
#include <ksched.h>

void z_impl_k_eventflag_init(struct k_eventflag *ev_flag)
{
	z_waitq_init(&ev_flag->wait_q);
	ev_flag->flags = 0U;
	z_object_init(ev_flag);

}

void z_impl_k_eventflag_set(struct k_eventflag *ev_flag, u32_t flags)
{
	ev_flag->flags |= flags;
}


u32_t z_impl_k_eventflag_get(struct k_eventflag *ev_flag)
{
	return ev_flag->flags;
}

void z_impl_k_eventflag_clear(struct k_eventflag *ev_flag, u32_t flags)
{
	if (flags == 0U) {
		flags = 0x7fffffff;
	}
	ev_flag->flags &= ~(flags);


}
