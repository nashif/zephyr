/*
 * Copyright (c) 2019 Laird Connectivity
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(net_test, CONFIG_DNS_RESOLVER_LOG_LEVEL);

#include <zephyr/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <sys/printk.h>

#include <ztest.h>

#include <net/ethernet.h>
#include <net/dummy.h>
#include <net/buf.h>
#include <net/net_ip.h>
#include <net/net_if.h>
#include <net/dns_resolve.h>
#include <net/net_event.h>
#include <net/net_mgmt.h>

#define NET_LOG_ENABLED 1
#include "net_private.h"

#if defined(CONFIG_DNS_RESOLVER_LOG_LEVEL_DBG)
#define DBG(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
#define DBG(fmt, ...)
#endif

#define NAME4 "4.zephyr.test"
#define NAME6 "6.zephyr.test"
#define NAME_IPV4 "192.0.2.1"
#define NAME_IPV6 "2001:db8::1"

#define DNS_NAME_IPV4 "192.0.2.4"
#define DNS_NAME_IPV6 "2001:db8::4"

#define DNS_TIMEOUT 500 /* ms */

#if defined(CONFIG_NET_IPV6)
/* Interface 1 addresses */
static struct in6_addr my_addr1 = { { { 0x20, 0x01, 0x0d, 0xb8, 1, 0, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0x1 } } };
#endif

#if defined(CONFIG_NET_IPV4)
/* Interface 1 addresses */
static struct in_addr my_addr2 = { { { 192, 0, 2, 1 } } };
#endif

static struct net_mgmt_event_callback mgmt_cb;
static struct k_sem dns_added;
static struct k_sem dns_removed;

static struct net_if *iface1;

#if defined(CONFIG_NET_IPV4)
static struct dns_resolve_context resv_ipv4;
static struct dns_resolve_context resv_ipv4_2;
#endif
#if defined(CONFIG_NET_IPV6)
static struct dns_resolve_context resv_ipv6;
static struct dns_resolve_context resv_ipv6_2;
#endif

/* this must be higher that the DNS_TIMEOUT */
#define WAIT_TIME K_MSEC((DNS_TIMEOUT + 300) * 3)

struct net_if_test {
	u8_t idx;
	u8_t mac_addr[sizeof(struct net_eth_addr)];
};

static int net_iface_dev_init(struct device *dev)
{
	return 0;
}

static u8_t *net_iface_get_mac(struct device *dev)
{
	struct net_if_test *data = dev->driver_data;

	if (data->mac_addr[2] == 0x00) {
		/* 00-00-5E-00-53-xx Documentation RFC 7042 */
		data->mac_addr[0] = 0x00;
		data->mac_addr[1] = 0x00;
		data->mac_addr[2] = 0x5E;
		data->mac_addr[3] = 0x00;
		data->mac_addr[4] = 0x53;
		data->mac_addr[5] = sys_rand32_get();
	}

	return data->mac_addr;
}

static void net_iface_init(struct net_if *iface)
{
	u8_t *mac = net_iface_get_mac(net_if_get_device(iface));

	net_if_set_link_addr(iface, mac, sizeof(struct net_eth_addr),
			     NET_LINK_ETHERNET);
}

static int sender_iface(struct device *dev, struct net_pkt *pkt)
{
	if (!pkt->frags) {
		DBG("No data to send!\n");
		return -ENODATA;
	}

	return 0;
}

struct net_if_test net_iface1_data;

static struct dummy_api net_iface_api = {
	.iface_api.init = net_iface_init,
	.send = sender_iface,
};

#define _ETH_L2_LAYER DUMMY_L2
#define _ETH_L2_CTX_TYPE NET_L2_GET_CTX_TYPE(DUMMY_L2)

NET_DEVICE_INIT_INSTANCE(net_iface1_test,
			 "iface1",
			 iface1,
			 net_iface_dev_init,
			 device_pm_control_nop,
			 &net_iface1_data,
			 NULL,
			 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
			 &net_iface_api,
			 _ETH_L2_LAYER,
			 _ETH_L2_CTX_TYPE,
			 127);

static void test_dns_evt_handler(struct net_mgmt_event_callback *cb,
			      u32_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_DNS_SERVER_ADD) {
		k_sem_give(&dns_added);
	} else if (mgmt_event == NET_EVENT_DNS_SERVER_DEL) {
		k_sem_give(&dns_removed);
	}
}

static void test_init(void)
{
	struct net_if_addr *ifaddr;

	iface1 = net_if_get_by_index(0);
	zassert_is_null(iface1, "iface1");

	iface1 = net_if_get_by_index(1);

	((struct net_if_test *)net_if_get_device(iface1)->driver_data)->idx =
		net_if_get_by_iface(iface1);

#if defined(CONFIG_NET_IPV6)
	ifaddr = net_if_ipv6_addr_add(iface1, &my_addr1,
				      NET_ADDR_MANUAL, 0);
	if (!ifaddr) {
		DBG("Cannot add IPv6 address %s\n",
		       net_sprint_ipv6_addr(&my_addr1));
		zassert_not_null(ifaddr, "addr1");

		return;
	}

	/* For testing purposes we need to set the adddresses preferred */
	ifaddr->addr_state = NET_ADDR_PREFERRED;
#endif

#if defined(CONFIG_NET_IPV4)
	ifaddr = net_if_ipv4_addr_add(iface1, &my_addr2,
				      NET_ADDR_MANUAL, 0);
	if (!ifaddr) {
		DBG("Cannot add IPv4 address %s\n",
		       net_sprint_ipv4_addr(&my_addr2));
		zassert_not_null(ifaddr, "addr2");

		return;
	}

	ifaddr->addr_state = NET_ADDR_PREFERRED;
#endif

	net_if_up(iface1);

	k_sem_init(&dns_added, 0, 1);
	k_sem_init(&dns_removed, 0, 1);

	net_mgmt_init_event_callback(&mgmt_cb, dns_evt_handler,
				     NET_EVENT_DNS_SERVER_ADD |
				     NET_EVENT_DNS_SERVER_DEL);
	net_mgmt_add_event_callback(&mgmt_cb);
}

static void test_dns_do_not_add_add_callback6(void)
{
#if defined(CONFIG_NET_IPV6)
	/* Wait for DNS added callback without adding DNS */

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_added, WAIT_TIME)) {
		zassert_true(true,
			"Received DNS added callback when should not have");
	}
#endif
}

/* Wait for DNS added callback after adding DNS */
static void test_dns_add_callback6(void)
{
#if defined(CONFIG_NET_IPV6)

	struct dns_resolve_context *dnsCtx = &resv_ipv6;
	const char *dns_servers_str[] = { DNS_NAME_IPV6, NULL };
	int ret;

	dns_resolve_close(dnsCtx);

	ret = dns_resolve_init(dnsCtx, dns_servers_str, NULL);
	if (ret < 0) {
		LOG_ERR("dns_resolve_init fail (%d)", ret);
		return;
	}

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_added, WAIT_TIME)) {
		zassert_true(false,
			     "Timeout while waiting for DNS added callback");
	}
#endif
}

static void test_dns_remove_callback6(void)
{
#if defined(CONFIG_NET_IPV6)
	/* Wait for DNS removed callback after removing DNS */

	int ret;

	ret = dns_resolve_close(&resv_ipv6);

	zassert_equal(ret, 0, "Cannot remove DNS server");

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_removed, WAIT_TIME)) {
		zassert_true(false,
			     "Timeout while waiting for DNS removed callback");
	}
#endif
}

static void test_dns_remove_none_callback6(void)
{
#if defined(CONFIG_NET_IPV6)
	/* Wait for DNS removed callback without removing DNS */
	int ret;

	ret = dns_resolve_close(&resv_ipv6);

	zassert_not_equal(ret, 0, "Cannot remove DNS server");

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_removed, WAIT_TIME)) {
		zassert_true(true,
			"Received DNS removed callback when should not have");
	}
#endif
}

static void test_dns_add_remove_two_callback6(void)
{
#if defined(CONFIG_NET_IPV6)
	struct dns_resolve_context *dnsCtx = &resv_ipv6;
	const char *dns_servers_str[] = { DNS_NAME_IPV6, NULL };
	int ret;

	dns_resolve_close(dnsCtx);

	ret = dns_resolve_init(dnsCtx, dns_servers_str, NULL);
	if (ret < 0) {
		LOG_ERR("dns_resolve_init fail (%d)", ret);
		return;
	}

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_added, WAIT_TIME)) {
		zassert_true(false,
			     "Timeout while waiting for DNS added callback");
	}

	/* Add second DNS entry */
	dnsCtx = &resv_ipv6_2;
	dns_resolve_close(dnsCtx);

	ret = dns_resolve_init(dnsCtx, dns_servers_str, NULL);
	if (ret < 0) {
		LOG_ERR("dns_resolve_init fail (%d)", ret);
		return;
	}

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_added, WAIT_TIME)) {
		zassert_true(false,
			     "Timeout while waiting for DNS added callback");
	}

	/* Check both DNS servers are used */
	zassert_true(resv_ipv6.is_used, "DNS server #1 is missing");
	zassert_true(resv_ipv6_2.is_used, "DNS server #2 is missing");

	/* Remove first DNS server */
	dnsCtx = &resv_ipv6;
	ret = dns_resolve_close(dnsCtx);
	zassert_equal(ret, 0, "Cannot remove DNS server #1");

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_removed, WAIT_TIME)) {
		zassert_true(true,
			"Received DNS removed callback when should not have");
	}

	/* Check second DNS servers is used */
	zassert_false(resv_ipv6.is_used, "DNS server #1 is active");
	zassert_true(resv_ipv6_2.is_used, "DNS server #2 is missing");

	/* Check first DNS server cannot be removed once removed */
	ret = dns_resolve_close(dnsCtx);
	zassert_not_equal(ret, 0,
			  "Successful result code when attempting to "
			  "remove DNS server #1 again");

	/* Remove second DNS server */
	dnsCtx = &resv_ipv6_2;
	ret = dns_resolve_close(dnsCtx);
	zassert_equal(ret, 0, "Cannot remove DNS server #2");

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_removed, WAIT_TIME)) {
		zassert_true(true,
			     "Received DNS removed callback when should "
			     "not have");
	}

	/* Check neither DNS server is used */
	zassert_false(resv_ipv6.is_used, "DNS server #1 isa ctive");
	zassert_false(resv_ipv6_2.is_used, "DNS server #2 is active");

	/* Check first DNS server cannot be removed once removed */
	ret = dns_resolve_close(dnsCtx);
	zassert_not_equal(ret, 0,
			  "Successful result code when attempting "
			  "to remove DNS server #1 again");
#endif
}

static void test_dns_do_not_add_add_callback(void)
{
#if defined(CONFIG_NET_IPV4)
	/* Wait for DNS added callback without adding DNS */

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_added, WAIT_TIME)) {
		zassert_true(true,
			"Received DNS added callback when should not have");
	}
#endif
}

static void test_dns_add_callback(void)
{
#if defined(CONFIG_NET_IPV4)
	/* Wait for DNS added callback after adding DNS */
	struct dns_resolve_context *dnsCtx = &resv_ipv4;
	const char *dns_servers_str[] = { DNS_NAME_IPV4, NULL };
	int ret;

	dns_resolve_close(dnsCtx);

	ret = dns_resolve_init(dnsCtx, dns_servers_str, NULL);
	if (ret < 0) {
		LOG_ERR("dns_resolve_init fail (%d)", ret);
		return;
	}

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_added, WAIT_TIME)) {
		zassert_true(false,
			     "Timeout while waiting for DNS added callback");
	}
#endif
}

static void test_dns_remove_callback(void)
{
#if defined(CONFIG_NET_IPV4)
	/* Wait for DNS removed callback after removing DNS */
	int ret;

	ret = dns_resolve_close(&resv_ipv4);

	zassert_equal(ret, 0, "Cannot remove DNS server");

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_removed, WAIT_TIME)) {
		zassert_true(false,
			     "Timeout while waiting for DNS removed callback");
	}
#endif
}

static void test_dns_remove_none_callback(void)
{
#if defined(CONFIG_NET_IPV4)
	/* Wait for DNS removed callback without removing DNS */
	int ret;

	ret = dns_resolve_close(&resv_ipv4);

	zassert_not_equal(ret, 0, "Cannot remove DNS server");

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_removed, WAIT_TIME)) {
		zassert_true(true,
			"Received DNS removed callback when should not have");
	}
#endif
}

static void test_dns_add_remove_two_callback(void)
{
#if defined(CONFIG_NET_IPV4)
	struct dns_resolve_context *dnsCtx = &resv_ipv4;
	const char *dns_servers_str[] = { DNS_NAME_IPV4, NULL };
	int ret;

	dns_resolve_close(dnsCtx);

	ret = dns_resolve_init(dnsCtx, dns_servers_str, NULL);
	if (ret < 0) {
		LOG_ERR("dns_resolve_init fail (%d)", ret);
		return;
	}

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_added, WAIT_TIME)) {
		zassert_true(false,
			     "Timeout while waiting for DNS added callback");
	}

	/* Add second DNS entry */
	dnsCtx = &resv_ipv4_2;
	dns_resolve_close(dnsCtx);

	ret = dns_resolve_init(dnsCtx, dns_servers_str, NULL);
	if (ret < 0) {
		LOG_ERR("dns_resolve_init fail (%d)", ret);
		return;
	}

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_added, WAIT_TIME)) {
		zassert_true(false,
			     "Timeout while waiting for DNS added callback");
	}

	/* Check both DNS servers are used */
	zassert_true(resv_ipv4.is_used, "DNS server #1 is missing");
	zassert_true(resv_ipv4_2.is_used, "DNS server #2 is missing");

	/* Remove first DNS server */
	dnsCtx = &resv_ipv4;
	ret = dns_resolve_close(dnsCtx);
	zassert_equal(ret, 0, "Cannot remove DNS server #1");

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_removed, WAIT_TIME)) {
		zassert_true(true,
			"Received DNS removed callback when should not have");
	}

	/* Check second DNS servers is used */
	zassert_false(resv_ipv4.is_used, "DNS server #1 is active");
	zassert_true(resv_ipv4_2.is_used, "DNS server #2 is missing");

	/* Check first DNS server cannot be removed once removed */
	ret = dns_resolve_close(dnsCtx);
	zassert_not_equal(ret, 0,
			  "Successful result code when attempting to "
			  "remove DNS server #1 again");

	/* Remove second DNS server */
	dnsCtx = &resv_ipv4_2;
	ret = dns_resolve_close(dnsCtx);
	zassert_equal(ret, 0, "Cannot remove DNS server #2");

	k_yield(); /* mandatory so that net_if send func gets to run */

	if (k_sem_take(&dns_removed, WAIT_TIME)) {
		zassert_true(true,
			     "Received DNS removed callback when should "
			     "not have");
	}

	/* Check neither DNS server is used */
	zassert_false(resv_ipv4.is_used, "DNS server #1 isa ctive");
	zassert_false(resv_ipv4_2.is_used, "DNS server #2 is active");

	/* Check first DNS server cannot be removed once removed */
	ret = dns_resolve_close(dnsCtx);
	zassert_not_equal(ret, 0,
			  "Successful result code when attempting to "
			  "remove DNS server #1 again");
#endif
}

void test_main(void)
{
	ztest_test_suite(dns_tests,
			 ztest_unit_test(test_init),
			 ztest_unit_test(test_dns_do_not_add_add_callback6),
			 ztest_unit_test(test_dns_add_callback6),
			 ztest_unit_test(test_dns_remove_callback6),
			 ztest_unit_test(test_dns_remove_none_callback6),
			 ztest_unit_test(test_dns_add_remove_two_callback6),
			 ztest_unit_test(test_dns_do_not_add_add_callback),
			 ztest_unit_test(test_dns_add_callback),
			 ztest_unit_test(test_dns_remove_callback),
			 ztest_unit_test(test_dns_remove_none_callback),
			 ztest_unit_test(test_dns_add_remove_two_callback)

);

	ztest_run_test_suite(dns_tests);
}
