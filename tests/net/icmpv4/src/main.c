/* main.c - Application main entry point */

/*
 * Copyright (c) 2019 Intel Corporation
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_test, CONFIG_NET_ICMPV4_LOG_LEVEL);

#include <errno.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/sys/printk.h>
#include <zephyr/linker/sections.h>

#include <zephyr/tc_util.h>

#include <zephyr/net_buf.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/dummy.h>
#include <zephyr/net/icmp.h>

#include "net_private.h"
#include "icmpv4.h"
#include "ipv4.h"

#include <zephyr/ztest.h>

static const unsigned char icmpv4_echo_req[] = {
	/* IPv4 Header */
	0x45, 0x00, 0x00, 0x54, 0xea, 0x8c, 0x40, 0x00,
	0x40, 0x01, 0xcc, 0x18, 0xc0, 0x00, 0x02, 0x02,
	0xc0, 0x00, 0x02, 0x01,
	/* ICMP Header (Echo Request) */
	0x08, 0x00, 0xe3, 0x7c,
	0x10, 0x63, 0x00, 0x01,
	/* Payload */
	0xb8, 0xa4, 0x8c, 0x5d, 0x00, 0x00, 0x00, 0x00,
	0xfc, 0x49, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37 };

static const unsigned char icmpv4_echo_rep[] = {
	/* IPv4 Header */
	0x45, 0x00, 0x00, 0x20, 0x75, 0xac, 0x00, 0x00,
	0x40, 0x01, 0x81, 0x2d, 0xc0, 0x00, 0x02, 0x02,
	0xc0, 0x00, 0x02, 0x01,
	/* ICMP Header (Echo Reply)*/
	0x00, 0x00, 0x91, 0x12,
	0x16, 0x50, 0x00, 0x00, 0x01, 0xfd, 0x56, 0xa0 };

static const unsigned char icmpv4_echo_req_opt[] = {
	/* IPv4 Header */
	0x4e, 0x00, 0x00, 0x78, 0xe1, 0x4b, 0x40, 0x00,
	0x40, 0x01, 0x9a, 0x83, 0xc0, 0x00, 0x02, 0x02,
	0xc0, 0x00, 0x02, 0x01,
	/* IPv4 Header Options (Timestamp) */
	0x44, 0x24, 0x0d, 0x01, 0xc0, 0x00, 0x02, 0x02,
	0x02, 0x4d, 0x1c, 0x3d, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	/* ICMP Header (Echo Request) */
	0x08, 0x00, 0x35, 0xbf,
	0x5d, 0xe7, 0x00, 0x01,
	0xcf, 0xe7, 0x8d, 0x5d, 0x00, 0x00, 0x00, 0x00,
	/* Payload */
	0x3a, 0x40, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37 };

static const unsigned char icmpv4_echo_req_opt_bad[] = {
	/* IPv4 Header */
	0x46, 0x00, 0x00, 0xa0, 0xf8, 0x6c, 0x00, 0x00,
	0x64, 0x01, 0x56, 0xa8, 0xc0, 0x00, 0x02, 0x02,
	0xc0, 0x00, 0x02, 0x01,

	/* IPv4 Header Options (Wrong length) */
	0x41, 0x03, 0x41, 0x41,

	/* ICMP Header (Echo Request) */
	0x08, 0x00, 0x06, 0xb8, 0x30, 0x31, 0x32, 0x07,
	/* Payload */
	0x80, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41,
	0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x41, 0x00 };

#define TEST_ICMPV4_UNKNOWN  0
#define TEST_ICMPV4_ECHO_REQ  1
#define TEST_ICMPV4_ECHO_REQ_OPTS 2
#define TEST_ICMPV4_PARAM_PROBLEM 3

/* Offset of the octet a Parameter Problem about icmpv4_echo_req_opt_bad
 * points to: the type of its second option, which lacks a length octet.
 */
#define BAD_OPT_POINTER 23
#define BAD_OPT_HDR_LEN 24

static bool param_problem_received;

static uint8_t current = TEST_ICMPV4_UNKNOWN;
static struct net_in_addr my_addr  = { { { 192, 0, 2, 1 } } };
static struct net_if *net_iface;

static enum net_verdict handle_reply_msg(struct net_icmp_ctx *ctx,
					 struct net_pkt *pkt,
					 struct net_icmp_ip_hdr *hdr,
					 struct net_icmp_hdr *icmp_hdr,
					 void *user_data)
{
	ARG_UNUSED(ctx);
	ARG_UNUSED(hdr);
	ARG_UNUSED(icmp_hdr);
	ARG_UNUSED(user_data);

	if (net_pkt_get_len(pkt) != sizeof(icmpv4_echo_rep)) {
		return NET_DROP;
	}

	return NET_OK;
}

struct net_icmpv4_context {
	uint8_t mac_addr[sizeof(struct net_eth_addr)];
	struct net_linkaddr ll_addr;
};

static int net_icmpv4_dev_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static uint8_t *net_icmpv4_get_mac(const struct device *dev)
{
	struct net_icmpv4_context *context = dev->data;

	if (context->mac_addr[2] == 0x00) {
		/* 00-00-5E-00-53-xx Documentation RFC 7042 */
		context->mac_addr[0] = 0x00;
		context->mac_addr[1] = 0x00;
		context->mac_addr[2] = 0x5E;
		context->mac_addr[3] = 0x00;
		context->mac_addr[4] = 0x53;
		context->mac_addr[5] = 0x01;
	}

	return context->mac_addr;
}

static void net_icmpv4_iface_init(struct net_if *iface)
{
	uint8_t *mac = net_icmpv4_get_mac(net_if_get_device(iface));

	net_if_set_link_addr(iface, mac, 6, NET_LINK_ETHERNET);
}

static int verify_echo_reply(struct net_pkt *pkt)
{
	struct net_icmp_hdr icmp_hdr;
	uint8_t buf[60];
	int ret;
	uint8_t payload_len;

	net_pkt_set_overwrite(pkt, true);
	net_pkt_cursor_init(pkt);

	ret = net_pkt_skip(pkt, NET_IPV4H_LEN);
	if (ret != 0) {
		zassert_true(false, "echo_reply skip failed");
	}

	/* EchoReply Code and Type is 0 */
	ret = net_pkt_read(pkt, &icmp_hdr, sizeof(struct net_icmp_hdr));
	if (ret != 0) {
		zassert_true(false, "echo_reply read failed");
	}

	if (icmp_hdr.code != 0 || icmp_hdr.type != 0) {
		zassert_true(false, "echo_reply invalid type or code");
	}

	/* Calculate payload length */
	payload_len = sizeof(icmpv4_echo_req) -
		      NET_IPV4H_LEN - NET_ICMPH_LEN;
	if (payload_len != net_pkt_remaining_data(pkt)) {
		zassert_true(false, "echo_reply invalid payload len");
	}

	ret = net_pkt_read(pkt, buf, payload_len);
	if (ret != 0) {
		zassert_true(false, "echo_reply read payload failed");
	}

	/* Compare the payload */
	if (memcmp(buf, icmpv4_echo_req + NET_IPV4H_LEN + NET_ICMPH_LEN,
		   payload_len)) {
		zassert_true(false, "echo_reply invalid payload");
	}

	/* Options length should be zero */
	if (net_pkt_ipv4_opts_len(pkt)) {
		zassert_true(false, "echo_reply invalid opts len");
	}

	return 0;
}

static int verify_echo_reply_with_opts(struct net_pkt *pkt)
{
	struct net_icmp_hdr icmp_hdr;
	uint8_t buf[60];
	int ret;
	uint8_t vhl;
	uint8_t opts_len;
	uint8_t payload_len;

	net_pkt_set_overwrite(pkt, true);
	net_pkt_cursor_init(pkt);

	ret = net_pkt_read_u8(pkt, &vhl);
	if (ret != 0) {
		zassert_true(false, "echo_reply_opts read failed");
	}

	vhl = (vhl & NET_IPV4_IHL_MASK) * 4U;
	opts_len = vhl - sizeof(struct net_ipv4_hdr);
	if (opts_len == 0) {
		zassert_true(false, "echo_reply_opts wrong opts len");
	}

	ret = net_pkt_skip(pkt, NET_IPV4H_LEN - 1U + opts_len);
	if (ret != 0) {
		zassert_true(false, "echo_reply_opts skip failed");
	}

	/* EchoReply Code and Type is 0 */
	ret = net_pkt_read(pkt, &icmp_hdr, sizeof(struct net_icmp_hdr));
	if (ret != 0) {
		zassert_true(false, "echo_reply_opts read failed");
	}

	if (icmp_hdr.code != 0 || icmp_hdr.type != 0) {
		zassert_true(false, "echo_reply_opts wrong code and type");
	}

	/* Calculate payload length */
	payload_len = sizeof(icmpv4_echo_req_opt) -
		      NET_IPV4H_LEN - NET_ICMPH_LEN - opts_len;
	if (payload_len != net_pkt_remaining_data(pkt)) {
		zassert_true(false, "echo_reply_opts invalid payload len");
	}

	ret = net_pkt_read(pkt, buf, payload_len);
	if (ret != 0) {
		zassert_true(false, "echo_reply_opts read payload failed");
	}

	/* Compare the payload */
	if (memcmp(buf, icmpv4_echo_req_opt +
		   NET_IPV4H_LEN + NET_ICMPH_LEN + opts_len,
		   payload_len)) {
		zassert_true(false, "echo_reply_opts invalid payload");
	}

	/* Options length should not be zero */
	if (net_pkt_ipv4_opts_len(pkt) != opts_len) {
		zassert_true(false, "echo_reply_opts wrong opts len");
	}

	return 0;
}

/* A malformed option is answered with a Parameter Problem that points at
 * the offending octet and carries the original header, options included,
 * plus the first 8 octets of the datagram. RFC 792, RFC 1122 ch. 3.2.2.
 */
static int verify_param_problem(struct net_pkt *pkt)
{
	struct net_icmp_hdr icmp_hdr;
	uint8_t buf[BAD_OPT_HDR_LEN + 8];
	uint8_t pointer[4];

	net_pkt_set_overwrite(pkt, true);
	net_pkt_cursor_init(pkt);

	zassert_ok(net_pkt_skip(pkt, NET_IPV4H_LEN), "param problem skip failed");
	zassert_ok(net_pkt_read(pkt, &icmp_hdr, sizeof(struct net_icmp_hdr)),
		   "param problem read failed");
	zassert_equal(icmp_hdr.type, NET_ICMPV4_BAD_IP_HEADER,
		      "param problem invalid type %u", icmp_hdr.type);
	zassert_equal(icmp_hdr.code, NET_ICMPV4_BAD_IP_HEADER_POINTER,
		      "param problem invalid code %u", icmp_hdr.code);

	zassert_ok(net_pkt_read(pkt, pointer, sizeof(pointer)),
		   "param problem read pointer failed");
	zassert_equal(pointer[0], BAD_OPT_POINTER,
		      "param problem invalid pointer %u", pointer[0]);

	zassert_equal(net_pkt_remaining_data(pkt), sizeof(buf),
		      "param problem invalid payload len %zu",
		      net_pkt_remaining_data(pkt));
	zassert_ok(net_pkt_read(pkt, buf, sizeof(buf)),
		   "param problem read payload failed");
	zassert_mem_equal(buf, icmpv4_echo_req_opt_bad, sizeof(buf),
			  "param problem invalid payload");

	param_problem_received = true;

	return 0;
}

static int tester_send(const struct device *dev, struct net_pkt *pkt)
{
	if (current == TEST_ICMPV4_ECHO_REQ) {
		return verify_echo_reply(pkt);
	} else if (current == TEST_ICMPV4_ECHO_REQ_OPTS) {
		return verify_echo_reply_with_opts(pkt);
	} else if (current == TEST_ICMPV4_PARAM_PROBLEM) {
		return verify_param_problem(pkt);
	}

	return -EINVAL;
}

struct net_icmpv4_context net_icmpv4_context_data;

static struct dummy_api net_icmpv4_if_api = {
	.iface_api.init = net_icmpv4_iface_init,
	.send = tester_send,
};

NET_DEVICE_INIT(net_icmpv4_test, "net_icmpv4_test",
		net_icmpv4_dev_init, NULL,
		&net_icmpv4_context_data, NULL,
		CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
		&net_icmpv4_if_api, DUMMY_L2,
		NET_L2_GET_CTX_TYPE(DUMMY_L2), 127);

static struct net_pkt *prepare_echo_request(struct net_if *iface)
{
	struct net_pkt *pkt;

	pkt = net_pkt_alloc_with_buffer(iface, sizeof(icmpv4_echo_req),
					NET_AF_INET, NET_IPPROTO_ICMP, K_FOREVER);
	if (!pkt) {
		return NULL;
	}

	if (net_pkt_write(pkt, icmpv4_echo_req, sizeof(icmpv4_echo_req))) {
		goto fail;
	}

	net_pkt_set_overwrite(pkt, true);
	net_pkt_cursor_init(pkt);

	return pkt;

fail:
	net_pkt_unref(pkt);

	return NULL;
}

static struct net_pkt *prepare_echo_reply(struct net_if *iface)
{
	struct net_pkt *pkt;

	pkt = net_pkt_alloc_with_buffer(iface, sizeof(icmpv4_echo_rep),
					NET_AF_INET, NET_IPPROTO_ICMP, K_FOREVER);
	if (!pkt) {
		return NULL;
	}

	if (net_pkt_write(pkt, icmpv4_echo_rep, sizeof(icmpv4_echo_rep))) {
		goto fail;
	}

	net_pkt_set_overwrite(pkt, true);
	net_pkt_cursor_init(pkt);

	return pkt;

fail:
	net_pkt_unref(pkt);

	return NULL;
}

static struct net_pkt *prepare_echo_request_with_options(struct net_if *iface)
{
	struct net_pkt *pkt;

	pkt = net_pkt_alloc_with_buffer(iface, sizeof(icmpv4_echo_req_opt),
					NET_AF_INET, NET_IPPROTO_ICMP, K_FOREVER);
	if (!pkt) {
		return NULL;
	}

	if (net_pkt_write(pkt, icmpv4_echo_req_opt,
			  sizeof(icmpv4_echo_req_opt))) {
		goto fail;
	}

	net_pkt_set_overwrite(pkt, true);
	net_pkt_cursor_init(pkt);

	return pkt;

fail:
	net_pkt_unref(pkt);

	return NULL;
}

static struct net_pkt *prepare_echo_request_with_bad_options(
							struct net_if *iface)
{
	struct net_pkt *pkt;

	pkt = net_pkt_alloc_with_buffer(iface, sizeof(icmpv4_echo_req_opt_bad),
					NET_AF_INET, NET_IPPROTO_ICMP, K_FOREVER);
	if (!pkt) {
		return NULL;
	}

	if (net_pkt_write(pkt, icmpv4_echo_req_opt_bad,
			  sizeof(icmpv4_echo_req_opt_bad))) {
		goto fail;
	}

	net_pkt_set_overwrite(pkt, true);
	net_pkt_cursor_init(pkt);

	return pkt;

fail:
	net_pkt_unref(pkt);

	return NULL;
}

static void *icmpv4_setup(void)
{
	struct net_if_addr *ifaddr;

	net_iface = net_if_get_first_by_type(&NET_L2_GET_NAME(DUMMY));
	if (!net_iface) {
		zassert_true(false, "Interface not available");
	}

	ifaddr = net_if_ipv4_addr_add(net_iface, &my_addr, NET_ADDR_MANUAL, 0);
	if (!ifaddr) {
		zassert_true(false, "Failed to add address");
	}
	return NULL;
}

static void icmpv4_teardown(void *dummy)
{
	ARG_UNUSED(dummy);

	net_iface = net_if_get_first_by_type(&NET_L2_GET_NAME(DUMMY));

	net_if_ipv4_addr_rm(net_iface, &my_addr);
}

static void icmpv4_send_echo_req(void)
{
	struct net_pkt *pkt;

	current = TEST_ICMPV4_ECHO_REQ;

	pkt = prepare_echo_request(net_iface);
	if (!pkt) {
		zassert_true(false, "EchoRequest packet prep failed");
	}

	if (net_ipv4_input(pkt)) {
		net_pkt_unref(pkt);
		zassert_true(false, "Failed to send");
	}
}

static void icmpv4_send_echo_rep(void)
{
	static struct net_icmp_ctx ctx;
	struct net_pkt *pkt;
	int ret;

	ret = net_icmp_init_ctx(&ctx, NET_AF_INET, NET_ICMPV4_ECHO_REPLY,
				0, handle_reply_msg);
	zassert_equal(ret, 0, "Cannot register %s handler (%d)",
		      STRINGIFY(NET_ICMPV4_ECHO_REPLY), ret);

	pkt = prepare_echo_reply(net_iface);
	if (!pkt) {
		zassert_true(false, "EchoReply packet prep failed");
	}

	if (net_ipv4_input(pkt)) {
		net_pkt_unref(pkt);
		zassert_true(false, "Failed to send");
	}

	ret = net_icmp_cleanup_ctx(&ctx);
	zassert_equal(ret, 0, "Cannot unregister handler (%d)", ret);
}

ZTEST(net_icmpv4, test_icmpv4_send_echo_req_opt)
{
	struct net_pkt *pkt;

	current = TEST_ICMPV4_ECHO_REQ_OPTS;

	pkt = prepare_echo_request_with_options(net_iface);
	if (!pkt) {
		zassert_true(false, "EchoRequest with opts packet prep failed");
	}

	if (net_ipv4_input(pkt)) {
		net_pkt_unref(pkt);
		zassert_true(false, "Failed to send");
	}
}

ZTEST(net_icmpv4, test_send_echo_req_bad_opt)
{
	struct net_pkt *pkt;

	current = TEST_ICMPV4_PARAM_PROBLEM;
	param_problem_received = false;

	pkt = prepare_echo_request_with_bad_options(net_iface);
	if (!pkt) {
		zassert_true(false,
			     "EchoRequest with bad opts packet prep failed");
	}

	zassert_equal(net_ipv4_input(pkt), NET_DROP,
		      "EchoRequest with bad opts was not dropped");
	net_pkt_unref(pkt);

	zassert_true(param_problem_received, "No Parameter Problem sent");
}

ZTEST(net_icmpv4, test_icmpv4_send_echo)
{
	icmpv4_send_echo_req();
	icmpv4_send_echo_rep();
}

/* A datagram that arrives with a TTL below 2 is for us all the same: RFC
 * 1122 ch. 3.2.1.7 forbids discarding it, only forwarding it is ruled out.
 */
ZTEST(net_icmpv4, test_icmpv4_echo_req_ttl_zero)
{
	uint8_t req[sizeof(icmpv4_echo_req)];
	struct net_pkt *pkt;

	memcpy(req, icmpv4_echo_req, sizeof(req));
	req[offsetof(struct net_ipv4_hdr, ttl)] = 0U;
	/* Header checksum for a TTL of 0 */
	req[offsetof(struct net_ipv4_hdr, chksum)] = 0x0c;
	req[offsetof(struct net_ipv4_hdr, chksum) + 1] = 0x19;

	current = TEST_ICMPV4_ECHO_REQ;

	pkt = net_pkt_alloc_with_buffer(net_iface, sizeof(req), NET_AF_INET,
					NET_IPPROTO_ICMP, K_FOREVER);
	zassert_not_null(pkt, "EchoRequest packet alloc failed");
	zassert_ok(net_pkt_write(pkt, req, sizeof(req)), "EchoRequest write failed");

	net_pkt_set_overwrite(pkt, true);
	net_pkt_cursor_init(pkt);

	if (net_ipv4_input(pkt) != NET_OK) {
		net_pkt_unref(pkt);
		zassert_true(false, "EchoRequest with TTL 0 was dropped");
	}
}

ZTEST_SUITE(net_icmpv4, NULL, icmpv4_setup, NULL, NULL, icmpv4_teardown);
