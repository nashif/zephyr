/* dw_spi.h - Designeware SPI driver utilities */

/*
 * Copyright (c) 2015 Intel Corporation.
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
