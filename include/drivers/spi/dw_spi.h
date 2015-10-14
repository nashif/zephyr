/* dw_spi.h - Designeware SPI driver utilities */

/*
 * Copyright (c) 2015 Intel Corporation.
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

#ifndef __DW_SPI_H__
#define __DW_SPI_H__

#include <spi.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void spi_dw_isr(void *data);

int spi_dw_init(struct device *dev);
typedef void (*spi_dw_config_t)(struct device *dev);

struct spi_dw_config {
	uint32_t regs;
	uint32_t irq;
	uint32_t int_mask;
#ifdef CONFIG_SPI_DW_CLOCK_GATE
	struct device *clock;
	void *clock_data;
#endif /* CONFIG_SPI_DW_CLOCK_GATE */
	spi_dw_config_t config_func;
};

struct spi_dw_data {
	uint32_t slave;
	spi_callback callback;
	uint8_t *tx_buf;
	uint32_t tx_buf_len;
	uint8_t *rx_buf;
	uint32_t rx_buf_len;
	uint32_t t_len;
};

#ifdef __cplusplus
}
#endif

#endif /* __DW_SPI_H__ */
