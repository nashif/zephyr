/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <zephyr/types.h>
#include <zephyr/mctp/mctp.h>
#include <zephyr/mctp/mctp-serial.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(mctp, CONFIG_MCTP_LOG_LEVEL);


#define MCTP_EID 8

static void rx_message(uint8_t eid, bool tag_owner,
                       uint8_t msg_tag, void *data, void *msg,
                       size_t len)
{
}


int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

	struct mctp_binding_serial *serial;
	struct mctp *mctp;
	int rc;

	mctp = mctp_init();
	assert(mctp != NULL);

	serial = mctp_serial_init();
	assert(serial);

	mctp_register_bus(mctp, mctp_binding_serial_core(serial), MCTP_EID);
	mctp_set_rx_all(mctp, rx_message, NULL);

#if 0
        for (;;) {
                rc = mctp_serial_read(serial);
                if (rc) {
                        break;
		}
        }
#endif
	return 0;
}
