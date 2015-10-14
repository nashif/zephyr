/* quark_se_clock_control.h - Clock controller header for Quark SE */

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
