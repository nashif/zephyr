/** @file
 *  @brief Simple UART driver header file.
 *
 *  A simple UART driver that allows applications to handle all aspects of
 *  received protocol data.
 */

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

#include <stdlib.h>

/** @brief Received data callback.
 *
 *  This function is called when new data is received on UART. The off parameter
 *  can be used to alter offset at which received data is stored. Typically,
 *  when the complete data is received and a new buffer is provided off should
 *  be set to 0.
 *
 *  @param buf Buffer with received data.
 *  @param off Data offset on next received and accumulated data length.
 *
 *  @return Buffer to be used on next receive.
 */
typedef uint8_t *(*uart_simple_recv_cb)(uint8_t *buf, size_t *off);

/** @brief Register UART application.
 *
 *  This function is used to register new UART application.
 *
 *  @param buf Initial buffer for received data.
 *  @param len Size of buffer.
 *  @param cb Callback to be called on data reception.
 */
void uart_simple_register(uint8_t *buf, size_t len, uart_simple_recv_cb cb);

/** @brief Send data over UART.
 *
 *  This function is used to send data over UART.
 *
 *  @param data Buffer with data to be send.
 *  @param len Size of data.
 *
 *  @return Number of bytes sent.
 */
int uart_simple_send(const uint8_t *data, int len);

/** @brief Simple UART interrupt handler.
 *
 *  This function is called from an interrupt and should not be called by
 *  an application directly.
 *
 *  @param unused unused
 */
void uart_simple_isr(void *unused);
