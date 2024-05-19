/* SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later */

#define _GNU_SOURCE

#include <zephyr/ztest.h>
#include "zephyr/mctp/mctp-serial.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(test);


#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct mctp_binding_serial_pipe {
	int ingress;
	int egress;

	struct mctp_binding_serial *serial;
};

static int mctp_binding_serial_pipe_tx(void *data, void *buf, size_t len)
{
	struct mctp_binding_serial_pipe *ctx = data;
	ssize_t rc;

	rc = write(ctx->egress, buf, len);
	zassert_true(rc >= 0);
	zassert_true((size_t)rc == len);

	return rc;
}

uint8_t mctp_msg_src[2 * MCTP_BTU];

static bool seen;
static bool received_tag_owner;
static uint8_t received_msg_tag;

static void rx_message(uint8_t eid __unused, bool tag_owner, uint8_t msg_tag,
		       void *data __unused, void *msg, size_t len)
{
	uint8_t type;

	type = *(uint8_t *)msg;

	LOG_DBG("MCTP message received: len %zd, type %d, tag %d", len,
		     type, msg_tag);

	zassert_true(sizeof(mctp_msg_src) == len);
	zassert_true(!memcmp(mctp_msg_src, msg, len));

	seen = true;
	received_msg_tag = msg_tag;
	received_tag_owner = tag_owner;
}

struct serial_test {
	struct mctp_binding_serial_pipe binding;
	struct mctp *mctp;
};

ZTEST(mctp_serial_pipe, test_serial_pipe)
{
	struct serial_test scenario[2];

	struct mctp_binding_serial_pipe *a;
	struct mctp_binding_serial_pipe *b;
	uint8_t msg_tag = 2;
	bool tag_owner = false;
	int p[2][2];
	int rc;

	memset(&mctp_msg_src[0], 0x5a, MCTP_BTU);
	memset(&mctp_msg_src[MCTP_BTU], 0xa5, MCTP_BTU);

	rc = pipe(p[0]);
	zassert_true(!rc);

	rc = pipe(p[1]);
	zassert_true(!rc);

	/* Instantiate the A side of the serial pipe */
	scenario[0].mctp = mctp_init();
	zassert_true(scenario[0].mctp);
	scenario[0].binding.serial = mctp_serial_init();
	zassert_true(scenario[0].binding.serial);
	a = &scenario[0].binding;
	a->ingress = p[0][0];
	a->egress = p[1][1];
	mctp_serial_open_fd(a->serial, a->ingress);
	mctp_serial_set_tx_fn(a->serial, mctp_binding_serial_pipe_tx, a);
	mctp_register_bus(scenario[0].mctp, mctp_binding_serial_core(a->serial),
			  8);

	/* Instantiate the B side of the serial pipe */
	scenario[1].mctp = mctp_init();
	zassert_true(scenario[1].mctp);
	mctp_set_rx_all(scenario[1].mctp, rx_message, NULL);
	scenario[1].binding.serial = mctp_serial_init();
	zassert_true(scenario[1].binding.serial);
	b = &scenario[1].binding;
	b->ingress = p[1][0];
	b->egress = p[0][1];
	mctp_serial_open_fd(b->serial, b->ingress);
	mctp_serial_set_tx_fn(b->serial, mctp_binding_serial_pipe_tx, a);
	mctp_register_bus(scenario[1].mctp, mctp_binding_serial_core(b->serial),
			  9);

	/* Transmit a message from A to B, with message tag */
	rc = mctp_message_tx(scenario[0].mctp, 9, tag_owner, msg_tag,
			     mctp_msg_src, sizeof(mctp_msg_src));
	zassert_true(rc == 0);

	/* Read the message at B from A */
	seen = false;
	received_tag_owner = true;
	received_msg_tag = 0;
	mctp_serial_read(b->serial);
	zassert_true(seen);
	zassert_true(received_tag_owner == tag_owner);
	zassert_true(received_msg_tag == msg_tag);

	mctp_serial_destroy(scenario[1].binding.serial);
	mctp_destroy(scenario[1].mctp);
	mctp_serial_destroy(scenario[0].binding.serial);
	mctp_destroy(scenario[0].mctp);
}

ZTEST_SUITE(mctp_serial_pipe, NULL, NULL, NULL, NULL, NULL);
