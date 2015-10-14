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

#include "board.h"
#include "qm_rtc.h"

IRQ_CONNECT_STATIC(rtc, INT_RTC_IRQ, 0, qm_rtc_isr, 0);

static void enable(void)
{
	qm_clk_periph_enable(CCU_RTC_PCLK_EN_SW);
}

static void disable(void)
{
	qm_clk_periph_disable(CCU_RTC_PCLK_EN_SW);
}

static int set_config(rtc_config_t *cfg)
{
	qm_rtc_config_t qm_cfg = { 0 };

	qm_cfg.init_val = cfg->init_val;
	qm_rtc_set_config(&qm_cfg);

	/* QMI API 'qm_rtc_set_config' doesn't wait CCVR being updated. So, we
	 * should wait CCVR is update before returning.
	 */
	while (QM_RTC->rtc_ccvr != cfg->init_val);

	return 0;
}

static int set_alarm(rtc_alarm_t *alarm_cfg)
{
	qm_rtc_config_t qm_cfg;

	qm_rtc_get_config(&qm_cfg);

	qm_cfg.alarm_en = alarm_cfg->alarm_enable;
	qm_cfg.alarm_val = alarm_cfg->alarm_val;
	qm_cfg.cb_fn = alarm_cfg->cb_fn;

	qm_rtc_set_config(&qm_cfg);

	return 0;
}

static uint32_t read(void)
{
	return QM_RTC->rtc_ccvr;
}

static struct rtc_driver_api api = {
	.enable = enable,
	.disable = disable,
	.clock_disable = disable,
	.read = read,
	.set_config = set_config,
	.set_alarm = set_alarm,
};

int qmi_rtc_init(struct device *dev)
{
	IRQ_CONFIG(rtc, INT_RTC_IRQ);

	/* Unmask RTC interrupt */
	irq_enable(INT_RTC_IRQ);

	/* Route RTC interrupt to Lakemont */
	QM_SCSS_INT->int_rtc_mask &= ~BIT(0);

	dev->driver_api = &api;
	return 0;
}
