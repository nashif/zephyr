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

#include <nanokernel.h>
#include <device.h>
#include <rtc.h>
#include <rtc/qmsi_rtc.h>

#include "board.h"
#include "qm_rtc.h"

IRQ_CONNECT_STATIC(rtc, INT_RTC_IRQ, CONFIG_RTC_IRQ_PRI, qm_rtc_isr_0, 0);

static void enable(void)
{
	qm_clk_periph_enable(QM_CCU_RTC_PCLK_EN_SW | QM_CCU_PERIPH_CLK_EN);
}

static void disable(void)
{
	qm_clk_periph_disable(CCU_RTC_PCLK_EN_SW);
}

static int set_config(struct rtc_config *cfg)
{
	qm_rtc_config_t qm_cfg;

	qm_cfg.init_val = cfg->init_val;
	qm_cfg.alarm_en = cfg->alarm_enable;
	qm_cfg.alarm_val = cfg->alarm_val;
	qm_cfg.cb_fn = cfg->cb_fn;

	if (qm_rtc_set_config(QM_RTC_0, &qm_cfg) != QM_RC_OK)
		return DEV_FAIL;

	/* QMSI API 'qm_rtc_set_config' doesn't wait CCVR being updated. So, we
	 * should wait CCVR is update before returning.
	 */
	while (QM_RTC[QM_RTC_0].rtc_ccvr != cfg->init_val) {
	}

	return DEV_OK;
}

static int set_alarm(const uint32_t alarm_val)
{
	return qm_rtc_set_alarm(QM_RTC_0, alarm_val) == QM_RC_OK ? DEV_OK : DEV_FAIL;
}

static uint32_t read(void)
{
	return QM_RTC[QM_RTC_0].rtc_ccvr;
}

static struct rtc_driver_api api = {
	.enable = enable,
	.disable = disable,
	.read = read,
	.set_config = set_config,
	.set_alarm = set_alarm,
};

int qmsi_rtc_init(struct device *dev)
{
	IRQ_CONFIG(rtc, INT_RTC_IRQ, 0);

	/* Unmask RTC interrupt */
	irq_enable(INT_RTC_IRQ);

	/* Route RTC interrupt to Lakemont */
	QM_SCSS_INT->int_rtc_mask &= ~BIT(0);

	dev->driver_api = &api;
	return DEV_OK;
}
