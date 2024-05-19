/* SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later */

#include <zephyr/ztest.h>

#define _GNU_SOURCE

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <zephyr/types.h>
#include "zephyr/mctp/mctp.h"
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(test);

#include "test-utils.h"

#define TEST_DEST_EID 9
#define TEST_SRC_EID  10

#define MAX_PAYLOAD_SIZE 50000

struct pktbuf {
	struct mctp_hdr hdr;
	uint8_t *payload;
};

struct test_params {
	bool seen;
	size_t message_size;
	uint8_t msg_tag;
	bool tag_owner;
};

static void rx_message(uint8_t eid __unused, bool tag_owner, uint8_t msg_tag,
		       void *data, void *msg __unused, size_t len)
{
	struct test_params *param = (struct test_params *)data;

	LOG_DBG("MCTP message received: len %zd, tag %u", len, msg_tag);

	param->seen = true;
	param->message_size = len;
	param->msg_tag = msg_tag;
	param->tag_owner = tag_owner;
}

static uint8_t get_sequence()
{
	static uint8_t pkt_seq = 0;

	return (pkt_seq++ % 4);
}

static uint8_t get_tag()
{
	static uint8_t tag = 0;

	return (tag++ % 8);
}

/*
 * receive_pktbuf bypasses all bindings and directly invokes mctp_bus_rx.
 * This is necessary in order invoke test cases on the core functionality.
 * The memory allocated for the mctp packet is capped at MCTP_BTU
 * size, however, the mimiced rx pkt still retains the len parameter.
 * This allows to mimic packets larger than a sane memory allocator can
 * provide.
 */
static void receive_ptkbuf(struct mctp_binding_test *binding,
			   const struct pktbuf *pktbuf, size_t len)
{
	size_t alloc_size = MIN((size_t)MCTP_BTU, len);
	struct mctp_pktbuf *rx_pkt;

	rx_pkt = __mctp_alloc(sizeof(*rx_pkt) + MCTP_PACKET_SIZE(alloc_size));
	zassert_true(rx_pkt);

	/* Preserve passed len parameter */
	rx_pkt->size = MCTP_PACKET_SIZE(len);
	rx_pkt->start = 0;
	rx_pkt->end = MCTP_PACKET_SIZE(len);
	rx_pkt->mctp_hdr_off = 0;
	rx_pkt->next = NULL;
	memcpy(rx_pkt->data, &pktbuf->hdr, sizeof(pktbuf->hdr));
	memcpy(rx_pkt->data + sizeof(pktbuf->hdr), pktbuf->payload, alloc_size);

	mctp_bus_rx((struct mctp_binding *)binding, rx_pkt);
}

static void receive_one_fragment(struct mctp_binding_test *binding,
				 uint8_t *payload, size_t fragment_size,
				 uint8_t flags_seq_tag, struct pktbuf *pktbuf)
{
	pktbuf->hdr.flags_seq_tag = flags_seq_tag;
	pktbuf->payload = payload;
	receive_ptkbuf(binding, pktbuf, fragment_size);
}

static void receive_two_fragment_message(struct mctp_binding_test *binding,
					 uint8_t *payload,
					 size_t fragment1_size,
					 size_t fragment2_size,
					 struct pktbuf *pktbuf)
{
	uint8_t tag = MCTP_HDR_FLAG_TO | get_tag();
	uint8_t flags_seq_tag;

	flags_seq_tag = MCTP_HDR_FLAG_SOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, payload, fragment1_size, flags_seq_tag,
			     pktbuf);

	flags_seq_tag = MCTP_HDR_FLAG_EOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, payload + fragment1_size, fragment2_size,
			     flags_seq_tag, pktbuf);
}

/**
 * @brief Test the reception of a single fragment message
 *
 * @details Test the reception of a single fragment message
 * with a size of MCTP_BTU bytes.
 */
ZTEST(mctp_core, mctp_core_test_simple_rx)
{
	struct mctp *mctp = NULL;
	struct mctp_binding_test *binding = NULL;
	struct test_params test_param;
	uint8_t test_payload[2 * MCTP_BTU];
	struct pktbuf pktbuf;

	memset(test_payload, 0, sizeof(test_payload));
	test_param.seen = false;
	test_param.message_size = 0;
	mctp_test_stack_init(&mctp, &binding, TEST_DEST_EID);
	mctp_set_rx_all(mctp, rx_message, &test_param);
	memset(&pktbuf, 0, sizeof(pktbuf));
	pktbuf.hdr.dest = TEST_DEST_EID;
	pktbuf.hdr.src = TEST_SRC_EID;

	/* Receive 2 fragments of equal size */
	receive_two_fragment_message(binding, test_payload, MCTP_BTU, MCTP_BTU,
				     &pktbuf);

	zassert_true(test_param.seen);
	zassert_true(test_param.message_size == 2 * MCTP_BTU);

	mctp_binding_test_destroy(binding);
	mctp_destroy(mctp);
}

ZTEST(mctp_core, mctp_core_test_receive_equal_length_fragments)
{
	struct mctp *mctp = NULL;
	struct mctp_binding_test *binding = NULL;
	struct test_params test_param;
	static uint8_t test_payload[MAX_PAYLOAD_SIZE];
	uint8_t tag = MCTP_HDR_FLAG_TO | get_tag();
	struct pktbuf pktbuf;
	uint8_t flags_seq_tag;

	memset(test_payload, 0, sizeof(test_payload));
	test_param.seen = false;
	test_param.message_size = 0;
	mctp_test_stack_init(&mctp, &binding, TEST_DEST_EID);
	mctp_set_rx_all(mctp, rx_message, &test_param);
	memset(&pktbuf, 0, sizeof(pktbuf));
	pktbuf.hdr.dest = TEST_DEST_EID;
	pktbuf.hdr.src = TEST_SRC_EID;

	/* Receive 3 fragments, each of size MCTP_BTU */
	flags_seq_tag = MCTP_HDR_FLAG_SOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload, MCTP_BTU, flags_seq_tag,
			     &pktbuf);

	flags_seq_tag = (get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload + MCTP_BTU, MCTP_BTU,
			     flags_seq_tag, &pktbuf);

	flags_seq_tag = MCTP_HDR_FLAG_EOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload + (2 * MCTP_BTU), MCTP_BTU,
			     flags_seq_tag, &pktbuf);

	zassert_true(test_param.seen);
	zassert_true(test_param.message_size == 3 * MCTP_BTU);

	mctp_binding_test_destroy(binding);
	mctp_destroy(mctp);
}

ZTEST(mctp_core, mctp_core_test_receive_unexpected_smaller_middle_fragment)
{
	struct mctp *mctp = NULL;
	struct mctp_binding_test *binding = NULL;
	struct test_params test_param;
	static uint8_t test_payload[MAX_PAYLOAD_SIZE];
	uint8_t tag = MCTP_HDR_FLAG_TO | get_tag();
	struct pktbuf pktbuf;
	uint8_t flags_seq_tag;

	memset(test_payload, 0, sizeof(test_payload));
	test_param.seen = false;
	test_param.message_size = 0;
	mctp_test_stack_init(&mctp, &binding, TEST_DEST_EID);
	mctp_set_rx_all(mctp, rx_message, &test_param);
	memset(&pktbuf, 0, sizeof(pktbuf));
	pktbuf.hdr.dest = TEST_DEST_EID;
	pktbuf.hdr.src = TEST_SRC_EID;

	/* Middle fragment with size MCTP_BTU - 1 */
	flags_seq_tag = MCTP_HDR_FLAG_SOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload, MCTP_BTU, flags_seq_tag,
			     &pktbuf);

	flags_seq_tag = (get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload + MCTP_BTU, MCTP_BTU - 1,
			     flags_seq_tag, &pktbuf);

	flags_seq_tag = MCTP_HDR_FLAG_EOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload + (2 * MCTP_BTU), MCTP_BTU,
			     flags_seq_tag, &pktbuf);

	zassert_true(!test_param.seen);

	mctp_binding_test_destroy(binding);
	mctp_destroy(mctp);
}

ZTEST( mctp_core, mctp_core_test_receive_unexpected_bigger_middle_fragment)
{
	struct mctp *mctp = NULL;
	struct mctp_binding_test *binding = NULL;
	struct test_params test_param;
	static uint8_t test_payload[MAX_PAYLOAD_SIZE];
	uint8_t tag = MCTP_HDR_FLAG_TO | get_tag();
	struct pktbuf pktbuf;
	uint8_t flags_seq_tag;

	memset(test_payload, 0, sizeof(test_payload));
	test_param.seen = false;
	test_param.message_size = 0;
	mctp_test_stack_init(&mctp, &binding, TEST_DEST_EID);
	mctp_set_rx_all(mctp, rx_message, &test_param);
	memset(&pktbuf, 0, sizeof(pktbuf));
	pktbuf.hdr.dest = TEST_DEST_EID;
	pktbuf.hdr.src = TEST_SRC_EID;

	/* Middle fragment with size MCTP_BTU + 1 */
	flags_seq_tag = MCTP_HDR_FLAG_SOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload, MCTP_BTU, flags_seq_tag,
			     &pktbuf);

	flags_seq_tag = (get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload + MCTP_BTU, MCTP_BTU + 1,
			     flags_seq_tag, &pktbuf);

	flags_seq_tag = MCTP_HDR_FLAG_EOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload + (2 * MCTP_BTU), MCTP_BTU,
			     flags_seq_tag, &pktbuf);

	zassert_true(!test_param.seen);

	mctp_binding_test_destroy(binding);
	mctp_destroy(mctp);
}

ZTEST(mctp_core, mctp_core_test_receive_smaller_end_fragment)
{
	struct mctp *mctp = NULL;
	struct mctp_binding_test *binding = NULL;
	struct test_params test_param;
	static uint8_t test_payload[MAX_PAYLOAD_SIZE];
	uint8_t tag = MCTP_HDR_FLAG_TO | get_tag();
	uint8_t end_frag_size = MCTP_BTU - 10;
	struct pktbuf pktbuf;
	uint8_t flags_seq_tag;

	memset(test_payload, 0, sizeof(test_payload));
	test_param.seen = false;
	test_param.message_size = 0;
	mctp_test_stack_init(&mctp, &binding, TEST_DEST_EID);
	mctp_set_rx_all(mctp, rx_message, &test_param);
	memset(&pktbuf, 0, sizeof(pktbuf));
	pktbuf.hdr.dest = TEST_DEST_EID;
	pktbuf.hdr.src = TEST_SRC_EID;

	flags_seq_tag = MCTP_HDR_FLAG_SOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload, MCTP_BTU, flags_seq_tag,
			     &pktbuf);

	flags_seq_tag = (get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload + MCTP_BTU, MCTP_BTU,
			     flags_seq_tag, &pktbuf);

	flags_seq_tag = MCTP_HDR_FLAG_EOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload + (2 * MCTP_BTU),
			     end_frag_size, flags_seq_tag, &pktbuf);

	zassert_true(test_param.seen);
	zassert_true(test_param.message_size ==
	       (size_t)(2 * MCTP_BTU + end_frag_size));

	mctp_binding_test_destroy(binding);
	mctp_destroy(mctp);
}

ZTEST(mctp_core, mctp_core_test_receive_bigger_end_fragment)
{
	struct mctp *mctp = NULL;
	struct mctp_binding_test *binding = NULL;
	struct test_params test_param;
	static uint8_t test_payload[MAX_PAYLOAD_SIZE];
	uint8_t tag = MCTP_HDR_FLAG_TO | get_tag();
	uint8_t end_frag_size = MCTP_BTU + 10;
	struct pktbuf pktbuf;
	uint8_t flags_seq_tag;

	memset(test_payload, 0, sizeof(test_payload));
	test_param.seen = false;
	test_param.message_size = 0;
	mctp_test_stack_init(&mctp, &binding, TEST_DEST_EID);
	mctp_set_rx_all(mctp, rx_message, &test_param);
	memset(&pktbuf, 0, sizeof(pktbuf));
	pktbuf.hdr.dest = TEST_DEST_EID;
	pktbuf.hdr.src = TEST_SRC_EID;

	flags_seq_tag = MCTP_HDR_FLAG_SOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload, MCTP_BTU, flags_seq_tag,
			     &pktbuf);

	flags_seq_tag = (get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload + MCTP_BTU, MCTP_BTU,
			     flags_seq_tag, &pktbuf);

	flags_seq_tag = MCTP_HDR_FLAG_EOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload + (2 * MCTP_BTU),
			     end_frag_size, flags_seq_tag, &pktbuf);

	zassert_true(!test_param.seen);

	mctp_binding_test_destroy(binding);
	mctp_destroy(mctp);
}

ZTEST(mctp_core, mctp_core_test_drop_large_fragments)
{
	struct mctp *mctp = NULL;
	struct mctp_binding_test *binding = NULL;
	struct test_params test_param;
	static uint8_t test_payload[MAX_PAYLOAD_SIZE];
	struct pktbuf pktbuf;

	memset(test_payload, 0, sizeof(test_payload));
	test_param.seen = false;
	test_param.message_size = 0;
	mctp_test_stack_init(&mctp, &binding, TEST_DEST_EID);
	mctp_set_rx_all(mctp, rx_message, &test_param);
	memset(&pktbuf, 0, sizeof(pktbuf));
	pktbuf.hdr.dest = TEST_DEST_EID;
	pktbuf.hdr.src = TEST_SRC_EID;

	/* Receive a large payload - first fragment with MCTP_BTU bytes,
	* 2nd fragment of SIZE_MAX */

	receive_two_fragment_message(binding, test_payload, MCTP_BTU,
				     SIZE_MAX - sizeof(struct mctp_hdr),
				     &pktbuf);

	zassert_true(!test_param.seen);

	mctp_binding_test_destroy(binding);
	mctp_destroy(mctp);
}

ZTEST(mctp_core, mctp_core_test_exhaust_context_buffers)
{
	struct mctp *mctp = NULL;
	struct mctp_binding_test *binding = NULL;
	struct test_params test_param;
	static uint8_t test_payload[MAX_PAYLOAD_SIZE];
	uint8_t tag = MCTP_HDR_FLAG_TO | get_tag();
	uint8_t i = 0;
	const uint8_t max_context_buffers = 16;
	struct pktbuf pktbuf;
	uint8_t flags_seq_tag;

	memset(test_payload, 0, sizeof(test_payload));
	test_param.seen = false;
	test_param.message_size = 0;
	mctp_test_stack_init(&mctp, &binding, TEST_DEST_EID);
	mctp_set_rx_all(mctp, rx_message, &test_param);
	memset(&pktbuf, 0, sizeof(pktbuf));
	pktbuf.hdr.dest = TEST_DEST_EID;
	pktbuf.hdr.src = TEST_SRC_EID;

	/* Exhaust all 16 context buffers*/
	for (i = 0; i < max_context_buffers; i++) {
		flags_seq_tag = MCTP_HDR_FLAG_SOM |
				(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
		receive_one_fragment(binding, test_payload, MCTP_BTU,
				     flags_seq_tag, &pktbuf);

		/* Change source EID so that different contexts are created */
		pktbuf.hdr.src++;
	}

	/* Send a full message from a different EID */
	pktbuf.hdr.src++;
	receive_two_fragment_message(binding, test_payload, MCTP_BTU, MCTP_BTU,
				     &pktbuf);

	/* Message assembly should fail */
	zassert_true(!test_param.seen);

	/* Complete message assembly for one of the messages */
	pktbuf.hdr.src -= max_context_buffers;
	flags_seq_tag = MCTP_HDR_FLAG_EOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) | tag;
	receive_one_fragment(binding, test_payload, MCTP_BTU, flags_seq_tag,
			     &pktbuf);

	zassert_true(test_param.seen);
	zassert_true(test_param.message_size == (2 * MCTP_BTU));

	mctp_binding_test_destroy(binding);
	mctp_destroy(mctp);
}

ZTEST(mctp_core, mctp_core_test_rx_with_tag)
{
	struct mctp *mctp = NULL;
	struct mctp_binding_test *binding = NULL;
	struct test_params test_param;
	static uint8_t test_payload[MCTP_BTU];
	uint8_t tag = get_tag();
	struct pktbuf pktbuf;
	uint8_t flags_seq_tag;

	memset(test_payload, 0, sizeof(test_payload));
	test_param.seen = false;
	test_param.message_size = 0;
	test_param.msg_tag = 0;
	test_param.tag_owner = false;

	mctp_test_stack_init(&mctp, &binding, TEST_DEST_EID);
	mctp_set_rx_all(mctp, rx_message, &test_param);
	memset(&pktbuf, 0, sizeof(pktbuf));
	pktbuf.hdr.dest = TEST_DEST_EID;
	pktbuf.hdr.src = TEST_SRC_EID;

	/* Set tag and tag owner fields for a recieve packet */
	flags_seq_tag = MCTP_HDR_FLAG_SOM | MCTP_HDR_FLAG_EOM |
			(1 << MCTP_HDR_TO_SHIFT) | tag;
	receive_one_fragment(binding, test_payload, MCTP_BTU, flags_seq_tag,
			     &pktbuf);

	zassert_true(test_param.seen);
	zassert_true(test_param.message_size == (MCTP_BTU));
	zassert_true(test_param.msg_tag == tag);
	zassert_true(test_param.tag_owner);

	mctp_binding_test_destroy(binding);
	mctp_destroy(mctp);
}

ZTEST(mctp_core, mctp_core_test_rx_with_tag_multifragment)
{
	struct mctp *mctp = NULL;
	struct mctp_binding_test *binding = NULL;
	struct test_params test_param;
	static uint8_t test_payload[MCTP_BTU];
	uint8_t tag = get_tag();
	struct pktbuf pktbuf;
	uint8_t flags_seq_tag;

	memset(test_payload, 0, sizeof(test_payload));
	test_param.seen = false;
	test_param.message_size = 0;
	test_param.msg_tag = 0;
	test_param.tag_owner = false;

	mctp_test_stack_init(&mctp, &binding, TEST_DEST_EID);
	mctp_set_rx_all(mctp, rx_message, &test_param);
	memset(&pktbuf, 0, sizeof(pktbuf));
	pktbuf.hdr.dest = TEST_DEST_EID;
	pktbuf.hdr.src = TEST_SRC_EID;

	/* Set tag and tag owner fields for a 3 fragment packet */
	flags_seq_tag = MCTP_HDR_FLAG_SOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) |
			(1 << MCTP_HDR_TO_SHIFT) | tag;
	receive_one_fragment(binding, test_payload, MCTP_BTU, flags_seq_tag,
			     &pktbuf);

	flags_seq_tag = (get_sequence() << MCTP_HDR_SEQ_SHIFT) |
			(1 << MCTP_HDR_TO_SHIFT) | tag;
	receive_one_fragment(binding, test_payload, MCTP_BTU, flags_seq_tag,
			     &pktbuf);

	flags_seq_tag = MCTP_HDR_FLAG_EOM |
			(get_sequence() << MCTP_HDR_SEQ_SHIFT) |
			(1 << MCTP_HDR_TO_SHIFT) | tag;
	receive_one_fragment(binding, test_payload, MCTP_BTU, flags_seq_tag,
			     &pktbuf);

	zassert_true(test_param.seen);
	zassert_true(test_param.message_size == (3 * MCTP_BTU));
	zassert_true(test_param.msg_tag == tag);
	zassert_true(test_param.tag_owner);

	mctp_binding_test_destroy(binding);
	mctp_destroy(mctp);
}

ZTEST_SUITE(mctp_core, NULL, NULL, NULL, NULL, NULL);
