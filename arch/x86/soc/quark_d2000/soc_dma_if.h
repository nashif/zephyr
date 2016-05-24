/**
 * @file
 *
 * @brief Hardware interface encoding for D2000 SOC DMA controller.
 */

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

#ifndef __SOC_DMA_IF_H__
#define __SOC_DMA_IF_H__

/*
 * Hardware interface encoding used by the DMA controller on the SOC
 * when the source or destination of the DMA transfer is one of the recognized
 * peripherals
 *
 */

enum dma_handshake_interface {
	DMA_HW_IF_UART_A_TX = 0x0,       /** UART_A_TX */
	DMA_HW_IF_UART_A_RX = 0x1,       /** UART_A_RX */
	DMA_HW_IF_UART_B_TX = 0x2,       /** UART_B_TX*/
	DMA_HW_IF_UART_B_RX = 0x3,       /** UART_B_RX */
	DMA_HW_IF_SPI_MASTER_0_TX = 0x4, /** SPI_Master_0_TX */
	DMA_HW_IF_SPI_MASTER_0_RX = 0x5, /** SPI_Master_0_RX */
	DMA_HW_IF_SPI_SLAVE_TX = 0x8,    /** SPI_Slave_TX */
	DMA_HW_IF_SPI_SLAVE_RX = 0x9,    /** SPI_Slave_RX */
	DMA_HW_IF_I2C_MASTER_0_TX = 0xc, /** I2C_Master_0_TX */
	DMA_HW_IF_I2C_MASTER_0_RX = 0xd, /** I2C_Master_0_RX */
};

#endif /* __SOC_DMA_IF_H__ */
