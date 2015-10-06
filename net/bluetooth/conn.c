/* conn.c - Bluetooth connection handling */

/*
 * Copyright (c) 2015 Intel Corporation
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

#include <nanokernel.h>
#include <arch/cpu.h>
#include <toolchain.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <atomic.h>
#include <misc/byteorder.h>
#include <misc/util.h>

#include <bluetooth/log.h>
#include <bluetooth/hci.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/driver.h>

#include "hci_core.h"
#include "conn_internal.h"
#include "l2cap.h"
#include "keys.h"
#include "smp.h"

#if !defined(CONFIG_BLUETOOTH_DEBUG_CONN)
#undef BT_DBG
#define BT_DBG(fmt, ...)
#endif

/* How long until we cancel HCI_LE_Create_Connection */
#define CONN_TIMEOUT	(3 * sys_clock_ticks_per_sec)

static struct bt_conn conns[CONFIG_BLUETOOTH_MAX_CONN];
static struct bt_conn_cb *callback_list;

#if defined(CONFIG_BLUETOOTH_DEBUG_CONN)
static const char *state2str(bt_conn_state_t state)
{
	switch (state) {
	case BT_CONN_DISCONNECTED:
		return "disconnected";
	case BT_CONN_CONNECT_SCAN:
		return "connect-scan";
	case BT_CONN_CONNECT:
		return "connect";
	case BT_CONN_CONNECTED:
		return "connected";
	case BT_CONN_DISCONNECT:
		return "disconnect";
	default:
		return "(unknown)";
	}
}
#endif

static void notify_connected(struct bt_conn *conn)
{
	struct bt_conn_cb *cb;

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->connected) {
			cb->connected(conn);
		}
	}
}

static void notify_disconnected(struct bt_conn *conn)
{
	struct bt_conn_cb *cb;

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->disconnected) {
			cb->disconnected(conn);
		}
	}
}

#if defined(CONFIG_BLUETOOTH_SMP)
void bt_conn_identity_resolved(struct bt_conn *conn)
{
	const bt_addr_le_t *rpa;
	struct bt_conn_cb *cb;

	if (conn->role == BT_HCI_ROLE_MASTER) {
		rpa = &conn->resp_addr;
	} else {
		rpa = &conn->init_addr;
	}

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->identity_resolved) {
			cb->identity_resolved(conn, rpa, &conn->dst);
		}
	}
}

void bt_conn_security_changed(struct bt_conn *conn)
{
	struct bt_conn_cb *cb;

	for (cb = callback_list; cb; cb = cb->_next) {
		if (cb->security_changed) {
			cb->security_changed(conn, conn->sec_level);
		}
	}
}

int bt_conn_le_start_encryption(struct bt_conn *conn, uint64_t rand,
				uint16_t ediv, const uint8_t *ltk)
{
	struct bt_hci_cp_le_start_encryption *cp;
	struct bt_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_START_ENCRYPTION, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = bt_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);
	cp->rand = rand;
	cp->ediv = ediv;
	memcpy(cp->ltk, ltk, sizeof(cp->ltk));

	return bt_hci_cmd_send_sync(BT_HCI_OP_LE_START_ENCRYPTION, buf, NULL);
}

int bt_conn_security(struct bt_conn *conn, bt_security_t sec)
{
	int err = 0;

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	/* nothing to do */
	if (conn->sec_level >= sec || conn->required_sec_level >= sec) {
		return 0;
	}

	/* for now we only support legacy pairing */
	if (sec > BT_SECURITY_HIGH) {
		return -EINVAL;
	}

	conn->required_sec_level = sec;

#if defined(CONFIG_BLUETOOTH_CENTRAL)
	if (conn->role == BT_HCI_ROLE_MASTER) {
		struct bt_keys *keys;

		keys = bt_keys_find(BT_KEYS_LTK, &conn->dst);
		if (keys) {
			if (sec > BT_SECURITY_MEDIUM &&
			    keys->type != BT_KEYS_AUTHENTICATED) {
				err = bt_smp_send_pairing_req(conn);
				goto done;
			}

			err = bt_conn_le_start_encryption(conn, keys->ltk.rand,
							  keys->ltk.ediv,
							  keys->ltk.val);
			goto done;
		}

		err = bt_smp_send_pairing_req(conn);
		goto done;
	}
#endif /* CONFIG_BLUETOOTH_CENTRAL */

#if defined(CONFIG_BLUETOOTH_PERIPHERAL)
	err = bt_smp_send_security_req(conn);
#endif /* CONFIG_BLUETOOTH_PERIPHERAL */

done:
	/* reset required security level in case of error */
	if (err) {
		conn->required_sec_level = conn->sec_level;
	}

	return err;
}
#endif /* CONFIG_BLUETOOTH_SMP */

void bt_conn_cb_register(struct bt_conn_cb *cb)
{
	cb->_next = callback_list;
	callback_list = cb;
}

static void bt_conn_reset_rx_state(struct bt_conn *conn)
{
	if (!conn->rx_len) {
		return;
	}

	bt_buf_put(conn->rx);
	conn->rx = NULL;
	conn->rx_len = 0;
}

void bt_conn_recv(struct bt_conn *conn, struct bt_buf *buf, uint8_t flags)
{
	struct bt_l2cap_hdr *hdr;
	uint16_t len;

	BT_DBG("handle %u len %u flags %02x\n", conn->handle, buf->len, flags);

	/* Check packet boundary flags */
	switch (flags) {
	case 0x02:
		/* First packet */
		hdr = (void *)buf->data;
		len = sys_le16_to_cpu(hdr->len);

		BT_DBG("First, len %u final %u\n", buf->len, len);

		if (conn->rx_len) {
			BT_ERR("Unexpected first L2CAP frame\n");
			bt_conn_reset_rx_state(conn);
		}

		conn->rx_len = (sizeof(*hdr) + len) - buf->len;
		BT_DBG("rx_len %u\n", conn->rx_len);
		if (conn->rx_len) {
			conn->rx = buf;
			return;
		}

		break;
	case 0x01:
		/* Continuation */
		if (!conn->rx_len) {
			BT_ERR("Unexpected L2CAP continuation\n");
			bt_conn_reset_rx_state(conn);
			bt_buf_put(buf);
			return;
		}

		if (buf->len > conn->rx_len) {
			BT_ERR("L2CAP data overflow\n");
			bt_conn_reset_rx_state(conn);
			bt_buf_put(buf);
			return;
		}

		BT_DBG("Cont, len %u rx_len %u\n", buf->len, conn->rx_len);

		if (buf->len > bt_buf_tailroom(conn->rx)) {
			BT_ERR("Not enough buffer space for L2CAP data\n");
			bt_conn_reset_rx_state(conn);
			bt_buf_put(buf);
			return;
		}

		memcpy(bt_buf_add(conn->rx, buf->len), buf->data, buf->len);
		conn->rx_len -= buf->len;
		bt_buf_put(buf);

		if (conn->rx_len) {
			return;
		}

		buf = conn->rx;
		conn->rx = NULL;
		conn->rx_len = 0;

		break;
	default:
		BT_ERR("Unexpected ACL flags (0x%02x)\n", flags);
		bt_conn_reset_rx_state(conn);
		bt_buf_put(buf);
		return;
	}

	hdr = (void *)buf->data;
	len = sys_le16_to_cpu(hdr->len);

	if (sizeof(*hdr) + len != buf->len) {
		BT_ERR("ACL len mismatch (%u != %u)\n", len, buf->len);
		bt_buf_put(buf);
		return;
	}

	BT_DBG("Successfully parsed %u byte L2CAP packet\n", buf->len);

	bt_l2cap_recv(conn, buf);
}

void bt_conn_send(struct bt_conn *conn, struct bt_buf *buf)
{
	uint16_t len, remaining = buf->len;
	struct bt_hci_acl_hdr *hdr;
	struct nano_fifo frags;
	uint8_t *ptr;

	BT_DBG("conn handle %u buf len %u\n", conn->handle, buf->len);

	if (conn->state != BT_CONN_CONNECTED) {
		BT_ERR("not connected!\n");
		return;
	}

	nano_fifo_init(&frags);

	len = min(remaining, bt_dev.le_mtu);

	hdr = bt_buf_push(buf, sizeof(*hdr));
	hdr->handle = sys_cpu_to_le16(conn->handle);
	hdr->len = sys_cpu_to_le16(len);

	buf->len -= remaining - len;
	ptr = bt_buf_tail(buf);

	nano_fifo_put(&frags, buf);
	remaining -= len;

	while (remaining) {
		buf = bt_l2cap_create_pdu(conn);

		len = min(remaining, bt_dev.le_mtu);

		/* Copy from original buffer */
		memcpy(bt_buf_add(buf, len), ptr, len);
		ptr += len;

		hdr = bt_buf_push(buf, sizeof(*hdr));
		hdr->handle = sys_cpu_to_le16(conn->handle | (1 << 12));
		hdr->len = sys_cpu_to_le16(len);

		nano_fifo_put(&frags, buf);
		remaining -= len;
	}

	while ((buf = nano_fifo_get(&frags))) {
		nano_fifo_put(&conn->tx_queue, buf);
	}
}

static void conn_tx_fiber(int arg1, int arg2)
{
	struct bt_conn *conn = (struct bt_conn *)arg1;
	struct bt_buf *buf;

	BT_DBG("Started for handle %u\n", conn->handle);

	while (conn->state == BT_CONN_CONNECTED) {
		int err;

		/* Wait until the controller can accept ACL packets */
		BT_DBG("calling sem_take_wait\n");
		nano_fiber_sem_take_wait(&bt_dev.le_pkts_sem);

		/* check for disconnection */
		if (conn->state != BT_CONN_CONNECTED) {
			nano_fiber_sem_give(&bt_dev.le_pkts_sem);
			break;
		}

		/* Get next ACL packet for connection */
		buf = nano_fifo_get_wait(&conn->tx_queue);
		if (conn->state != BT_CONN_CONNECTED) {
			nano_fiber_sem_give(&bt_dev.le_pkts_sem);
			bt_buf_put(buf);
			break;
		}

		BT_DBG("passing buf %p len %u to driver\n", buf, buf->len);
		err = bt_dev.drv->send(buf);
		if (err) {
			BT_ERR("Unable to send to driver (err %d)\n", err);
		} else {
			conn->pending_pkts++;
		}

		bt_buf_put(buf);
	}

	BT_DBG("handle %u disconnected - cleaning up\n", conn->handle);

	/* Give back any allocated buffers */
	while ((buf = nano_fifo_get(&conn->tx_queue))) {
		bt_buf_put(buf);
	}

	/* Return any unacknowledged packets */
	if (conn->pending_pkts) {
		while (conn->pending_pkts--) {
			nano_fiber_sem_give(&bt_dev.le_pkts_sem);
		}
	}

	bt_conn_reset_rx_state(conn);

	BT_DBG("handle %u exiting\n", conn->handle);
	bt_conn_put(conn);
}

struct bt_conn *bt_conn_add(const bt_addr_le_t *peer)
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
	bt_addr_le_copy(&conn->dst, peer);
#if defined(CONFIG_BLUETOOTH_SMP)
	conn->sec_level = BT_SECURITY_LOW;
	conn->required_sec_level = BT_SECURITY_LOW;
#endif /* CONFIG_BLUETOOTH_SMP */

	return conn;
}

static void timeout_fiber(int arg1, int arg2)
{
	struct bt_conn *conn = (struct bt_conn *)arg1;
	ARG_UNUSED(arg2);

	conn->timeout = NULL;

	bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	bt_conn_put(conn);
}

void bt_conn_set_state(struct bt_conn *conn, bt_conn_state_t state)
{
	bt_conn_state_t old_state;

	BT_DBG("%s -> %s\n", state2str(conn->state), state2str(state));

	if (conn->state == state) {
		BT_WARN("no transition\n");
		return;
	}

	old_state = conn->state;
	conn->state = state;

	/* Actions needed for exiting the old state */
	switch (old_state) {
	case BT_CONN_DISCONNECTED:
		/* Take a reference for the first state transition after
		 * bt_conn_add() and keep it until reaching DISCONNECTED
		 * again.
		 */
		bt_conn_get(conn);
		break;
	case BT_CONN_CONNECT:
		if (conn->timeout) {
			fiber_fiber_delayed_start_cancel(conn->timeout);
			conn->timeout = NULL;

			/* Drop the reference taken by timeout fiber */
			bt_conn_put(conn);
		}
		break;
	default:
		break;
	}

	/* Actions needed for entering the new state */
	switch (conn->state){
	case BT_CONN_CONNECTED:
		nano_fifo_init(&conn->tx_queue);
		fiber_start(conn->stack, sizeof(conn->stack), conn_tx_fiber,
			    (int)bt_conn_get(conn), 0, 7, 0);

		bt_l2cap_connected(conn);
		notify_connected(conn);
		break;
	case BT_CONN_DISCONNECTED:
		/* Send dummy buffer to wake up and stop the tx fiber
		 * for states where it was running
		 */
		if (old_state == BT_CONN_CONNECTED ||
		    old_state == BT_CONN_DISCONNECT) {
			bt_l2cap_disconnected(conn);
			notify_disconnected(conn);

			nano_fifo_put(&conn->tx_queue, bt_buf_get(BT_DUMMY, 0));
		}

		/* Release the reference we took for the very first
		 * state transition.
		 */
		bt_conn_put(conn);

		break;
	case BT_CONN_CONNECT_SCAN:
		break;
	case BT_CONN_CONNECT:
		/* Add LE Create Connection timeout */
		conn->timeout = fiber_delayed_start(conn->stack,
						    sizeof(conn->stack),
						    timeout_fiber,
						    (int)bt_conn_get(conn),
						    0, 7, 0, CONN_TIMEOUT);
		break;
	case BT_CONN_DISCONNECT:
		break;
	default:
		BT_WARN("no valid (%u) state was set\n", state);

		break;
	}
}

struct bt_conn *bt_conn_lookup_handle(uint16_t handle)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(conns); i++) {
		if (!atomic_get(&conns[i].ref)) {
			continue;
		}

		/* We only care about connections with a valid handle */
		if (conns[i].state != BT_CONN_CONNECTED &&
		    conns[i].state != BT_CONN_DISCONNECT) {
			continue;
		}

		if (conns[i].handle == handle) {
			return bt_conn_get(&conns[i]);
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
			return bt_conn_get(&conns[i]);
		}
	}

	return NULL;
}

struct bt_conn *bt_conn_lookup_state(const bt_addr_le_t *peer,
				     const bt_conn_state_t state)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(conns); i++) {
		if (!atomic_get(&conns[i].ref)) {
			continue;
		}

		if (bt_addr_le_cmp(peer, BT_ADDR_LE_ANY) &&
		    bt_addr_le_cmp(peer, &conns[i].dst)) {
			continue;
		}

		if (conns[i].state == state) {
			return bt_conn_get(&conns[i]);
		}
	}

	return NULL;
}

struct bt_conn *bt_conn_get(struct bt_conn *conn)
{
	atomic_inc(&conn->ref);

	BT_DBG("handle %u ref %u\n", conn->handle, atomic_get(&conn->ref));

	return conn;
}

void bt_conn_put(struct bt_conn *conn)
{
	atomic_dec(&conn->ref);

	BT_DBG("handle %u ref %u\n", conn->handle, atomic_get(&conn->ref));
}

const bt_addr_le_t *bt_conn_get_dst(const struct bt_conn *conn)
{
	return &conn->dst;
}

void bt_conn_set_auto_conn(struct bt_conn *conn, bool auto_conn)
{
	if (auto_conn) {
		atomic_set_bit(conn->flags, BT_CONN_AUTO_CONNECT);
	} else {
		atomic_clear_bit(conn->flags, BT_CONN_AUTO_CONNECT);
	}
}

static int bt_hci_disconnect(struct bt_conn *conn, uint8_t reason)
{
	struct bt_buf *buf;
	struct bt_hci_cp_disconnect *disconn;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_DISCONNECT, sizeof(*disconn));
	if (!buf) {
		return -ENOBUFS;
	}

	disconn = bt_buf_add(buf, sizeof(*disconn));
	disconn->handle = sys_cpu_to_le16(conn->handle);
	disconn->reason = reason;

	err = bt_hci_cmd_send(BT_HCI_OP_DISCONNECT, buf);
	if (err) {
		return err;
	}

	bt_conn_set_state(conn, BT_CONN_DISCONNECT);

	return 0;
}

static int bt_hci_connect_le_cancel(struct bt_conn *conn)
{
	int err;

	if (conn->timeout) {
		fiber_fiber_delayed_start_cancel(conn->timeout);
		conn->timeout = NULL;

		/* Drop the reference took by timeout fiber */
		bt_conn_put(conn);
	}

	err = bt_hci_cmd_send(BT_HCI_OP_LE_CREATE_CONN_CANCEL, NULL);
	if (err) {
		return err;
	}

	return 0;
}

int bt_conn_disconnect(struct bt_conn *conn, uint8_t reason)
{
	/* Disconnection is initiated by us, so auto connection shall
	 * be disabled. Otherwise the passive scan would be enabled
	 * and we could send LE Create Connection as soon as the remote
	 * starts advertising.
	 */
	bt_conn_set_auto_conn(conn, false);

	switch (conn->state) {
	case BT_CONN_CONNECT_SCAN:
		bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
		bt_le_scan_update();
		return 0;
	case BT_CONN_CONNECT:
		return bt_hci_connect_le_cancel(conn);
	case BT_CONN_CONNECTED:
		return bt_hci_disconnect(conn, reason);
	case BT_CONN_DISCONNECT:
		return 0;
	case BT_CONN_DISCONNECTED:
	default:
		return -ENOTCONN;
	}
}

struct bt_conn *bt_conn_create_le(const bt_addr_le_t *peer)
{
	struct bt_conn *conn;

	conn = bt_conn_lookup_addr_le(peer);
	if (conn) {
		switch (conn->state) {
		case BT_CONN_CONNECT_SCAN:
		case BT_CONN_CONNECT:
		case BT_CONN_CONNECTED:
			return conn;
		default:
			bt_conn_put(conn);
			return NULL;
		}
	}

	conn = bt_conn_add(peer);
	if (!conn) {
		return NULL;
	}

	bt_conn_set_state(conn, BT_CONN_CONNECT_SCAN);

	bt_le_scan_update();

	return conn;
}

int bt_conn_le_conn_update(struct bt_conn *conn, uint16_t min, uint16_t max,
			   uint16_t latency, uint16_t timeout)
{
	struct hci_cp_le_conn_update *conn_update;
	struct bt_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_CONN_UPDATE,
				sizeof(*conn_update));
	if (!buf) {
		return -ENOBUFS;
	}

	conn_update = bt_buf_add(buf, sizeof(*conn_update));
	memset(conn_update, 0, sizeof(*conn_update));
	conn_update->handle = sys_cpu_to_le16(conn->handle);
	conn_update->conn_interval_min = sys_cpu_to_le16(min);
	conn_update->conn_interval_max = sys_cpu_to_le16(max);
	conn_update->conn_latency = sys_cpu_to_le16(latency);
	conn_update->supervision_timeout = sys_cpu_to_le16(timeout);

	return bt_hci_cmd_send(BT_HCI_OP_LE_CONN_UPDATE, buf);
}
