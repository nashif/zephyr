/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <assert.h>
#include <zephyr/mctp/mctp.h>

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

	struct mctp_binding_serial *serial;
	struct mctp *mctp;

	mctp = mctp_init();
	assert(mctp != NULL);

	serial = mctp_serial_init();
	assert(serial);

	return 0;
}
