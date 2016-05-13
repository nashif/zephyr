/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>

#include <atomic.h>
#include <misc/util.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <bluetooth/log.h>

#include "gap_internal.h"
#include "gatt_internal.h"
#include "conn_internal.h"

#if !defined(CONFIG_NBLE_DEBUG_CONN)
#undef BT_DBG
#define BT_DBG(fmt, ...)
#endif

static struct bt_conn conns[CONFIG_BLUETOOTH_MAX_CONN];
static struct bt_conn_cb *callback_list;

static struct bt_conn *conn_new(void)
{
	struct bt_conn *conn = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(conns); i++) {
		if (!atomic_get(&conns[i].ref)) {
			conn = &conns[i];
			break;
		}
	}

	if (!conn) {
		return NULL;
	}

	memset(conn, 0, sizeof(*conn));

	atomic_set(&conn->ref, 1);

	return conn;
}

struct bt_conn *bt_conn_ref(struct bt_conn *conn)
{
	atomic_inc(&conn->ref);

	BT_DBG("handle %u ref %u", conn->handle, atomic_get(&conn->ref));

	return conn;
}

void bt_conn_unref(struct bt_conn *conn)
{
	atomic_dec(&conn->ref);

	BT_DBG("handle %u ref %u", conn->handle, atomic_get(&conn->ref));
}

struct bt_conn *bt_conn_lookup_handle(uint16_t handle)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(conns); i++) {
		if (!atomic_get(&conns[i].ref)) {
			continue;
		}

		if (conns[i].handle == handle) {
			return bt_conn_ref(&conns[i]);
		}
	}

	return NULL;
}

struct bt_conn *bt_conn_lookup_addr_le(const bt_addr_le_t *peer)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(conns); i++) {
		if (!atomic_get(&conns[i].ref)) {
			continue;
		}

		if (!bt_addr_le_cmp(peer, &conns[i].dst)) {
			return bt_conn_ref(&conns[i]);
		}
	}

	return NULL;
}

const bt_addr_le_t *bt_conn_get_dst(const struct bt_conn *conn)
{
	return &conn->dst;
}

int bt_conn_get_info(const struct bt_conn *conn, struct bt_conn_info *info)
{
	memset(info, 0, sizeof(*info));

	info->type = BT_CONN_TYPE_LE;
	info->role = conn->role;
	info->le.dst = &conn->dst;
	info->le.src = &nble.addr;
	info->le.interval = conn->interval;
	info->le.latency = conn->latency;
	info->le.timeout = conn->timeout;

	return 0;
}

int bt_conn_le_param_update(struct bt_conn *conn,
			    const struct bt_le_conn_param *param)
{
	return -ENOSYS;
}

int bt_conn_disconnect(struct bt_conn *conn, uint8_t reason)
{
	struct nble_gap_disconnect_req req;

	switch (conn->state) {
	case BT_CONN_CONNECT:
		nble_gap_cancel_connect_req(conn);
		return 0;
	case BT_CONN_CONNECTED:
		break;
	case BT_CONN_DISCONNECT:
		BT_ERR("Disconnecting already");
		return -EBUSY;
	default:
		return -ENOTCONN;
	}

	/* Handle disconnect */
	req.conn_handle = conn->handle;
	req.reason = reason;

	conn->state = BT_CONN_DISCONNECT;

	nble_gap_disconnect_req(&req);

	return 0;
}

void on_nble_gap_disconnect_rsp(const struct nble_common_rsp *rsp)
{
	if (rsp->status) {
		BT_ERR("Disconnect failed, status %d", rsp->status);
		return;
	}

	BT_DBG("conn %p", rsp->user_data);
}

void on_nble_gap_cancel_connect_rsp(const struct nble_common_rsp *rsp)
{
	if (rsp->status) {
		BT_ERR("Cancel connect failed, status %d", rsp->status);
		return;
	}

	BT_DBG("conn %p", rsp->user_data);
}

static inline bool bt_le_conn_params_valid(uint16_t min, uint16_t max,
					   uint16_t latency, uint16_t timeout)
{
	if (min > max || min < 6 || max > 3200) {
		return false;
	}

	/* Limits according to BT Core spec 4.2 [Vol 2, Part E, 7.8.12] */
	if (timeout < 10 || timeout > 3200 ||
	    (2 * timeout) < ((1 + latency) * max * 5)) {
		return false;
	}

	/* Limits according to BT Core spec 4.2 [Vol 6, Part B, 4.5.1] */
	if (latency > 499 || ((latency + 1) * max) > (timeout * 4)) {
		return false;
	}

	return true;
}

struct bt_conn *bt_conn_create_le(const bt_addr_le_t *peer,
				  const struct bt_le_conn_param *param)
{
	struct nble_gap_connect_req req;
	struct bt_conn *conn;

	if (!bt_le_conn_params_valid(param->interval_min, param->interval_max,
				     param->latency, param->timeout)) {
		return NULL;
	}

	conn = bt_conn_lookup_addr_le(peer);
	if (conn) {
		return conn;
	}

	conn = conn_new();
	if (!conn) {
		BT_ERR("Unable to create new bt_conn object");
		return NULL;
	}

	/* Update connection parameters */
	bt_addr_le_copy(&conn->dst, peer);
	conn->latency = param->latency;
	conn->timeout = param->timeout;

	/* Construct parameters to NBLE */
	bt_addr_le_copy(&req.bda, peer);

	req.conn_params.interval_min = param->interval_min;
	req.conn_params.interval_max = param->interval_max;
	req.conn_params.slave_latency = param->latency;
	req.conn_params.link_sup_to = param->timeout;

	req.scan_params.interval = BT_GAP_SCAN_FAST_INTERVAL;
	req.scan_params.window = BT_GAP_SCAN_FAST_WINDOW;
	/* Use passive scanning */
	req.scan_params.active = 0;
	/* Do not use whitelist */
	req.scan_params.selective = 0;
	/* Disable timeout */
	req.scan_params.timeout = 0;

	conn->state = BT_CONN_CONNECT;

	nble_gap_connect_req(&req, conn);

	return conn;
}

void on_nble_gap_connect_rsp(const struct nble_common_rsp *rsp)
{
	if (rsp->status) {
		BT_ERR("Connect failed, status %d", rsp->status);
		return;
	}

	BT_DBG("conn %p", rsp->user_data);
}

int bt_conn_security(struct bt_conn *conn, bt_security_t sec)
{
	struct nble_sm_security_req params = {
			.conn = conn,
			.conn_handle = conn->handle,
	};

	BT_DBG("conn %p sec %u", conn, sec);

	switch (sec) {
	case BT_SECURITY_LOW:
		params.params.auth_level = BT_SMP_AUTH_NONE;
		break;
	case BT_SECURITY_MEDIUM:
		params.params.auth_level = BT_SMP_AUTH_BONDING;
		break;
	case BT_SECURITY_HIGH:
		params.params.auth_level = BT_SMP_AUTH_BONDING |
						BT_SMP_AUTH_MITM;
		break;
	case BT_SECURITY_FIPS:
		params.params.auth_level = BT_SMP_AUTH_BONDING | BT_SMP_AUTH_SC;
	}

	nble_sm_security_req(&params);

	return 0;
}

uint8_t bt_conn_enc_key_size(struct bt_conn *conn)
{
	return 0;
}

void bt_conn_cb_register(struct bt_conn_cb *cb)
{
	cb->_next = callback_list;
	callback_list = cb;
}

int bt_le_set_auto_conn(bt_addr_le_t *addr,
			const struct bt_le_conn_param *param)
{
	return -ENOSYS;
}

struct bt_conn *bt_conn_create_slave_le(const bt_addr_le_t *peer,
					const struct bt_le_adv_param *param)
{
	return NULL;
}

int bt_conn_auth_cb_register(const struct bt_conn_auth_cb *cb)
{
	if (!cb) {
		nble.auth = NULL;
		return 0;
	}

	/* cancel callback should always be provided */
	if (!cb->cancel) {
		return -EINVAL;
	}

	if (nble.auth) {
		return -EALREADY;
	}

	nble.auth = cb;

	return 0;
}

int bt_conn_auth_passkey_entry(struct bt_conn *conn, unsigned int passkey)
{
	return -ENOSYS;
}

int bt_conn_auth_cancel(struct bt_conn *conn)
{
	return -ENOSYS;
}

int bt_conn_auth_passkey_confirm(struct bt_conn *conn)
{
	return -ENOSYS;
}

int bt_conn_auth_pairing_confirm(struct bt_conn *conn)
{
	return -ENOSYS;
}

/* Connection related events */

static void notify_connected(struct bt_conn *conn)
{
	struct bt_conn_cb *cb;

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->connected) {
			cb->connected(conn, 0);
		}
	}
}

static void notify_disconnected(struct bt_conn *conn)
{
	struct bt_conn_cb *cb;

	bt_gatt_disconnected(conn);

	/*
	 * FIXME: When disconnected NBLE stops advertising, this should
	 * be fixed in the NBLE firmware, use this hack for now
	 */
	if (nble.keep_adv) {
		BT_WARN("Re-enable advertising on disconnect");
		nble_gap_start_adv_req();
	}

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->disconnected) {
			cb->disconnected(conn, 0);
		}
	}
}

void on_nble_gap_connect_evt(const struct nble_gap_connect_evt *ev)
{
	struct bt_conn *conn;

	BT_DBG("handle %u role %u", ev->conn_handle, ev->role_slave);

	conn = conn_new();
	if (!conn) {
		BT_ERR("Unable to create new bt_conn object");
		return;
	}

	conn->handle = ev->conn_handle;
	conn->role = ev->role_slave;
	conn->interval = ev->conn_values.interval;
	conn->latency = ev->conn_values.latency;
	conn->timeout = ev->conn_values.supervision_to;
	bt_addr_le_copy(&conn->dst, &ev->peer_bda);

	conn->state = BT_CONN_CONNECTED;

	notify_connected(conn);
}

void on_nble_gap_disconnect_evt(const struct nble_gap_disconnect_evt *ev)
{
	struct bt_conn *conn;

	conn = bt_conn_lookup_handle(ev->conn_handle);
	if (!conn) {
		BT_ERR("Unable to find conn for handle %u", ev->conn_handle);
		return;
	}

	BT_DBG("conn %p handle %u", conn, ev->conn_handle);

	conn->state = BT_CONN_DISCONNECTED;

	notify_disconnected(conn);

	/* Drop the reference given by lookup_handle() */
	bt_conn_unref(conn);
	/* Drop the initial reference from conn_new() */
	bt_conn_unref(conn);
}

void on_nble_gap_conn_update_evt(const struct nble_gap_conn_update_evt *ev)
{
	struct bt_conn *conn;

	conn = bt_conn_lookup_handle(ev->conn_handle);
	if (!conn) {
		BT_ERR("Unable to find conn for handle %u", ev->conn_handle);
		return;
	}

	BT_DBG("conn %p handle %u interval %u latency %u to %u",
	       conn, ev->conn_handle, ev->conn_values.interval,
	       ev->conn_values.latency, ev->conn_values.supervision_to);

	bt_conn_unref(conn);
}
