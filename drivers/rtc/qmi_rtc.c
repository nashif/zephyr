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
