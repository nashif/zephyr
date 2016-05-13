/* gap.c - Bluetooth GAP Tester */

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

#include <atomic.h>
#include <stdint.h>
#include <string.h>

#include <toolchain.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>

#include <misc/byteorder.h>

#include "bttester.h"

#define CONTROLLER_INDEX 0
#define CONTROLLER_NAME "btp_tester"

/* TODO add api for reading real address */
#define CONTROLLER_ADDR (&(bt_addr_t) {{1, 2, 3, 4, 5, 6}})

static atomic_t current_settings;
struct bt_conn_auth_cb cb;

static void le_connected(struct bt_conn *conn, uint8_t err)
{
	struct gap_device_connected_ev ev;
	const bt_addr_le_t *addr = bt_conn_get_dst(conn);

	if (err) {
		return;
	}

	memcpy(ev.address, addr->a.val, sizeof(ev.address));
	ev.address_type = addr->type;

	tester_send(BTP_SERVICE_ID_GAP, GAP_EV_DEVICE_CONNECTED,
		    CONTROLLER_INDEX, (uint8_t *) &ev, sizeof(ev));
}

static void le_disconnected(struct bt_conn *conn, uint8_t reason)
{
	struct gap_device_disconnected_ev ev;
	const bt_addr_le_t *addr = bt_conn_get_dst(conn);

	memcpy(ev.address, addr->a.val, sizeof(ev.address));
	ev.address_type = addr->type;

	tester_send(BTP_SERVICE_ID_GAP, GAP_EV_DEVICE_DISCONNECTED,
		    CONTROLLER_INDEX, (uint8_t *) &ev, sizeof(ev));
}

static struct bt_conn_cb conn_callbacks = {
	.connected = le_connected,
	.disconnected = le_disconnected,
};

static void supported_commands(uint8_t *data, uint16_t len)
{
	uint8_t cmds[3];
	struct gap_read_supported_commands_rp *rp = (void *) &cmds;

	memset(cmds, 0, sizeof(cmds));

	tester_set_bit(cmds, GAP_READ_SUPPORTED_COMMANDS);
	tester_set_bit(cmds, GAP_READ_CONTROLLER_INDEX_LIST);
	tester_set_bit(cmds, GAP_READ_CONTROLLER_INFO);
	tester_set_bit(cmds, GAP_SET_CONNECTABLE);
	tester_set_bit(cmds, GAP_SET_DISCOVERABLE);
	tester_set_bit(cmds, GAP_START_ADVERTISING);
	tester_set_bit(cmds, GAP_STOP_ADVERTISING);
	tester_set_bit(cmds, GAP_START_DISCOVERY);
	tester_set_bit(cmds, GAP_STOP_DISCOVERY);
	tester_set_bit(cmds, GAP_CONNECT);
	tester_set_bit(cmds, GAP_DISCONNECT);
	tester_set_bit(cmds, GAP_SET_IO_CAP);
	tester_set_bit(cmds, GAP_PAIR);
	tester_set_bit(cmds, GAP_PASSKEY_ENTRY);

	tester_send(BTP_SERVICE_ID_GAP, GAP_READ_SUPPORTED_COMMANDS,
		    CONTROLLER_INDEX, (uint8_t *) rp, sizeof(cmds));
}

static void controller_index_list(uint8_t *data,  uint16_t len)
{
	struct gap_read_controller_index_list_rp *rp;
	uint8_t buf[sizeof(*rp) + 1];

	rp = (void *) buf;

	rp->num = 1;
	rp->index[0] = CONTROLLER_INDEX;

	tester_send(BTP_SERVICE_ID_GAP, GAP_READ_CONTROLLER_INDEX_LIST,
		    BTP_INDEX_NONE, (uint8_t *) rp, sizeof(buf));
}

static void controller_info(uint8_t *data, uint16_t len)
{
	struct gap_read_controller_info_rp rp;
	uint32_t supported_settings;

	memset(&rp, 0, sizeof(rp));
	memcpy(rp.address, CONTROLLER_ADDR, sizeof(bt_addr_t));

	supported_settings = BIT(GAP_SETTINGS_POWERED);
	supported_settings |= BIT(GAP_SETTINGS_CONNECTABLE);
	supported_settings |= BIT(GAP_SETTINGS_BONDABLE);
	supported_settings |= BIT(GAP_SETTINGS_LE);
	supported_settings |= BIT(GAP_SETTINGS_ADVERTISING);

	rp.supported_settings = sys_cpu_to_le32(supported_settings);
	rp.current_settings = sys_cpu_to_le32(current_settings);

	memcpy(rp.name, CONTROLLER_NAME, sizeof(CONTROLLER_NAME));

	tester_send(BTP_SERVICE_ID_GAP, GAP_READ_CONTROLLER_INFO,
		    CONTROLLER_INDEX, (uint8_t *) &rp, sizeof(rp));
}

static void set_connectable(uint8_t *data, uint16_t len)
{
	const struct gap_set_connectable_cmd *cmd = (void *) data;
	struct gap_set_connectable_rp rp;

	if (cmd->connectable) {
		atomic_set_bit(&current_settings, GAP_SETTINGS_CONNECTABLE);
	} else {
		atomic_clear_bit(&current_settings, GAP_SETTINGS_CONNECTABLE);
	}

	rp.current_settings = sys_cpu_to_le32(current_settings);

	tester_send(BTP_SERVICE_ID_GAP, GAP_SET_CONNECTABLE, CONTROLLER_INDEX,
		    (uint8_t *) &rp, sizeof(rp));
}

static uint8_t ad_flags = BT_LE_AD_NO_BREDR;
static struct bt_data ad[10] = {
	BT_DATA(BT_DATA_FLAGS, &ad_flags, sizeof(ad_flags)),
};
static struct bt_data sd[10];

static void set_discoverable(uint8_t *data, uint16_t len)
{
	const struct gap_set_discoverable_cmd *cmd = (void *) data;
	struct gap_set_discoverable_rp rp;

	switch (cmd->discoverable) {
	case GAP_NON_DISCOVERABLE:
		ad_flags &= ~(BT_LE_AD_GENERAL | BT_LE_AD_LIMITED);
		atomic_clear_bit(&current_settings, GAP_SETTINGS_DISCOVERABLE);
		break;
	case GAP_GENERAL_DISCOVERABLE:
		ad_flags &= ~BT_LE_AD_LIMITED;
		ad_flags |= BT_LE_AD_GENERAL;
		atomic_set_bit(&current_settings, GAP_SETTINGS_DISCOVERABLE);
		break;
	case GAP_LIMITED_DISCOVERABLE:
		ad_flags &= ~BT_LE_AD_GENERAL;
		ad_flags |= BT_LE_AD_LIMITED;
		atomic_set_bit(&current_settings, GAP_SETTINGS_DISCOVERABLE);
		break;
	default:
		tester_rsp(BTP_SERVICE_ID_GAP, GAP_SET_DISCOVERABLE,
			   CONTROLLER_INDEX, BTP_STATUS_FAILED);
		return;
	}

	rp.current_settings = sys_cpu_to_le32(current_settings);

	tester_send(BTP_SERVICE_ID_GAP, GAP_SET_DISCOVERABLE, CONTROLLER_INDEX,
		    (uint8_t *) &rp, sizeof(rp));
}

static void start_advertising(const uint8_t *data, uint16_t len)
{
	const struct gap_start_advertising_cmd *cmd = (void *) data;
	struct gap_start_advertising_rp rp;
	uint8_t adv_len, sd_len;
	bool adv_conn;
	int i;

	for (i = 0, adv_len = 1; i < cmd->adv_data_len; adv_len++) {
		if (adv_len >= ARRAY_SIZE(ad)) {
			BTTESTER_DBG("ad[] Out of memory");
			goto fail;
		}

		ad[adv_len].type = cmd->adv_data[i++];
		ad[adv_len].data_len = cmd->adv_data[i++];
		ad[adv_len].data = &cmd->adv_data[i];
		i += ad[adv_len].data_len;
	}

	for (i = 0, sd_len = 0; i < cmd->scan_rsp_len; sd_len++) {
		if (sd_len >= ARRAY_SIZE(sd)) {
			BTTESTER_DBG("sd[] Out of memory");
			goto fail;
		}

		sd[sd_len].type = cmd->scan_rsp[i++];
		sd[sd_len].data_len = cmd->scan_rsp[i++];
		sd[sd_len].data = &cmd->scan_rsp[i];
		i += sd[sd_len].data_len;
	}

	adv_conn = atomic_test_bit(&current_settings, GAP_SETTINGS_CONNECTABLE);

	/* BTP API don't allow to set empty scan response data. */
	if (bt_le_adv_start(adv_conn ? BT_LE_ADV_CONN : BT_LE_ADV_NCONN,
			    ad, adv_len, sd_len ? sd : NULL, sd_len) < 0) {
		BTTESTER_DBG("Failed to start advertising");
		goto fail;
	}

	atomic_set_bit(&current_settings, GAP_SETTINGS_ADVERTISING);
	rp.current_settings = sys_cpu_to_le32(current_settings);

	tester_send(BTP_SERVICE_ID_GAP, GAP_START_ADVERTISING, CONTROLLER_INDEX,
		    (uint8_t *) &rp, sizeof(rp));
	return;
fail:
	tester_rsp(BTP_SERVICE_ID_GAP, GAP_START_ADVERTISING, CONTROLLER_INDEX,
		   BTP_STATUS_FAILED);
}

static void stop_advertising(const uint8_t *data, uint16_t len)
{
	struct gap_stop_advertising_rp rp;

	if (bt_le_adv_stop() < 0) {
		tester_rsp(BTP_SERVICE_ID_GAP, GAP_STOP_ADVERTISING,
			   CONTROLLER_INDEX, BTP_STATUS_FAILED);
		return;
	}

	atomic_clear_bit(&current_settings, GAP_SETTINGS_ADVERTISING);
	rp.current_settings = sys_cpu_to_le32(current_settings);

	tester_send(BTP_SERVICE_ID_GAP, GAP_STOP_ADVERTISING, CONTROLLER_INDEX,
		    (uint8_t *) &rp, sizeof(rp));
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t evtype,
			 const uint8_t *ad, uint8_t len)
{
	struct gap_device_found_ev *ev;
	uint8_t buf[sizeof(*ev) + len];

	ev = (void*) buf;

	memcpy(ev->address, addr->a.val, sizeof(ev->address));
	ev->address_type = addr->type;

	ev->flags = GAP_DEVICE_FOUND_FLAG_RSSI;
	ev->rssi = rssi;

	if (evtype == BT_LE_ADV_SCAN_RSP) {
		ev->flags |= GAP_DEVICE_FOUND_FLAG_SD;
	} else {
		ev->flags |= GAP_DEVICE_FOUND_FLAG_AD;
	}

	ev->eir_data_len = len;
	if (len) {
		memcpy(ev->eir_data, ad, len);
	}

	tester_send(BTP_SERVICE_ID_GAP, GAP_EV_DEVICE_FOUND, CONTROLLER_INDEX,
		    buf, sizeof(buf));
}

static void start_discovery(const uint8_t *data, uint16_t len)
{
	const struct gap_start_discovery_cmd *cmd = (void *) data;
	uint8_t status;

	/* only LE scan is supported */
	if (cmd->flags & GAP_DISCOVERY_FLAG_BREDR) {
		status = BTP_STATUS_FAILED;
		goto reply;
	}

	if (bt_le_scan_start(cmd->flags & GAP_DISCOVERY_FLAG_LE_ACTIVE_SCAN ?
			     BT_LE_SCAN_ACTIVE : BT_LE_SCAN_PASSIVE,
			     device_found) < 0) {
		status = BTP_STATUS_FAILED;
		goto reply;
	}

	status = BTP_STATUS_SUCCESS;
reply:
	tester_rsp(BTP_SERVICE_ID_GAP, GAP_START_DISCOVERY, CONTROLLER_INDEX,
		   status);
}

static void stop_discovery(const uint8_t *data, uint16_t len)
{
	uint8_t status = BTP_STATUS_SUCCESS;

	if (bt_le_scan_stop() < 0) {
		status = BTP_STATUS_FAILED;
	}

	tester_rsp(BTP_SERVICE_ID_GAP, GAP_STOP_DISCOVERY, CONTROLLER_INDEX,
		   status);
}

static void connect(const uint8_t *data, uint16_t len)
{
	struct bt_conn *conn;
	uint8_t status;

	conn = bt_conn_create_le((bt_addr_le_t *) data,
				 BT_LE_CONN_PARAM_DEFAULT);
	if (!conn) {
		status = BTP_STATUS_FAILED;
		goto rsp;
	}

	bt_conn_unref(conn);
	status = BTP_STATUS_SUCCESS;

rsp:
	tester_rsp(BTP_SERVICE_ID_GAP, GAP_CONNECT, CONTROLLER_INDEX, status);
}

static void disconnect(const uint8_t *data, uint16_t len)
{
	struct bt_conn *conn;
	uint8_t status;

	conn = bt_conn_lookup_addr_le((bt_addr_le_t *) data);
	if (!conn) {
		status = BTP_STATUS_FAILED;
		goto rsp;
	}

	if (bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN)) {
		status = BTP_STATUS_FAILED;
	} else {
		status = BTP_STATUS_SUCCESS;
	}

	bt_conn_unref(conn);

rsp:
	tester_rsp(BTP_SERVICE_ID_GAP, GAP_DISCONNECT, CONTROLLER_INDEX,
		   status);
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	struct gap_passkey_display_ev ev;
	const bt_addr_le_t *addr = bt_conn_get_dst(conn);

	memcpy(ev.address, addr->a.val, sizeof(ev.address));
	ev.address_type = addr->type;
	ev.passkey = sys_cpu_to_le32(passkey);

	tester_send(BTP_SERVICE_ID_GAP, GAP_EV_PASSKEY_DISPLAY,
		    CONTROLLER_INDEX, (uint8_t *) &ev, sizeof(ev));
}

static void auth_passkey_entry(struct bt_conn *conn)
{
	struct gap_passkey_entry_req_ev ev;
	const bt_addr_le_t *addr = bt_conn_get_dst(conn);

	memcpy(ev.address, addr->a.val, sizeof(ev.address));
	ev.address_type = addr->type;

	tester_send(BTP_SERVICE_ID_GAP, GAP_EV_PASSKEY_ENTRY_REQ,
		    CONTROLLER_INDEX, (uint8_t *) &ev, sizeof(ev));
}

static void auth_cancel(struct bt_conn *conn)
{
	/* TODO */
}

static void set_io_cap(const uint8_t *data, uint16_t len)
{
	const struct gap_set_io_cap_cmd *cmd = (void *) data;
	uint8_t status;

	/* Reset io cap requirements */
	memset(&cb, 0, sizeof(cb));
	bt_conn_auth_cb_register(NULL);

	switch (cmd->io_cap) {
	case GAP_IO_CAP_DISPLAY_ONLY:
		cb.cancel = auth_cancel;
		cb.passkey_display = auth_passkey_display;
		break;
	case GAP_IO_CAP_KEYBOARD_DISPLAY:
		cb.cancel = auth_cancel;
		cb.passkey_display = auth_passkey_display;
		cb.passkey_entry = auth_passkey_entry;
		break;
	case GAP_IO_CAP_NO_INPUT_OUTPUT:
		cb.cancel = auth_cancel;
		break;
	case GAP_IO_CAP_KEYBOARD_ONLY:
		cb.cancel = auth_cancel;
		cb.passkey_entry = auth_passkey_entry;
		break;
	case GAP_IO_CAP_DISPLAY_YESNO:
	default:
		status = BTP_STATUS_FAILED;
		goto rsp;
	}

	if (bt_conn_auth_cb_register(&cb)) {
		status = BTP_STATUS_FAILED;
		goto rsp;
	}

	status = BTP_STATUS_SUCCESS;

rsp:
	tester_rsp(BTP_SERVICE_ID_GAP, GAP_SET_IO_CAP, CONTROLLER_INDEX,
		   status);
}

static void pair(const uint8_t *data, uint16_t len)
{
	struct bt_conn *conn;
	uint8_t status;

	conn = bt_conn_lookup_addr_le((bt_addr_le_t *) data);
	if (!conn) {
		status = BTP_STATUS_FAILED;
		goto rsp;
	}

	if (bt_conn_security(conn, BT_SECURITY_MEDIUM)) {
		status = BTP_STATUS_FAILED;
		bt_conn_unref(conn);
		goto rsp;
	}

	bt_conn_unref(conn);
	status = BTP_STATUS_SUCCESS;

rsp:
	tester_rsp(BTP_SERVICE_ID_GAP, GAP_PAIR, CONTROLLER_INDEX, status);
}

static void passkey_entry(const uint8_t *data, uint16_t len)
{
	const struct gap_passkey_entry_cmd *cmd = (void *) data;
	struct bt_conn *conn;
	uint8_t status;

	conn = bt_conn_lookup_addr_le((bt_addr_le_t *) data);
	if (!conn) {
		status = BTP_STATUS_FAILED;
		goto rsp;
	}

	bt_conn_auth_passkey_entry(conn, sys_le32_to_cpu(cmd->passkey));

	bt_conn_unref(conn);
	status = BTP_STATUS_SUCCESS;

rsp:
	tester_rsp(BTP_SERVICE_ID_GAP, GAP_PASSKEY_ENTRY, CONTROLLER_INDEX,
		   status);
}

void tester_handle_gap(uint8_t opcode, uint8_t index, uint8_t *data,
		       uint16_t len)
{
	switch (opcode) {
	case GAP_READ_SUPPORTED_COMMANDS:
	case GAP_READ_CONTROLLER_INDEX_LIST:
		if (index != BTP_INDEX_NONE){
			tester_rsp(BTP_SERVICE_ID_GAP, opcode, index,
				   BTP_STATUS_FAILED);
			return;
		}
		break;
	default:
		if (index != CONTROLLER_INDEX){
			tester_rsp(BTP_SERVICE_ID_GAP, opcode, index,
				   BTP_STATUS_FAILED);
			return;
		}
		break;
	}

	switch (opcode) {
	case GAP_READ_SUPPORTED_COMMANDS:
		supported_commands(data, len);
		return;
	case GAP_READ_CONTROLLER_INDEX_LIST:
		controller_index_list(data, len);
		return;
	case GAP_READ_CONTROLLER_INFO:
		controller_info(data, len);
		return;
	case GAP_SET_CONNECTABLE:
		set_connectable(data, len);
		return;
	case GAP_SET_DISCOVERABLE:
		set_discoverable(data, len);
		return;
	case GAP_START_ADVERTISING:
		start_advertising(data, len);
		return;
	case GAP_STOP_ADVERTISING:
		stop_advertising(data, len);
		return;
	case GAP_START_DISCOVERY:
		start_discovery(data, len);
		return;
	case GAP_STOP_DISCOVERY:
		stop_discovery(data, len);
		return;
	case GAP_CONNECT:
		connect(data, len);
		return;
	case GAP_DISCONNECT:
		disconnect(data, len);
		return;
	case GAP_SET_IO_CAP:
		set_io_cap(data, len);
		return;
	case GAP_PAIR:
		pair(data, len);
		return;
	case GAP_PASSKEY_ENTRY:
		passkey_entry(data, len);
		return;
	default:
		tester_rsp(BTP_SERVICE_ID_GAP, opcode, index,
			   BTP_STATUS_UNKNOWN_CMD);
		return;
	}
}

static void tester_init_gap_cb(int err)
{
	if (err) {
		tester_rsp(BTP_SERVICE_ID_CORE, CORE_REGISTER_SERVICE,
			   BTP_INDEX_NONE, BTP_STATUS_FAILED);
		return;
	}

	atomic_clear(&current_settings);
	atomic_set_bit(&current_settings, GAP_SETTINGS_POWERED);
	atomic_set_bit(&current_settings, GAP_SETTINGS_CONNECTABLE);
	atomic_set_bit(&current_settings, GAP_SETTINGS_BONDABLE);
	atomic_set_bit(&current_settings, GAP_SETTINGS_LE);

	bt_conn_cb_register(&conn_callbacks);

	tester_rsp(BTP_SERVICE_ID_CORE, CORE_REGISTER_SERVICE, BTP_INDEX_NONE,
		   BTP_STATUS_SUCCESS);
}

uint8_t tester_init_gap(void)
{
	if (bt_enable(tester_init_gap_cb) < 0) {
		return BTP_STATUS_FAILED;
	}

	return BTP_STATUS_SUCCESS;
}
