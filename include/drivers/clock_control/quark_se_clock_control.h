/* quark_se_clock_control.h - Clock controller header for Quark SE */

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

#ifndef __QUARK_SE_CLOCK_CONTROL_H__
#define __QUARK_SE_CLOCK_CONTROL_H__

struct quark_se_clock_control_config {
	uint32_t base_address;
};

enum quark_se_peripheral_clocks {
	QUARK_SE_PERIPH_PCLK_EN = 0,
	QUARK_SE_CCU_PERIPH_PCLK_EN,
	QUARK_SE_CCU_I2C_M0_PCLK_EN,
	QUARK_SE_CCU_I2C_M1_PCLK_EN,
	QUARK_SE_CCU_SPI_S_PCLK_EN,
	QUARK_SE_CCU_SPI_M0_PCLK_EN,
	QUARK_SE_CCU_SPI_M1_PCLK_EN,
	QUARK_SE_CCU_GPIO_INTR_PCLK_EN,
	QUARK_SE_CCU_PERIPH_GPIO_DB_PCLK_EN,
	QUARK_SE_CCU_I2S_PCLK_EN,
	QUARK_SE_CCU_WDT_PCLK_EN_SW,
	QUARK_SE_CCU_RTC_PCLK_EN_SW,
	QUARK_SE_CCU_PWM_PCLK_EN_SW,
	QUARK_SE_CCU_GPIO_PCLK_EN_SW,
	QUARK_SE_CCU_SPI_M0_PCLK_EN_SW,
	QUARK_SE_CCU_SPI_M1_PCLK_EN_SW,
	QUARK_SE_CCU_SPI_PCLK_EN_SW,
	QUARK_SE_CCU_UARTA_PCLK_EN_SW,
	QUARK_SE_CCU_UARTB_PCLK_EN_SW,
	QUARK_SE_CCU_I2C_M0_PCLK_EN_SW,
	QUARK_SE_CCU_I2C_M1_PCLK_EN_SW,
	QUARK_SE_CCU_I2S_PCLK_EN_SW,
};

enum quark_se_external_clocks {
	QUARK_SE_CCU_EXT_RTC_EN = 0,
	QUARK_SE_CCU_EXT_CLK_EN,
	QUARK_SE_CCU_EXT_CLK_DIV_EN,
};

enum quark_se_sensor_clocks {
	QUARK_SE_CCU_SENSOR_CLK_EN = 0,
	QUARK_SE_CCU_SS_I2C_M0_CLK_EN,
	QUARK_SE_CCU_SS_I2C_M1_CLK_EN,
	QUARK_SE_CCU_SS_SPI_M0_CLK_EN,
	QUARK_SE_CCU_SS_SPI_M1_CLK_EN,
	QUARK_SE_CCU_SS_GPIO_INTR_CLK_EN,
	QUARK_SE_CCU_SS_GPIO_DB_CLK_EN,
	QUARK_SE_CCU_ADC_CLK_EN,
};

extern int quark_se_clock_control_init(struct device *dev);

#endif /* __QUARK_SE_CLOCK_CONTROL_H__ */
