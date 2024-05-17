/* SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later */

#include <sys/types.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(mctp, CONFIG_MCTP_LOG_LEVEL);



#define pr_fmt(x) "serial: " x

/*
 * @fn: A function that will copy data from the buffer at src into the dst object
 * @dst: An opaque object to pass as state to fn
 * @src: A pointer to the buffer of data to copy to dst
 * @len: The length of the data pointed to by src
 * @return: 0 on succes, negative error code on failure
 *
 * Pre-condition: fn returns a write count or a negative error code
 * Post-condition: All bytes written or an error has occurred
 */
#define mctp_write_all(fn, dst, src, len)                                      \
	({                                                                     \
		__typeof__(src) __src = src;                                       \
		ssize_t wrote;                                                 \
		while (len) {                                                  \
			wrote = fn(dst, __src, len);                           \
			if (wrote < 0)                                         \
				break;                                         \
			__src += wrote;                                        \
			len -= wrote;                                          \
		}                                                              \
		len ? wrote : 0;                                               \
	})

static ssize_t mctp_serial_write(const struct device *uart, const void *buf, size_t nbyte)
{
	for (size_t i = 0; i < nbyte; i++) {
		uart_poll_out(uart, ((uint8_t *)buf)[i]);
	}

	return nbyte;
}

#include <zephyr/mctp/mctp.h>
#include "libmctp-alloc.h"

#include <zephyr/mctp/mctp-serial.h>

struct mctp_binding_serial {
	struct mctp_binding binding;
	const struct device *uart;
	unsigned long bus_id;

	mctp_serial_tx_fn tx_fn;
	void *tx_fn_data;

	/* receive buffer and state */
	uint8_t rxbuf[1024];
	struct mctp_pktbuf *rx_pkt;
	uint8_t rx_exp_len;
	uint16_t rx_fcs;
	enum {
		STATE_WAIT_SYNC_START,
		STATE_WAIT_REVISION,
		STATE_WAIT_LEN,
		STATE_DATA,
		STATE_DATA_ESCAPED,
		STATE_WAIT_FCS1,
		STATE_WAIT_FCS2,
		STATE_WAIT_SYNC_END,
	} rx_state;

	/* temporary transmit buffer */
	uint8_t txbuf[256];
};

#define binding_to_serial(b)                                                   \
	CONTAINER_OF(b, struct mctp_binding_serial, binding)

#define MCTP_SERIAL_REVISION	 0x01
#define MCTP_SERIAL_FRAMING_FLAG 0x7e
#define MCTP_SERIAL_ESCAPE	 0x7d

struct mctp_serial_header {
	uint8_t flag;
	uint8_t revision;
	uint8_t len;
};

struct mctp_serial_trailer {
	uint8_t fcs_msb;
	uint8_t fcs_lsb;
	uint8_t flag;
};

/**
 * @brief Escape special characters in the packet
 * @details Escape 0x7e and 0x7d characters in the packet
 */
static size_t mctp_serial_pkt_escape(struct mctp_pktbuf *pkt, uint8_t *buf)
{
	uint8_t total_len;
	uint8_t *p;
	int i, j;

	total_len = pkt->end - pkt->mctp_hdr_off;

	p = (void *)mctp_pktbuf_hdr(pkt);

	for (i = 0, j = 0; i < total_len; i++, j++) {
		uint8_t c = p[i];
		if (c == 0x7e || c == 0x7d) {
			if (buf)
				buf[j] = 0x7d;
			j++;
			c ^= 0x20;
		}
		if (buf)
			buf[j] = c;
	}

	return j;
}

static int mctp_binding_serial_tx(struct mctp_binding *b,
				  struct mctp_pktbuf *pkt)
{
	struct mctp_binding_serial *serial = binding_to_serial(b);
	struct mctp_serial_header *hdr;
	struct mctp_serial_trailer *tlr;
	uint8_t *buf;
	size_t len;

	/* the length field in the header excludes serial framing
	 * and escape sequences */
	len = mctp_pktbuf_size(pkt);

	hdr = (void *)serial->txbuf;
	hdr->flag = MCTP_SERIAL_FRAMING_FLAG;
	hdr->revision = MCTP_SERIAL_REVISION;
	hdr->len = len;

	buf = (void *)(hdr + 1);

	len = mctp_serial_pkt_escape(pkt, NULL);
	if (len + sizeof(*hdr) + sizeof(*tlr) > sizeof(serial->txbuf))
		return -EMSGSIZE;

	mctp_serial_pkt_escape(pkt, buf);

	buf += len;

	tlr = (void *)buf;
	tlr->flag = MCTP_SERIAL_FRAMING_FLAG;
	/* todo: trailer FCS */
	tlr->fcs_msb = 0;
	tlr->fcs_lsb = 0;

	len += sizeof(*hdr) + sizeof(*tlr);

	if (!serial->tx_fn)
		return mctp_write_all(mctp_serial_write, serial->uart,
				      &serial->txbuf[0], len);

	return mctp_write_all(serial->tx_fn, serial->tx_fn_data,
			      &serial->txbuf[0], len);
}

static void mctp_serial_finish_packet(struct mctp_binding_serial *serial,
				      bool valid)
{
	struct mctp_pktbuf *pkt = serial->rx_pkt;
	assert(pkt);

	if (valid)
		mctp_bus_rx(&serial->binding, pkt);

	serial->rx_pkt = NULL;
}

static void mctp_serial_start_packet(struct mctp_binding_serial *serial,
				     uint8_t len)
{
	serial->rx_pkt = mctp_pktbuf_alloc(&serial->binding, len);
}

static void mctp_rx_consume_one(struct mctp_binding_serial *serial, uint8_t c)
{
	struct mctp_pktbuf *pkt = serial->rx_pkt;

	LOG_DBG("state: %d, char 0x%02x", serial->rx_state, c);

	assert(!pkt == (serial->rx_state == STATE_WAIT_SYNC_START ||
			serial->rx_state == STATE_WAIT_REVISION ||
			serial->rx_state == STATE_WAIT_LEN));

	switch (serial->rx_state) {
	case STATE_WAIT_SYNC_START:
		if (c != MCTP_SERIAL_FRAMING_FLAG) {
			LOG_DBG("lost sync, dropping packet");
			if (pkt)
				mctp_serial_finish_packet(serial, false);
		} else {
			serial->rx_state = STATE_WAIT_REVISION;
		}
		break;

	case STATE_WAIT_REVISION:
		if (c == MCTP_SERIAL_REVISION) {
			serial->rx_state = STATE_WAIT_LEN;
		} else if (c == MCTP_SERIAL_FRAMING_FLAG) {
			/* Handle the case where there are bytes dropped in request,
			 * and the state machine is out of sync. The failed request's
			 * trailing footer i.e. 0x7e would be interpreted as next
			 * request's framing footer. So if we are in STATE_WAIT_REVISION
			 * and receive 0x7e byte, then contine to stay in
			 * STATE_WAIT_REVISION
			 */
			LOG_DBG(
				"Received serial framing flag 0x%02x while waiting"
				" for serial revision 0x%02x.",
				c, MCTP_SERIAL_REVISION);
		} else {
			LOG_DBG("invalid revision 0x%02x", c);
			serial->rx_state = STATE_WAIT_SYNC_START;
		}
		break;
	case STATE_WAIT_LEN:
		if (c > serial->binding.pkt_size ||
		    c < sizeof(struct mctp_hdr)) {
			LOG_DBG("invalid size %d", c);
			serial->rx_state = STATE_WAIT_SYNC_START;
		} else {
			mctp_serial_start_packet(serial, 0);
			pkt = serial->rx_pkt;
			serial->rx_exp_len = c;
			serial->rx_state = STATE_DATA;
		}
		break;

	case STATE_DATA:
		if (c == MCTP_SERIAL_ESCAPE) {
			serial->rx_state = STATE_DATA_ESCAPED;
		} else {
			mctp_pktbuf_push(pkt, &c, 1);
			if (pkt->end - pkt->mctp_hdr_off == serial->rx_exp_len)
				serial->rx_state = STATE_WAIT_FCS1;
		}
		break;

	case STATE_DATA_ESCAPED:
		c ^= 0x20;
		mctp_pktbuf_push(pkt, &c, 1);
		if (pkt->end - pkt->mctp_hdr_off == serial->rx_exp_len)
			serial->rx_state = STATE_WAIT_FCS1;
		else
			serial->rx_state = STATE_DATA;
		break;

	case STATE_WAIT_FCS1:
		serial->rx_fcs = c << 8;
		serial->rx_state = STATE_WAIT_FCS2;
		break;
	case STATE_WAIT_FCS2:
		serial->rx_fcs |= c;
		/* todo: check fcs */
		serial->rx_state = STATE_WAIT_SYNC_END;
		break;

	case STATE_WAIT_SYNC_END:
		if (c == MCTP_SERIAL_FRAMING_FLAG) {
			mctp_serial_finish_packet(serial, true);
		} else {
			LOG_DBG("missing end frame marker");
			mctp_serial_finish_packet(serial, false);
		}
		serial->rx_state = STATE_WAIT_SYNC_START;
		break;
	}

	LOG_DBG(" -> state: %d", serial->rx_state);
}
static void mctp_rx_consume(struct mctp_binding_serial *serial, const void *buf,
			    size_t len)
{
	size_t i;
	const uint8_t *bbuf = buf;

	for (i = 0; i < len; i++) {
		mctp_rx_consume_one(serial, bbuf[i]);
	}
}

int mctp_serial_read(struct mctp_binding_serial *serial)
{
	ssize_t len;

	for (len = 0; len < sizeof(serial->rxbuf); len++) {
		int res = uart_poll_in(serial->uart, &serial->rxbuf[len]);

		if (res != 0) {
			break;
		}
	}

	if (len == 0)
		return -1;

	if (len < 0) {
		LOG_ERR("can't read from serial device: %m");
		return -1;
	}

	mctp_rx_consume(serial, serial->rxbuf, len);

	return 0;
}

void mctp_serial_open(struct mctp_binding_serial *serial, const struct device *uart)
{
	serial->uart = uart;
}


void mctp_serial_set_tx_fn(struct mctp_binding_serial *serial,
			   mctp_serial_tx_fn fn, void *data)
{
	serial->tx_fn = fn;
	serial->tx_fn_data = data;
}

int mctp_serial_rx(struct mctp_binding_serial *serial, const void *buf,
		   size_t len)
{
	mctp_rx_consume(serial, buf, len);
	return 0;
}

static int mctp_serial_core_start(struct mctp_binding *binding)
{
	mctp_binding_set_tx_enabled(binding, true);
	return 0;
}

struct mctp_binding *mctp_binding_serial_core(struct mctp_binding_serial *b)
{
	return &b->binding;
}

struct mctp_binding_serial *mctp_serial_init(void)
{
	struct mctp_binding_serial *serial;

	serial = __mctp_alloc(sizeof(*serial));
	memset(serial, 0, sizeof(*serial));
	serial->uart = NULL;
	serial->rx_state = STATE_WAIT_SYNC_START;
	serial->rx_pkt = NULL;
	serial->binding.name = "serial";
	serial->binding.version = 1;
	serial->binding.pkt_size = MCTP_PACKET_SIZE(MCTP_BTU);
	serial->binding.pkt_header = 0;
	serial->binding.pkt_trailer = 0;

	serial->binding.start = mctp_serial_core_start;
	serial->binding.tx = mctp_binding_serial_tx;

	return serial;
}

void mctp_serial_destroy(struct mctp_binding_serial *serial)
{
	__mctp_free(serial);
}
