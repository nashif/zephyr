/* conn.h - Bluetooth connection handling */

/*
 * Copyright (c) 2015 Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3) Neither the name of Intel Corporation nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define BT_CONN_TX_STACK_SIZE		256
#define BT_CONN_RX_STACK_SIZE		1024

enum {
	BT_CONN_DISCONNECTED,
	BT_CONN_CONNECTED,
};

/* L2CAP signaling channel specific context */
struct bt_conn_l2cap {
	uint8_t			ident;
};

/* ATT channel specific context */
struct bt_conn_att {
};

/* SMP channel specific context */
struct bt_conn_smp {
	/* Pairing Request PDU */
	uint8_t			preq[7];

	/* Pairing Response PDU */
	uint8_t			prsp[7];

	/* Local random number */
	uint8_t			prnd[16];
};

struct bt_conn {
	struct bt_dev		*dev;
	uint16_t		handle;

	uint8_t			dst[6];
	uint8_t			dst_type;

	uint16_t		rx_len;
	struct bt_buf		*rx;

	/* Queue for outgoing ACL data */
	struct nano_fifo	tx_queue;
	struct nano_fifo	rx_queue;

	/* Fixed channel contexts */
	struct bt_conn_l2cap	l2cap;
	struct bt_conn_att	att;
	struct bt_conn_smp	smp;

	uint8_t			le_conn_interval;

	uint8_t			ref;

	uint8_t			state;

	char			tx_stack[BT_CONN_TX_STACK_SIZE];
	char			rx_stack[BT_CONN_RX_STACK_SIZE];
};

/* Prepare a new buffer to be sent over the connection */
struct bt_buf *bt_conn_create_pdu(struct bt_conn *conn);

/* Process incoming data for a connection */
void bt_conn_recv(struct bt_conn *conn, struct bt_buf *buf, uint8_t flags);

/* Send data over a connection */
void bt_conn_send(struct bt_conn *conn, struct bt_buf *buf);

/* Add a new connection */
struct bt_conn *bt_conn_add(struct bt_dev *dev, uint16_t handle);

/* Delete an existing connection */
void bt_conn_del(struct bt_conn *conn);

/* Look up an existing connection */
struct bt_conn *bt_conn_lookup(uint16_t handle);

/* Increment conn reference count */
struct bt_conn *bt_conn_get(struct bt_conn *conn);

/* Decrement conn reference count */
void bt_conn_put(struct bt_conn *conn);
