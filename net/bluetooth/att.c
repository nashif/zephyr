/* att.c - Attribute protocol handling */

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
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>
#include <bluetooth/driver.h>

#include "hci_core.h"
#include "conn_internal.h"
#include "l2cap_internal.h"
#include "smp.h"
#include "att_internal.h"
#include "gatt_internal.h"

#if !defined(CONFIG_BLUETOOTH_DEBUG_ATT)
#undef BT_DBG
#define BT_DBG(fmt, ...)
#endif

#define BT_GATT_PERM_READ_MASK			(BT_GATT_PERM_READ | \
						BT_GATT_PERM_READ_ENCRYPT | \
						BT_GATT_PERM_READ_AUTHEN)
#define BT_GATT_PERM_WRITE_MASK			(BT_GATT_PERM_WRITE | \
						BT_GATT_PERM_WRITE_ENCRYPT | \
						BT_GATT_PERM_WRITE_AUTHEN)
#define BT_GATT_PERM_ENCRYPT_MASK		(BT_GATT_PERM_READ_ENCRYPT | \
						BT_GATT_PERM_WRITE_ENCRYPT)
#define BT_GATT_PERM_AUTHEN_MASK		(BT_GATT_PERM_READ_AUTHEN | \
						BT_GATT_PERM_WRITE_AUTHEN)
#define BT_ATT_OP_CMD_FLAG			0x40

/* ATT request context */
struct bt_att_req {
	bt_att_func_t		func;
	void			*user_data;
	bt_att_destroy_t	destroy;
	struct net_buf		*buf;
#if defined(CONFIG_BLUETOOTH_SMP)
	bool			retrying;
#endif /* CONFIG_BLUETOOTH_SMP */
};

/* ATT channel specific context */
struct bt_att {
	/* The channel this context is associated with */
	struct bt_l2cap_chan	chan;
	struct bt_att_req	req;
	 /* TODO: Allow more than one pending request */
};

static struct bt_att bt_att_pool[CONFIG_BLUETOOTH_MAX_CONN];

/*
 * Pool for outgoing ATT packets. Reserve one buffer per connection plus
 * one additional one in case cloning is needed.
 */
static struct nano_fifo att_buf;
static NET_BUF_POOL(att_pool, CONFIG_BLUETOOTH_MAX_CONN + 1,
		    BT_L2CAP_BUF_SIZE(CONFIG_BLUETOOTH_ATT_MTU),
		    &att_buf, NULL, BT_BUF_USER_DATA_MIN);

static void att_req_destroy(struct bt_att_req *req)
{
	if (req->buf) {
		net_buf_unref(req->buf);
	}

	if (req->destroy) {
		req->destroy(req->user_data);
	}

	memset(req, 0, sizeof(*req));
}

static void send_err_rsp(struct bt_conn *conn, uint8_t req, uint16_t handle,
			 uint8_t err)
{
	struct bt_att_error_rsp *rsp;
	struct net_buf *buf;

	/* Ignore opcode 0x00 */
	if (!req) {
		return;
	}

	buf = bt_att_create_pdu(conn, BT_ATT_OP_ERROR_RSP, sizeof(*rsp));
	if (!buf) {
		return;
	}

	rsp = net_buf_add(buf, sizeof(*rsp));
	rsp->request = req;
	rsp->handle = sys_cpu_to_le16(handle);
	rsp->error = err;

	bt_l2cap_send(conn, BT_L2CAP_CID_ATT, buf);
}

static uint8_t att_mtu_req(struct bt_att *att, struct net_buf *buf)
{
	struct bt_conn *conn = att->chan.conn;
	struct bt_att_exchange_mtu_req *req;
	struct bt_att_exchange_mtu_rsp *rsp;
	struct net_buf *pdu;
	uint16_t mtu_client, mtu_server;

	req = (void *)buf->data;

	mtu_client = sys_le16_to_cpu(req->mtu);

	BT_DBG("Client MTU %u", mtu_client);

	/* Check if MTU is valid */
	if (mtu_client < BT_ATT_DEFAULT_LE_MTU) {
		return BT_ATT_ERR_INVALID_PDU;
	}

	pdu = bt_att_create_pdu(conn, BT_ATT_OP_MTU_RSP, sizeof(*rsp));
	if (!pdu) {
		return BT_ATT_ERR_UNLIKELY;
	}

	mtu_server = CONFIG_BLUETOOTH_ATT_MTU;

	BT_DBG("Server MTU %u", mtu_server);

	rsp = net_buf_add(pdu, sizeof(*rsp));
	rsp->mtu = sys_cpu_to_le16(mtu_server);

	bt_l2cap_send(conn, BT_L2CAP_CID_ATT, pdu);

	/* BLUETOOTH SPECIFICATION Version 4.2 [Vol 3, Part F] page 484:
	 *
	 * A device's Exchange MTU Request shall contain the same MTU as the
	 * device's Exchange MTU Response (i.e. the MTU shall be symmetric).
	 */
	att->chan.rx.mtu = min(mtu_client, mtu_server);
	att->chan.tx.mtu = att->chan.rx.mtu;

	BT_DBG("Negotiated MTU %u", att->chan.rx.mtu);
	return 0;
}

static uint8_t att_handle_rsp(struct bt_att *att, void *pdu, uint16_t len,
			      uint8_t err)
{
	struct bt_att_req req;

	if (!att->req.func) {
		return 0;
	}

	/* Release cloned buffer */
	if (att->req.buf) {
		net_buf_unref(att->req.buf);
		att->req.buf = NULL;
	}

	/* Reset request before callback so another request can be queued */
	memcpy(&req, &att->req, sizeof(req));
	att->req.func = NULL;

	req.func(att->chan.conn, err, pdu, len, req.user_data);

	att_req_destroy(&req);

	return 0;
}

static uint8_t att_mtu_rsp(struct bt_att *att, struct net_buf *buf)
{
	struct bt_att_exchange_mtu_rsp *rsp;
	uint16_t mtu;

	if (!att) {
		return 0;
	}

	rsp = (void *)buf->data;

	mtu = sys_le16_to_cpu(rsp->mtu);

	BT_DBG("Server MTU %u", mtu);

	/* Check if MTU is valid */
	if (mtu < BT_ATT_DEFAULT_LE_MTU) {
		return att_handle_rsp(att, NULL, 0, BT_ATT_ERR_INVALID_PDU);
	}

	att->chan.rx.mtu = min(mtu, CONFIG_BLUETOOTH_ATT_MTU);

	/* BLUETOOTH SPECIFICATION Version 4.2 [Vol 3, Part F] page 484:
	 *
	 * A device's Exchange MTU Request shall contain the same MTU as the
	 * device's Exchange MTU Response (i.e. the MTU shall be symmetric).
	 */
	att->chan.tx.mtu = att->chan.rx.mtu;

	BT_DBG("Negotiated MTU %u", att->chan.rx.mtu);

	return att_handle_rsp(att, rsp, buf->len, 0);
}

static bool range_is_valid(uint16_t start, uint16_t end, uint16_t *err)
{
	/* Handle 0 is invalid */
	if (!start || !end) {
		if (err) {
			*err = 0;
		}
		return false;
	}

	/* Check if range is valid */
	if (start > end) {
		if (err) {
			*err = start;
		}
		return false;
	}

	return true;
}

struct find_info_data {
	struct bt_att *att;
	struct net_buf *buf;
	struct bt_att_find_info_rsp *rsp;
	union {
		struct bt_att_info_16 *info16;
		struct bt_att_info_128 *info128;
	};
};

static uint8_t find_info_cb(const struct bt_gatt_attr *attr, void *user_data)
{
	struct find_info_data *data = user_data;
	struct bt_att *att = data->att;

	BT_DBG("handle 0x%04x", attr->handle);

	/* Initialize rsp at first entry */
	if (!data->rsp) {
		data->rsp = net_buf_add(data->buf, sizeof(*data->rsp));
		data->rsp->format = (attr->uuid->type == BT_UUID_TYPE_16) ?
				    BT_ATT_INFO_16 : BT_ATT_INFO_128;
	}

	switch (data->rsp->format) {
	case BT_ATT_INFO_16:
		if (attr->uuid->type != BT_UUID_TYPE_16) {
			return BT_GATT_ITER_STOP;
		}

		/* Fast foward to next item position */
		data->info16 = net_buf_add(data->buf, sizeof(*data->info16));
		data->info16->handle = sys_cpu_to_le16(attr->handle);
		data->info16->uuid = sys_cpu_to_le16(BT_UUID_16(attr->uuid)->val);

		if (att->chan.tx.mtu - data->buf->len > sizeof(*data->info16)) {
			return BT_GATT_ITER_CONTINUE;
		}

		break;
	case BT_ATT_INFO_128:
		if (attr->uuid->type != BT_UUID_TYPE_128) {
			return BT_GATT_ITER_STOP;
		}

		/* Fast foward to next item position */
		data->info128 = net_buf_add(data->buf, sizeof(*data->info128));
		data->info128->handle = sys_cpu_to_le16(attr->handle);
		memcpy(data->info128->uuid, BT_UUID_128(attr->uuid)->val,
		       sizeof(data->info128->uuid));

		if (att->chan.tx.mtu - data->buf->len >
		    sizeof(*data->info128)) {
			return BT_GATT_ITER_CONTINUE;
		}
	}

	return BT_GATT_ITER_STOP;
}

static uint8_t att_find_info_rsp(struct bt_att *att, uint16_t start_handle,
				 uint16_t end_handle)
{
	struct bt_conn *conn = att->chan.conn;
	struct find_info_data data;

	memset(&data, 0, sizeof(data));

	data.buf = bt_att_create_pdu(conn, BT_ATT_OP_FIND_INFO_RSP, 0);
	if (!data.buf) {
		return BT_ATT_ERR_UNLIKELY;
	}

	data.att = att;
	bt_gatt_foreach_attr(start_handle, end_handle, find_info_cb, &data);

	if (!data.rsp) {
		net_buf_unref(data.buf);
		/* Respond since handle is set */
		send_err_rsp(conn, BT_ATT_OP_FIND_INFO_REQ, start_handle,
			     BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
		return 0;
	}

	bt_l2cap_send(conn, BT_L2CAP_CID_ATT, data.buf);

	return 0;
}

static uint8_t att_find_info_req(struct bt_att *att, struct net_buf *buf)
{
	struct bt_conn *conn = att->chan.conn;
	struct bt_att_find_info_req *req;
	uint16_t start_handle, end_handle, err_handle;

	req = (void *)buf->data;

	start_handle = sys_le16_to_cpu(req->start_handle);
	end_handle = sys_le16_to_cpu(req->end_handle);

	BT_DBG("start_handle 0x%04x end_handle 0x%04x", start_handle,
	       end_handle);

	if (!range_is_valid(start_handle, end_handle, &err_handle)) {
		send_err_rsp(conn, BT_ATT_OP_FIND_INFO_REQ, err_handle,
			     BT_ATT_ERR_INVALID_HANDLE);
		return 0;
	}

	return att_find_info_rsp(att, start_handle, end_handle);
}

struct find_type_data {
	struct bt_att *att;
	struct net_buf *buf;
	struct bt_att_handle_group *group;
	const void *value;
	uint8_t value_len;
	uint8_t err;
};

static uint8_t find_type_cb(const struct bt_gatt_attr *attr, void *user_data)
{
	struct find_type_data *data = user_data;
	struct bt_att *att = data->att;
	struct bt_conn *conn = att->chan.conn;
	int read;
	uint8_t uuid[16];

	/* Skip secondary services */
	if (!bt_uuid_cmp(attr->uuid, BT_UUID_GATT_SECONDARY)) {
		data->group = NULL;
		return BT_GATT_ITER_CONTINUE;
	}

	/* Update group end_handle if not a primary service */
	if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_PRIMARY)) {
		if (data->group && attr->handle > data->group->end_handle) {
			data->group->end_handle = sys_cpu_to_le16(attr->handle);
		}
		return BT_GATT_ITER_CONTINUE;
	}

	BT_DBG("handle 0x%04x", attr->handle);

	/* stop if there is no space left */
	if (att->chan.tx.mtu - data->buf->len < sizeof(*data->group)) {
		return BT_GATT_ITER_STOP;
	}

	/* Read attribute value and store in the buffer */
	read = attr->read(conn, attr, uuid, sizeof(uuid), 0);
	if (read < 0) {
		/*
		 * Since we don't know if it is the service with requested UUID,
		 * we cannot respond with an error to this request.
		 */
		data->group = NULL;
		return BT_GATT_ITER_CONTINUE;
	}

	/* Check if data matches */
	if (read != data->value_len || memcmp(data->value, uuid, read)) {
		data->group = NULL;
		return BT_GATT_ITER_CONTINUE;
	}

	/* If service has been found, error should be cleared */
	data->err = 0x00;

	/* Fast foward to next item position */
	data->group = net_buf_add(data->buf, sizeof(*data->group));
	data->group->start_handle = sys_cpu_to_le16(attr->handle);
	data->group->end_handle = sys_cpu_to_le16(attr->handle);

	/* continue to find the end_handle */
	return BT_GATT_ITER_CONTINUE;
}

static uint8_t att_find_type_rsp(struct bt_att *att, uint16_t start_handle,
				 uint16_t end_handle, const void *value,
				 uint8_t value_len)
{
	struct bt_conn *conn = att->chan.conn;
	struct find_type_data data;

	memset(&data, 0, sizeof(data));

	data.buf = bt_att_create_pdu(conn, BT_ATT_OP_FIND_TYPE_RSP, 0);
	if (!data.buf) {
		return BT_ATT_ERR_UNLIKELY;
	}

	data.att = att;
	data.group = NULL;
	data.value = value;
	data.value_len = value_len;

	/* Pre-set error in case no service will be found */
	data.err = BT_ATT_ERR_ATTRIBUTE_NOT_FOUND;

	bt_gatt_foreach_attr(start_handle, end_handle, find_type_cb, &data);

	/* If error has not been cleared, no service has been found */
	if (data.err) {
		net_buf_unref(data.buf);
		/* Respond since handle is set */
		send_err_rsp(conn, BT_ATT_OP_FIND_TYPE_REQ, start_handle,
			     data.err);
		return 0;
	}

	bt_l2cap_send(conn, BT_L2CAP_CID_ATT, data.buf);

	return 0;
}

static uint8_t att_find_type_req(struct bt_att *att, struct net_buf *buf)
{
	struct bt_conn *conn = att->chan.conn;
	struct bt_att_find_type_req *req;
	uint16_t start_handle, end_handle, err_handle, type;
	uint8_t *value;

	req = (void *)buf->data;

	start_handle = sys_le16_to_cpu(req->start_handle);
	end_handle = sys_le16_to_cpu(req->end_handle);
	type = sys_le16_to_cpu(req->type);
	value = net_buf_pull(buf, sizeof(*req));

	BT_DBG("start_handle 0x%04x end_handle 0x%04x type %u", start_handle,
	       end_handle, type);

	if (!range_is_valid(start_handle, end_handle, &err_handle)) {
		send_err_rsp(conn, BT_ATT_OP_FIND_TYPE_REQ, err_handle,
			     BT_ATT_ERR_INVALID_HANDLE);
		return 0;
	}

	/* The Attribute Protocol Find By Type Value Request shall be used with
	 * the Attribute Type parameter set to the UUID for «Primary Service»
	 * and the Attribute Value set to the 16-bit Bluetooth UUID or 128-bit
	 * UUID for the specific primary service.
	 */
	if (type != BT_UUID_GATT_PRIMARY_VAL) {
		send_err_rsp(conn, BT_ATT_OP_FIND_TYPE_REQ, start_handle,
			     BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
		return 0;
	}

	return att_find_type_rsp(att, start_handle, end_handle, value,
				 buf->len);
}

static bool uuid_create(struct bt_uuid *uuid, struct net_buf *buf)
{
	switch (buf->len) {
	case 2:
		uuid->type = BT_UUID_TYPE_16;
		BT_UUID_16(uuid)->val = net_buf_pull_le16(buf);
		return true;
	case 16:
		uuid->type = BT_UUID_TYPE_128;
		memcpy(BT_UUID_128(uuid)->val, buf->data, buf->len);
		return true;
	}

	return false;
}

static uint8_t check_perm(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  uint8_t mask)
{
	if ((mask & BT_GATT_PERM_READ) &&
	    !(attr->perm & BT_GATT_PERM_READ_MASK)) {
		return BT_ATT_ERR_READ_NOT_PERMITTED;
	}

	if ((mask & BT_GATT_PERM_WRITE) &&
	    !(attr->perm & BT_GATT_PERM_WRITE_MASK)) {
		return BT_ATT_ERR_WRITE_NOT_PERMITTED;
	}

	mask &= attr->perm;
	if (mask & BT_GATT_PERM_AUTHEN_MASK) {
#if defined(CONFIG_BLUETOOTH_SMP)
		if (conn->sec_level < BT_SECURITY_HIGH) {
			return BT_ATT_ERR_AUTHENTICATION;
		}
#else
		return BT_ATT_ERR_AUTHENTICATION;
#endif /* CONFIG_BLUETOOTH_SMP */
	}

	if ((mask & BT_GATT_PERM_ENCRYPT_MASK)) {
#if defined(CONFIG_BLUETOOTH_SMP)
		if (!conn->encrypt) {
			return BT_ATT_ERR_INSUFFICIENT_ENCRYPTION;
		}
#else
		return BT_ATT_ERR_INSUFFICIENT_ENCRYPTION;
#endif /* CONFIG_BLUETOOTH_SMP */
	}

	return 0;
}

static uint8_t err_to_att(int err)
{
	BT_DBG("%d", err);

	if (err < 0 && err >= -0xff) {
		return -err;
	}

	return BT_ATT_ERR_UNLIKELY;
}

struct read_type_data {
	struct bt_att *att;
	struct bt_uuid *uuid;
	struct net_buf *buf;
	struct bt_att_read_type_rsp *rsp;
	struct bt_att_data *item;
	uint8_t err;
};

static uint8_t read_type_cb(const struct bt_gatt_attr *attr, void *user_data)
{
	struct read_type_data *data = user_data;
	struct bt_att *att = data->att;
	struct bt_conn *conn = att->chan.conn;
	int read;

	/* Skip if doesn't match */
	if (bt_uuid_cmp(attr->uuid, data->uuid)) {
		return BT_GATT_ITER_CONTINUE;
	}

	BT_DBG("handle 0x%04x", attr->handle);

	/*
	 * If an attribute in the set of requested attributes would cause an
	 * Error Response then this attribute cannot be included in a
	 * Read By Type Response and the attributes before this attribute
	 * shall be returned
	 *
	 * If the first attribute in the set of requested attributes would
	 * cause an Error Response then no other attributes in the requested
	 * attributes can be considered.
	 */
	data->err = check_perm(conn, attr, BT_GATT_PERM_READ_MASK);
	if (data->err) {
		if (data->rsp->len) {
			data->err = 0x00;
		}
		return BT_GATT_ITER_STOP;
	}

	/*
	 * If any attribute is founded in handle range it means that error
	 * should be changed from pre-set: attr not found error to no error.
	 */
	data->err = 0x00;

	/* Fast foward to next item position */
	data->item = net_buf_add(data->buf, sizeof(*data->item));
	data->item->handle = sys_cpu_to_le16(attr->handle);

	/* Read attribute value and store in the buffer */
	read = attr->read(conn, attr, data->buf->data + data->buf->len,
			  att->chan.tx.mtu - data->buf->len, 0);
	if (read < 0) {
		data->err = err_to_att(read);
		return BT_GATT_ITER_STOP;
	}

	if (!data->rsp->len) {
		/* Set len to be the first item found */
		data->rsp->len = read + sizeof(*data->item);
	} else if (data->rsp->len != read + sizeof(*data->item)) {
		/* All items should have the same size */
		data->buf->len -= sizeof(*data->item);
		return BT_GATT_ITER_STOP;
	}

	net_buf_add(data->buf, read);

	/* return true only if there are still space for more items */
	return att->chan.tx.mtu - data->buf->len > data->rsp->len ?
	       BT_GATT_ITER_CONTINUE : BT_GATT_ITER_STOP;
}

static uint8_t att_read_type_rsp(struct bt_att *att, struct bt_uuid *uuid,
				 uint16_t start_handle, uint16_t end_handle)
{
	struct bt_conn *conn = att->chan.conn;
	struct read_type_data data;

	memset(&data, 0, sizeof(data));

	data.buf = bt_att_create_pdu(conn, BT_ATT_OP_READ_TYPE_RSP,
				     sizeof(*data.rsp));
	if (!data.buf) {
		return BT_ATT_ERR_UNLIKELY;
	}

	data.att = att;
	data.uuid = uuid;
	data.rsp = net_buf_add(data.buf, sizeof(*data.rsp));
	data.rsp->len = 0;

	/* Pre-set error if no attr will be found in handle */
	data.err = BT_ATT_ERR_ATTRIBUTE_NOT_FOUND;

	bt_gatt_foreach_attr(start_handle, end_handle, read_type_cb, &data);

	if (data.err) {
		net_buf_unref(data.buf);
		/* Response here since handle is set */
		send_err_rsp(conn, BT_ATT_OP_READ_TYPE_REQ, start_handle,
			     data.err);
		return 0;
	}

	bt_l2cap_send(conn, BT_L2CAP_CID_ATT, data.buf);

	return 0;
}

static uint8_t att_read_type_req(struct bt_att *att, struct net_buf *buf)
{
	struct bt_conn *conn = att->chan.conn;
	struct bt_att_read_type_req *req;
	uint16_t start_handle, end_handle, err_handle;
	union {
		struct bt_uuid uuid;
		struct bt_uuid_16 u16;
		struct bt_uuid_128 u128;
	} u;

	/* Type can only be UUID16 or UUID128 */
	if (buf->len != sizeof(*req) + 2 && buf->len != sizeof(*req) + 16) {
		return BT_ATT_ERR_INVALID_PDU;
	}

	req = (void *)buf->data;

	start_handle = sys_le16_to_cpu(req->start_handle);
	end_handle = sys_le16_to_cpu(req->end_handle);
	net_buf_pull(buf, sizeof(*req));

	if (!uuid_create(&u.uuid, buf)) {
		return BT_ATT_ERR_UNLIKELY;
	}

	BT_DBG("start_handle 0x%04x end_handle 0x%04x type %s",
	       start_handle, end_handle, bt_uuid_str(&u.uuid));

	if (!range_is_valid(start_handle, end_handle, &err_handle)) {
		send_err_rsp(conn, BT_ATT_OP_READ_TYPE_REQ, err_handle,
			     BT_ATT_ERR_INVALID_HANDLE);
		return 0;
	}

	return att_read_type_rsp(att, &u.uuid, start_handle, end_handle);
}

struct read_data {
	struct bt_att *att;
	uint16_t offset;
	struct net_buf *buf;
	struct bt_att_read_rsp *rsp;
	uint8_t err;
};

static uint8_t read_cb(const struct bt_gatt_attr *attr, void *user_data)
{
	struct read_data *data = user_data;
	struct bt_att *att = data->att;
	struct bt_conn *conn = att->chan.conn;
	int read;

	BT_DBG("handle 0x%04x", attr->handle);

	data->rsp = net_buf_add(data->buf, sizeof(*data->rsp));

	/*
	 * If any attribute is founded in handle range it means that error
	 * should be changed from pre-set: invalid handle error to no error.
	 */
	data->err = 0x00;

	if (!attr->read) {
		data->err = BT_ATT_ERR_READ_NOT_PERMITTED;
		return BT_GATT_ITER_STOP;
	}

	/* Check attribute permissions */
	data->err = check_perm(conn, attr, BT_GATT_PERM_READ_MASK);
	if (data->err) {
		return BT_GATT_ITER_STOP;
	}

	/* Read attribute value and store in the buffer */
	read = attr->read(conn, attr, data->buf->data + data->buf->len,
			  att->chan.tx.mtu - data->buf->len, data->offset);
	if (read < 0) {
		data->err = err_to_att(read);
		return BT_GATT_ITER_STOP;
	}

	net_buf_add(data->buf, read);

	return BT_GATT_ITER_CONTINUE;
}

static uint8_t att_read_rsp(struct bt_att *att, uint8_t op, uint8_t rsp,
			    uint16_t handle, uint16_t offset)
{
	struct bt_conn *conn = att->chan.conn;
	struct read_data data;

	if (!handle) {
		return BT_ATT_ERR_INVALID_HANDLE;
	}

	memset(&data, 0, sizeof(data));

	data.buf = bt_att_create_pdu(conn, rsp, 0);
	if (!data.buf) {
		return BT_ATT_ERR_UNLIKELY;
	}

	data.att = att;
	data.offset = offset;

	/* Pre-set error if no attr will be found in handle */
	data.err = BT_ATT_ERR_INVALID_HANDLE;

	bt_gatt_foreach_attr(handle, handle, read_cb, &data);

	/* In case of error discard data and respond with an error */
	if (data.err) {
		net_buf_unref(data.buf);
		/* Respond here since handle is set */
		send_err_rsp(conn, op, handle, data.err);
		return 0;
	}

	bt_l2cap_send(conn, BT_L2CAP_CID_ATT, data.buf);

	return 0;
}

static uint8_t att_read_req(struct bt_att *att, struct net_buf *buf)
{
	struct bt_att_read_req *req;
	uint16_t handle;

	req = (void *)buf->data;

	handle = sys_le16_to_cpu(req->handle);

	BT_DBG("handle 0x%04x", handle);

	return att_read_rsp(att, BT_ATT_OP_READ_REQ, BT_ATT_OP_READ_RSP,
			    handle, 0);
}

static uint8_t att_read_blob_req(struct bt_att *att, struct net_buf *buf)
{
	struct bt_att_read_blob_req *req;
	uint16_t handle, offset;

	req = (void *)buf->data;

	handle = sys_le16_to_cpu(req->handle);
	offset = sys_le16_to_cpu(req->offset);

	BT_DBG("handle 0x%04x offset %u", handle, offset);

	return att_read_rsp(att, BT_ATT_OP_READ_BLOB_REQ,
			    BT_ATT_OP_READ_BLOB_RSP, handle, offset);
}

static uint8_t att_read_mult_req(struct bt_att *att, struct net_buf *buf)
{
	struct bt_conn *conn = att->chan.conn;
	struct read_data data;
	uint16_t handle;

	memset(&data, 0, sizeof(data));

	data.buf = bt_att_create_pdu(conn, BT_ATT_OP_READ_MULT_RSP, 0);
	if (!data.buf) {
		return BT_ATT_ERR_UNLIKELY;
	}

	data.att = att;

	while (buf->len >= sizeof(uint16_t)) {
		handle = net_buf_pull_le16(buf);

		BT_DBG("handle 0x%04x ", handle);

		/* An Error Response shall be sent by the server in response to
		 * the Read Multiple Request [....] if a read operation is not
		 * permitted on any of the Characteristic Values.
		 *
		 * If handle is not valid then return invalid handle error.
		 * If handle is found error will be cleared by read_cb.
		 */
		data.err = BT_ATT_ERR_INVALID_HANDLE;

		bt_gatt_foreach_attr(handle, handle, read_cb, &data);

		/* Stop reading in case of error */
		if (data.err) {
			net_buf_unref(data.buf);
			/* Respond here since handle is set */
			send_err_rsp(conn, BT_ATT_OP_READ_MULT_REQ, handle,
				     data.err);
			return 0;
		}
	}

	bt_l2cap_send(conn, BT_L2CAP_CID_ATT, data.buf);

	return 0;
}

struct read_group_data {
	struct bt_att *att;
	struct bt_uuid *uuid;
	struct net_buf *buf;
	struct bt_att_read_group_rsp *rsp;
	struct bt_att_group_data *group;
};

static uint8_t read_group_cb(const struct bt_gatt_attr *attr, void *user_data)
{
	struct read_group_data *data = user_data;
	struct bt_att *att = data->att;
	struct bt_conn *conn = att->chan.conn;
	int read;

	/* Update group end_handle if attribute is not a service */
	if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_PRIMARY) &&
	    bt_uuid_cmp(attr->uuid, BT_UUID_GATT_SECONDARY)) {
		if (data->group && attr->handle > data->group->end_handle) {
			data->group->end_handle = sys_cpu_to_le16(attr->handle);
		}
		return BT_GATT_ITER_CONTINUE;
	}

	/* If Group Type don't match skip */
	if (bt_uuid_cmp(attr->uuid, data->uuid)) {
		data->group = NULL;
		return BT_GATT_ITER_CONTINUE;
	}

	BT_DBG("handle 0x%04x", attr->handle);

	/* Stop if there is no space left */
	if (data->rsp->len &&
	    att->chan.tx.mtu - data->buf->len < data->rsp->len) {
		return BT_GATT_ITER_STOP;
	}

	/* Fast foward to next group position */
	data->group = net_buf_add(data->buf, sizeof(*data->group));

	/* Initialize group handle range */
	data->group->start_handle = sys_cpu_to_le16(attr->handle);
	data->group->end_handle = sys_cpu_to_le16(attr->handle);

	/* Read attribute value and store in the buffer */
	read = attr->read(conn, attr, data->buf->data + data->buf->len,
			  att->chan.tx.mtu - data->buf->len, 0);
	if (read < 0) {
		/* TODO: Handle read errors */
		return BT_GATT_ITER_STOP;
	}

	if (!data->rsp->len) {
		/* Set len to be the first group found */
		data->rsp->len = read + sizeof(*data->group);
	} else if (data->rsp->len != read + sizeof(*data->group)) {
		/* All groups entries should have the same size */
		data->buf->len -= sizeof(*data->group);
		return false;
	}

	net_buf_add(data->buf, read);

	/* Continue to find the end handle */
	return BT_GATT_ITER_CONTINUE;
}

static uint8_t att_read_group_rsp(struct bt_att *att, struct bt_uuid *uuid,
				  uint16_t start_handle, uint16_t end_handle)
{
	struct bt_conn *conn = att->chan.conn;
	struct read_group_data data;

	memset(&data, 0, sizeof(data));

	data.buf = bt_att_create_pdu(conn, BT_ATT_OP_READ_GROUP_RSP,
				     sizeof(*data.rsp));
	if (!data.buf) {
		return BT_ATT_ERR_UNLIKELY;
	}

	data.att = att;
	data.uuid = uuid;
	data.rsp = net_buf_add(data.buf, sizeof(*data.rsp));
	data.rsp->len = 0;
	data.group = NULL;

	bt_gatt_foreach_attr(start_handle, end_handle, read_group_cb, &data);

	if (!data.rsp->len) {
		net_buf_unref(data.buf);
		/* Respond here since handle is set */
		send_err_rsp(conn, BT_ATT_OP_READ_GROUP_REQ, start_handle,
			     BT_ATT_ERR_ATTRIBUTE_NOT_FOUND);
		return 0;
	}

	bt_l2cap_send(conn, BT_L2CAP_CID_ATT, data.buf);

	return 0;
}

static uint8_t att_read_group_req(struct bt_att *att, struct net_buf *buf)
{
	struct bt_conn *conn = att->chan.conn;
	struct bt_att_read_group_req *req;
	uint16_t start_handle, end_handle, err_handle;
	union {
		struct bt_uuid uuid;
		struct bt_uuid_16 u16;
		struct bt_uuid_128 u128;
	} u;

	/* Type can only be UUID16 or UUID128 */
	if (buf->len != sizeof(*req) + 2 && buf->len != sizeof(*req) + 16) {
		return BT_ATT_ERR_INVALID_PDU;
	}

	req = (void *)buf->data;

	start_handle = sys_le16_to_cpu(req->start_handle);
	end_handle = sys_le16_to_cpu(req->end_handle);
	net_buf_pull(buf, sizeof(*req));

	if (!uuid_create(&u.uuid, buf)) {
		return BT_ATT_ERR_UNLIKELY;
	}

	BT_DBG("start_handle 0x%04x end_handle 0x%04x type %s",
	       start_handle, end_handle, bt_uuid_str(&u.uuid));

	if (!range_is_valid(start_handle, end_handle, &err_handle)) {
		send_err_rsp(conn, BT_ATT_OP_READ_GROUP_REQ, err_handle,
			     BT_ATT_ERR_INVALID_HANDLE);
		return 0;
	}

	/* Core v4.2, Vol 3, sec 2.5.3 Attribute Grouping:
	 * Not all of the grouping attributes can be used in the ATT
	 * Read By Group Type Request. The «Primary Service» and «Secondary
	 * Service» grouping types may be used in the Read By Group Type
	 * Request. The «Characteristic» grouping type shall not be used in
	 * the ATT Read By Group Type Request.
	 */
	if (bt_uuid_cmp(&u.uuid, BT_UUID_GATT_PRIMARY) &&
	    bt_uuid_cmp(&u.uuid, BT_UUID_GATT_SECONDARY)) {
		send_err_rsp(conn, BT_ATT_OP_READ_GROUP_REQ, start_handle,
			     BT_ATT_ERR_UNSUPPORTED_GROUP_TYPE);
		return 0;
	}

	return att_read_group_rsp(att, &u.uuid, start_handle, end_handle);
}

struct write_data {
	struct bt_conn *conn;
	struct net_buf *buf;
	uint8_t op;
	const void *value;
	uint8_t len;
	uint16_t offset;
	uint8_t err;
};

static uint8_t write_cb(const struct bt_gatt_attr *attr, void *user_data)
{
	struct write_data *data = user_data;
	int write;

	BT_DBG("handle 0x%04x", attr->handle);

	/* Check for write support and flush support in case of prepare */
	if (!attr->write ||
	    (data->op == BT_ATT_OP_PREPARE_WRITE_REQ && !attr->flush)) {
		data->err = BT_ATT_ERR_WRITE_NOT_PERMITTED;
		return BT_GATT_ITER_STOP;
	}

	/* Check attribute permissions */
	data->err = check_perm(data->conn, attr, BT_GATT_PERM_WRITE_MASK);
	if (data->err) {
		return BT_GATT_ITER_STOP;
	}

	/* Read attribute value and store in the buffer */
	write = attr->write(data->conn, attr, data->value, data->len,
			    data->offset);
	if (write < 0 || write != data->len) {
		data->err = err_to_att(write);
		return BT_GATT_ITER_STOP;
	}

	/* Flush in case of regular write operation */
	if (attr->flush && data->op != BT_ATT_OP_PREPARE_WRITE_REQ) {
		write = attr->flush(data->conn, attr, BT_GATT_FLUSH_SYNC);
		if (write < 0) {
			data->err = err_to_att(write);
			return BT_GATT_ITER_STOP;
		}
	}

	data->err = 0;

	return BT_GATT_ITER_CONTINUE;
}

static uint8_t att_write_rsp(struct bt_conn *conn, uint8_t op, uint8_t rsp,
			     uint16_t handle, uint16_t offset,
			     const void *value, uint8_t len)
{
	struct write_data data;

	if (!handle) {
		return BT_ATT_ERR_INVALID_HANDLE;
	}

	memset(&data, 0, sizeof(data));

	/* Only allocate buf if required to respond */
	if (rsp) {
		data.buf = bt_att_create_pdu(conn, rsp, 0);
		if (!data.buf) {
			return BT_ATT_ERR_UNLIKELY;
		}
	}

	data.conn = conn;
	data.op = op;
	data.offset = offset;
	data.value = value;
	data.len = len;
	data.err = BT_ATT_ERR_INVALID_HANDLE;

	bt_gatt_foreach_attr(handle, handle, write_cb, &data);

	if (data.err) {
		/* Don't send error response when user attribute write handler
		 * returns invalid offset or invalid attribute value length.
		 * Such response needs to be sent when execute write request
		 * is received.
		 * Refer to BT SIG 4.2 [Vol 3, Part F, 3.4.6.1] page 504
		 */
		if (data.op == BT_ATT_OP_PREPARE_WRITE_REQ &&
		    (data.err == BT_ATT_ERR_INVALID_OFFSET ||
		     data.err == BT_ATT_ERR_INVALID_ATTRIBUTE_LEN)) {
			goto done;
		}

		/* In case of error discard data and respond with an error */
		if (rsp) {
			net_buf_unref(data.buf);
			/* Respond here since handle is set */
			send_err_rsp(conn, op, handle, data.err);
		}
		return 0;
	}

done:
	if (data.buf) {
		/* Add prepare write response */
		if (rsp == BT_ATT_OP_PREPARE_WRITE_RSP) {
			struct bt_att_prepare_write_rsp *rsp;

			rsp = net_buf_add(data.buf, sizeof(*rsp));
			rsp->handle = sys_cpu_to_le16(handle);
			rsp->offset = sys_cpu_to_le16(offset);
			net_buf_add(data.buf, len);
			memcpy(rsp->value, value, len);
		}
		bt_l2cap_send(conn, BT_L2CAP_CID_ATT, data.buf);
	}

	return 0;
}

static uint8_t att_write_req(struct bt_att *att, struct net_buf *buf)
{
	struct bt_conn *conn = att->chan.conn;
	struct bt_att_write_req *req;
	uint16_t handle;

	req = (void *)buf->data;

	handle = sys_le16_to_cpu(req->handle);
	net_buf_pull(buf, sizeof(*req));

	BT_DBG("handle 0x%04x", handle);

	return att_write_rsp(conn, BT_ATT_OP_WRITE_REQ, BT_ATT_OP_WRITE_RSP,
			     handle, 0, buf->data, buf->len);
}

static uint8_t att_prepare_write_req(struct bt_att *att, struct net_buf *buf)
{
	struct bt_conn *conn = att->chan.conn;
	struct bt_att_prepare_write_req *req;
	uint16_t handle, offset;

	req = (void *)buf->data;

	handle = sys_le16_to_cpu(req->handle);
	offset = sys_le16_to_cpu(req->offset);
	net_buf_pull(buf, sizeof(*req));

	BT_DBG("handle 0x%04x offset %u", handle, offset);

	return att_write_rsp(conn, BT_ATT_OP_PREPARE_WRITE_REQ,
			     BT_ATT_OP_PREPARE_WRITE_RSP, handle, offset,
			     buf->data, buf->len);
}

struct flush_data {
	struct bt_conn *conn;
	struct net_buf *buf;
	uint8_t flags;
	uint8_t err;
};

static uint8_t flush_cb(const struct bt_gatt_attr *attr, void *user_data)
{
	struct flush_data *data = user_data;
	int err;

	/* If attribute cannot be flushed continue to next */
	if (!attr->flush) {
		return BT_GATT_ITER_CONTINUE;
	}

	BT_DBG("handle 0x%04x flags 0x%02x", attr->handle, data->flags);

	/* Flush attribute any data cached to be written */
	err = attr->flush(data->conn, attr, data->flags);
	if (err < 0) {
		data->err = err_to_att(err);
		return BT_GATT_ITER_STOP;
	}

	data->err = 0;

	return BT_GATT_ITER_CONTINUE;
}

static uint8_t att_exec_write_rsp(struct bt_conn *conn, uint8_t flags)
{
	struct flush_data data;

	memset(&data, 0, sizeof(data));

	data.buf = bt_att_create_pdu(conn, BT_ATT_OP_EXEC_WRITE_RSP, 0);
	if (!data.buf) {
		return BT_ATT_ERR_UNLIKELY;
	}

	data.conn = conn;
	data.flags = flags;

	/* Apply to the whole database */
	bt_gatt_foreach_attr(0x0001, 0xffff, flush_cb, &data);

	/* In case of error discard data */
	if (data.err) {
		net_buf_unref(data.buf);
		return data.err;
	}

	bt_l2cap_send(conn, BT_L2CAP_CID_ATT, data.buf);

	return 0;
}

static uint8_t att_exec_write_req(struct bt_att *att, struct net_buf *buf)
{
	struct bt_conn *conn = att->chan.conn;
	struct bt_att_exec_write_req *req;

	req = (void *)buf->data;

	BT_DBG("flags 0x%02x", req->flags);

	return att_exec_write_rsp(conn, req->flags);
}

static uint8_t att_write_cmd(struct bt_att *att, struct net_buf *buf)
{
	struct bt_conn *conn = att->chan.conn;
	struct bt_att_write_cmd *req;
	uint16_t handle;

	if (buf->len < sizeof(*req)) {
		/* Commands don't have any response */
		return 0;
	}

	req = (void *)buf->data;

	handle = sys_le16_to_cpu(req->handle);

	BT_DBG("handle 0x%04x", handle);

	return att_write_rsp(conn, 0, 0, handle, 0, buf->data, buf->len);
}

static uint8_t att_signed_write_cmd(struct bt_att *att, struct net_buf *buf)
{
	struct bt_conn *conn = att->chan.conn;
	struct bt_att_signed_write_cmd *req;
	uint16_t handle;
	int err;

	req = (void *)buf->data;

	handle = sys_le16_to_cpu(req->handle);

	BT_DBG("handle 0x%04x", handle);

	/* Verifying data requires full buffer including attribute header */
	net_buf_push(buf, sizeof(struct bt_att_hdr));
	err = bt_smp_sign_verify(conn, buf);
	if (err) {
		BT_ERR("Error verifying data");
		/* No response for this command */
		return 0;
	}

	net_buf_pull(buf, sizeof(struct bt_att_hdr));
	net_buf_pull(buf, sizeof(*req));

	return att_write_rsp(conn, 0, 0, handle, 0, buf->data,
			     buf->len - sizeof(struct bt_att_signature));
}

#if defined(CONFIG_BLUETOOTH_SMP)
static int att_change_security(struct bt_conn *conn, uint8_t err)
{
	bt_security_t sec;

	switch (err) {
	case BT_ATT_ERR_INSUFFICIENT_ENCRYPTION:
		if (conn->sec_level >= BT_SECURITY_MEDIUM)
			return -EALREADY;
		sec = BT_SECURITY_MEDIUM;
		break;
	case BT_ATT_ERR_AUTHENTICATION:
		if (conn->sec_level >= BT_SECURITY_HIGH)
			return -EALREADY;
		sec = BT_SECURITY_HIGH;
		break;
	default:
		return -EINVAL;
	}

	return bt_conn_security(conn, sec);
}
#endif /* CONFIG_BLUETOOTH_SMP */

static uint8_t att_error_rsp(struct bt_att *att, struct net_buf *buf)
{
	struct bt_att_req *req = &att->req;
	struct bt_att_error_rsp *rsp;
	struct bt_att_hdr *hdr;
	uint8_t err;

	rsp = (void *)buf->data;

	BT_DBG("request 0x%02x handle 0x%04x error 0x%02x", rsp->request,
	       sys_le16_to_cpu(rsp->handle), rsp->error);

	if (!req->buf) {
		err = BT_ATT_ERR_UNLIKELY;
		goto done;
	}

	hdr = (void *)req->buf->data;

	err = rsp->request == hdr->code ? rsp->error : BT_ATT_ERR_UNLIKELY;
#if defined(CONFIG_BLUETOOTH_SMP)
	if (req->retrying) {
		goto done;
	}

	/* Check if security needs to be changed */
	if (!att_change_security(att->chan.conn, err)) {
		req->retrying = true;
		/* Wait security_changed: TODO: Handle fail case */
		return 0;
	}
#endif /* CONFIG_BLUETOOTH_SMP */

done:
	return att_handle_rsp(att, NULL, 0, err);
}

static uint8_t att_handle_find_info_rsp(struct bt_att *att,
					struct net_buf *buf)
{
	BT_DBG("");

	return att_handle_rsp(att, buf->data, buf->len, 0);
}

static uint8_t att_handle_find_type_rsp(struct bt_att *att,
					struct net_buf *buf)
{
	BT_DBG("");

	return att_handle_rsp(att, buf->data, buf->len, 0);
}

static uint8_t att_handle_read_type_rsp(struct bt_att *att,
					struct net_buf *buf)
{
	BT_DBG("");

	return att_handle_rsp(att, buf->data, buf->len, 0);
}

static uint8_t att_handle_read_rsp(struct bt_att *att,
				   struct net_buf *buf)
{
	BT_DBG("");

	return att_handle_rsp(att, buf->data, buf->len, 0);
}

static uint8_t att_handle_read_blob_rsp(struct bt_att *att,
					struct net_buf *buf)
{
	BT_DBG("");

	return att_handle_rsp(att, buf->data, buf->len, 0);
}

static uint8_t att_handle_read_mult_rsp(struct bt_att *att,
					struct net_buf *buf)
{
	BT_DBG("");

	return att_handle_rsp(att, buf->data, buf->len, 0);
}

static uint8_t att_handle_write_rsp(struct bt_att *att,
				    struct net_buf *buf)
{
	BT_DBG("");

	return att_handle_rsp(att, buf->data, buf->len, 0);
}

static uint8_t att_handle_prepare_write_rsp(struct bt_att *att,
					    struct net_buf *buf)
{
	BT_DBG("");

	return att_handle_rsp(att, buf->data, buf->len, 0);
}

static uint8_t att_handle_exec_write_rsp(struct bt_att *att,
					 struct net_buf *buf)
{
	BT_DBG("");

	return att_handle_rsp(att, buf->data, buf->len, 0);
}

static uint8_t att_notify(struct bt_att *att, struct net_buf *buf)
{
	struct bt_conn *conn = att->chan.conn;
	uint16_t handle;

	handle = net_buf_pull_le16(buf);

	BT_DBG("handle 0x%04x", handle);

	bt_gatt_notification(conn, handle, buf->data, buf->len);

	return 0;
}

static uint8_t att_indicate(struct bt_att *att, struct net_buf *buf)
{
	struct bt_conn *conn = att->chan.conn;
	uint16_t handle;

	handle = net_buf_pull_le16(buf);

	BT_DBG("handle 0x%04x", handle);

	bt_gatt_notification(conn, handle, buf->data, buf->len);

	buf = bt_att_create_pdu(conn, BT_ATT_OP_CONFIRM, 0);
	if (!buf) {
		return 0;
	}

	bt_l2cap_send(conn, BT_L2CAP_CID_ATT, buf);

	return 0;
}

static uint8_t att_confirm(struct bt_att *att, struct net_buf *buf)
{
	BT_DBG("");

	return att_handle_rsp(att, buf->data, buf->len, 0);
}

static const struct {
	uint8_t  op;
	uint8_t  (*func)(struct bt_att *att, struct net_buf *buf);
	uint8_t  expect_len;
} handlers[] = {
	{ BT_ATT_OP_ERROR_RSP, att_error_rsp,
	  sizeof(struct bt_att_error_rsp) },
	{ BT_ATT_OP_MTU_REQ, att_mtu_req,
	  sizeof(struct bt_att_exchange_mtu_req) },
	{ BT_ATT_OP_MTU_RSP, att_mtu_rsp,
	  sizeof(struct bt_att_exchange_mtu_rsp) },
	{ BT_ATT_OP_FIND_INFO_REQ, att_find_info_req,
	  sizeof(struct bt_att_find_info_req) },
	{ BT_ATT_OP_FIND_INFO_RSP, att_handle_find_info_rsp,
	  sizeof(struct bt_att_find_info_rsp) },
	{ BT_ATT_OP_FIND_TYPE_REQ, att_find_type_req,
	  sizeof(struct bt_att_find_type_req) },
	{ BT_ATT_OP_FIND_TYPE_RSP, att_handle_find_type_rsp,
	  sizeof(struct bt_att_find_type_rsp) },
	{ BT_ATT_OP_READ_TYPE_REQ, att_read_type_req,
	  sizeof(struct bt_att_read_type_req) },
	{ BT_ATT_OP_READ_TYPE_RSP, att_handle_read_type_rsp,
	  sizeof(struct bt_att_read_type_rsp) },
	{ BT_ATT_OP_READ_REQ, att_read_req,
	  sizeof(struct bt_att_read_req) },
	{ BT_ATT_OP_READ_RSP, att_handle_read_rsp,
	  sizeof(struct bt_att_read_rsp) },
	{ BT_ATT_OP_READ_BLOB_REQ, att_read_blob_req,
	  sizeof(struct bt_att_read_blob_req) },
	{ BT_ATT_OP_READ_BLOB_RSP, att_handle_read_blob_rsp,
	  sizeof(struct bt_att_read_blob_rsp) },
	{ BT_ATT_OP_READ_MULT_REQ, att_read_mult_req,
	  BT_ATT_READ_MULT_MIN_LEN_REQ },
	{ BT_ATT_OP_READ_MULT_RSP, att_handle_read_mult_rsp,
	  sizeof(struct bt_att_read_mult_rsp) },
	{ BT_ATT_OP_READ_GROUP_REQ, att_read_group_req,
	  sizeof(struct bt_att_read_group_req) },
	{ BT_ATT_OP_WRITE_REQ, att_write_req,
	  sizeof(struct bt_att_write_req) },
	{ BT_ATT_OP_WRITE_RSP, att_handle_write_rsp, 0 },
	{ BT_ATT_OP_PREPARE_WRITE_REQ, att_prepare_write_req,
	  sizeof(struct bt_att_prepare_write_req) },
	{ BT_ATT_OP_PREPARE_WRITE_RSP, att_handle_prepare_write_rsp,
	  sizeof(struct bt_att_prepare_write_rsp) },
	{ BT_ATT_OP_EXEC_WRITE_REQ, att_exec_write_req,
	  sizeof(struct bt_att_exec_write_req) },
	{ BT_ATT_OP_EXEC_WRITE_RSP, att_handle_exec_write_rsp, 0 },
	{ BT_ATT_OP_NOTIFY, att_notify,
	  sizeof(struct bt_att_notify) },
	{ BT_ATT_OP_INDICATE, att_indicate,
	  sizeof(struct bt_att_indicate) },
	{ BT_ATT_OP_CONFIRM, att_confirm, 0 },
	{ BT_ATT_OP_WRITE_CMD, att_write_cmd,
	  sizeof(struct bt_att_write_cmd) },
	{ BT_ATT_OP_SIGNED_WRITE_CMD, att_signed_write_cmd,
	  sizeof(struct bt_att_write_cmd) + sizeof(struct bt_att_signature) },
};

static void bt_att_recv(struct bt_l2cap_chan *chan, struct net_buf *buf)
{
	struct bt_att *att = CONTAINER_OF(chan, struct bt_att, chan);
	struct bt_att_hdr *hdr = (void *)buf->data;
	uint8_t err = BT_ATT_ERR_NOT_SUPPORTED;
	size_t i;

	BT_ASSERT(att);

	if (buf->len < sizeof(*hdr)) {
		BT_ERR("Too small ATT PDU received");
		return;
	}

	BT_DBG("Received ATT code 0x%02x len %u", hdr->code, buf->len);

	net_buf_pull(buf, sizeof(*hdr));

	for (i = 0; i < ARRAY_SIZE(handlers); i++) {
		if (hdr->code != handlers[i].op) {
			continue;
		}

		if (buf->len < handlers[i].expect_len) {
			BT_ERR("Invalid len %u for code 0x%02x", buf->len,
			       hdr->code);
			err = BT_ATT_ERR_INVALID_PDU;
			break;
		}

		err = handlers[i].func(att, buf);
		break;
	}

	/* Commands don't have response */
	if ((hdr->code & BT_ATT_OP_CMD_FLAG)) {
		return;
	}

	if (err) {
		BT_DBG("ATT error 0x%02x", err);
		send_err_rsp(chan->conn, hdr->code, 0, err);
	}
}

static struct bt_att *att_chan_get(struct bt_conn *conn)
{
	struct bt_l2cap_chan *chan;

	chan = bt_l2cap_lookup_rx_cid(conn, BT_L2CAP_CID_ATT);
	if (!chan) {
		BT_ERR("Unable to find ATT channel");
		return NULL;
	}

	return CONTAINER_OF(chan, struct bt_att, chan);
}

struct net_buf *bt_att_create_pdu(struct bt_conn *conn, uint8_t op, size_t len)
{
	struct bt_att_hdr *hdr;
	struct net_buf *buf;
	struct bt_att *att;

	att = att_chan_get(conn);
	if (!att) {
		return NULL;
	}

	if (len + sizeof(op) > att->chan.tx.mtu) {
		BT_WARN("ATT MTU exceeded, max %u, wanted %zu",
			att->chan.tx.mtu, len + sizeof(op));
		return NULL;
	}

	buf = bt_l2cap_create_pdu(&att_buf);
	if (!buf) {
		return NULL;
	}

	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->code = op;

	return buf;
}

static void bt_att_connected(struct bt_l2cap_chan *chan)
{
	BT_DBG("chan %p cid 0x%04x", chan, chan->tx.cid);

	chan->tx.mtu = BT_ATT_DEFAULT_LE_MTU;
	chan->rx.mtu = BT_ATT_DEFAULT_LE_MTU;

	bt_gatt_connected(chan->conn);
}

static void bt_att_disconnected(struct bt_l2cap_chan *chan)
{
	struct bt_att *att = CONTAINER_OF(chan, struct bt_att, chan);

	BT_DBG("chan %p cid 0x%04x", chan, chan->tx.cid);

	/* Notify client if request is pending */
	att_handle_rsp(att, NULL, 0, BT_ATT_ERR_UNLIKELY);

	bt_gatt_disconnected(chan->conn);
	memset(att, 0, sizeof(*att));
}

#if defined(CONFIG_BLUETOOTH_SMP)
static void bt_att_encrypt_change(struct bt_l2cap_chan *chan)
{
	struct bt_att *att = CONTAINER_OF(chan, struct bt_att, chan);
	struct bt_conn *conn = chan->conn;
	struct bt_att_req *req;

	BT_DBG("chan %p conn %p handle %u sec_level 0x%02x", chan, conn,
	       conn->handle, conn->sec_level);

	if (conn->sec_level == BT_SECURITY_LOW) {
		return;
	}

	req = &att->req;
	if (!req->retrying) {
		return;
	}

	BT_DBG("Retrying");

	/* Resend buffer */
	bt_l2cap_send(conn, BT_L2CAP_CID_ATT, req->buf);
	req->buf = NULL;
}
#endif /* CONFIG_BLUETOOTH_SMP */

static int bt_att_accept(struct bt_conn *conn, struct bt_l2cap_chan **chan)
{
	int i;
	static struct bt_l2cap_chan_ops ops = {
		.connected = bt_att_connected,
		.disconnected = bt_att_disconnected,
		.recv = bt_att_recv,
#if defined(CONFIG_BLUETOOTH_SMP)
		.encrypt_change = bt_att_encrypt_change,
#endif /* CONFIG_BLUETOOTH_SMP */
	};

	BT_DBG("conn %p handle %u", conn, conn->handle);

	for (i = 0; i < ARRAY_SIZE(bt_att_pool); i++) {
		struct bt_att *att = &bt_att_pool[i];

		if (att->chan.conn) {
			continue;
		}

		att->chan.ops = &ops;

		*chan = &att->chan;

		return 0;
	}

	BT_ERR("No available ATT context for conn %p", conn);

	return -ENOMEM;
}

void bt_att_init(void)
{
	static struct bt_l2cap_fixed_chan chan = {
		.cid		= BT_L2CAP_CID_ATT,
		.accept		= bt_att_accept,
	};

	net_buf_pool_init(att_pool);

	bt_l2cap_le_fixed_chan_register(&chan);
}

uint16_t bt_att_get_mtu(struct bt_conn *conn)
{
	struct bt_att *att;

	att = att_chan_get(conn);
	if (!att) {
		return 0;
	}

	/* tx and rx MTU shall be symmetric */
	return att->chan.tx.mtu;
}

int bt_att_send(struct bt_conn *conn, struct net_buf *buf, bt_att_func_t func,
		void *user_data, bt_att_destroy_t destroy)
{
	struct bt_att *att;
	struct bt_att_hdr *hdr = (void *)buf->data;

	if (!conn) {
		return -EINVAL;
	}

	att = att_chan_get(conn);
	if (!att) {
		return -ENOTCONN;
	}

	if (func) {
		/* Check if there is a request pending */
		if (att->req.func) {
			 /* TODO: Allow more than one pending request */
			return -EBUSY;
		}

		att->req.buf = net_buf_clone(buf);
#if defined(CONFIG_BLUETOOTH_SMP)
		att->req.retrying = false;
#endif /* CONFIG_BLUETOOTH_SMP */
		att->req.func = func;
		att->req.user_data = user_data;
		att->req.destroy = destroy;
	}

	if (hdr->code == BT_ATT_OP_SIGNED_WRITE_CMD) {
		int err;

		err = bt_smp_sign(conn, buf);
		if (err) {
			BT_ERR("Error signing data");
			return err;
		}
	}

	bt_l2cap_send(conn, BT_L2CAP_CID_ATT, buf);

	return 0;
}

void bt_att_cancel(struct bt_conn *conn)
{
	struct bt_att *att;

	if (!conn) {
		return;
	}

	att = att_chan_get(conn);
	if (!att) {
		return;
	}

	att_req_destroy(&att->req);
}
