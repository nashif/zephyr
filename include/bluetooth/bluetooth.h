/*! @file
 *  @brief Bluetooth subsystem core APIs.
 */

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
#ifndef __BT_BLUETOOTH_H
#define __BT_BLUETOOTH_H

#include <stdio.h>
#include <string.h>

#include <bluetooth/buf.h>
#include <bluetooth/conn.h>
#include <bluetooth/hci.h>

/** @brief Initialize the Bluetooth Subsystem.
 *
 *  Initialize Bluetooth. Must be the called before anything else.
 *  Caller shall be either task or a fiber.
 *
 *  @return zero in success or error code otherwise
 */
int bt_init(void);

/* Advertising API */

struct bt_eir {
	uint8_t len;
	uint8_t type;
	uint8_t data[29];
} __packed;

/*! @brief Define a type allowing user to implement a function that can be used
 *  to get back active LE scan results.
 *
 *  A function of this type will be called back when user application triggers
 *  active LE scan. The caller will populate all needed parameters based on data
 *  coming from scan result.
 *  Such function can be set by user when LE active scan API is used.
 *
 *  @param addr  Advertiser LE address and type.
 *  @param rssi  Strength of advertiser signal.
 *  @param adv_type  Type of advertising response from advertiser.
 *  @param adv_data  Address of buffer containig advertiser data.
 *  @param len  Length of advertiser data contained in buffer.
 */
typedef void bt_le_scan_cb_t(const bt_addr_le_t *addr, int8_t rssi,
		             uint8_t adv_type, const uint8_t *adv_data,
		             uint8_t len);

/** @brief Start advertising
 *
 *  Set advertisement data,  scan response data, advertisement parameters
 *  and start advertising.
 *
 *  @param type advertising type
 *  @param ad data to be used in advertisement packets
 *  @param sd data to be used in scan response packets
 *
 *  @return zero in success or error code otherwise.
 */
int bt_start_advertising(uint8_t type, const struct bt_eir *ad,
			 const struct bt_eir *sd);

int bt_start_scanning(uint8_t scan_filter, bt_le_scan_cb_t cb);
int bt_stop_scanning(void);

/** @brief Setting LE connection to peer
 *
 *  Allows initiate new LE link to remote peer using its address
 *
 *  @param peer address of peer's object containing LE address
 *
 *  @return zero in success or error code otherwise.
 */
int bt_connect_le(const bt_addr_le_t *peer);
int bt_disconnect(struct bt_conn *conn, uint8_t reason);

/*! @def BT_ADDR_STR_LEN
 *
 *  @brief Recommended length of user string buffer for bluetooth address
 *
 *  @details The recommended length guarantee the output of address conversion
 *  will not lose valuable information about address being processed.
 */
#define BT_ADDR_STR_LEN 18

/*! @def BT_ADDR_LE_STR_LEN
 *
 *  @brief Recommended length of user string buffer for bluetooth LE address
 *
 *  @details The recommended length guarantee the output of address conversion
 *  will not lose valuable information about address being processed.
 */
#define BT_ADDR_LE_STR_LEN 27

/*! @brief Converts binary bluetooth address to string.
 *
 *  @param addr Address of buffer containing binary bluetooth address.
 *  @param str Address of user buffer with enough room to store formatted
 *  string containing binary address.
 *  @param len Length of data to be copied to user string buffer. Refer to
 *  BT_ADDR_STR_LEN about recommended value.
 *
 *  @return Number of successfully formatted bytes from binary address.
 */
static inline int bt_addr_to_str(const bt_addr_t *addr, char *str, size_t len)
{
	return snprintf(str, len, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
			addr->val[5], addr->val[4], addr->val[3],
			addr->val[2], addr->val[1], addr->val[0]);
}

/*! @brief Converts binary LE bluetooth address to string.
 *
 *  @param addr Address of buffer containing binary LE bluetooth address.
 *  @param user_buf Address of user buffer with enough room to store formatted
 *  string containing binary LE address.
 *  @param len Length of data to be copied to user string buffer. Refer to
 *  BT_ADDR_LE_STR_LEN about recommended value.
 *
 *  @return Number of successfully formatted bytes from binary address.
 */
static inline int bt_addr_le_to_str(const bt_addr_le_t *addr, char *str,
				    size_t len)
{
	char type[7];

	switch (addr->type) {
	case BT_ADDR_LE_PUBLIC:
		strcpy(type, "public");
		break;
	case BT_ADDR_LE_RANDOM:
		strcpy(type, "random");
		break;
	default:
		sprintf(type, "0x%02x", addr->type);
		break;
	}

	return snprintf(str, len, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X (%s)",
			addr->val[5], addr->val[4], addr->val[3],
			addr->val[2], addr->val[1], addr->val[0], type);
}
#endif /* __BT_BLUETOOTH_H */
