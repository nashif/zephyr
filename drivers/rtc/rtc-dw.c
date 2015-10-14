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
#include <arch/cpu.h>
#include "board.h"
#include <stdbool.h>
#include <device.h>
#include <rtc.h>
#include <stdio.h>
#include <init.h>

#include "rtc-dw.h"

#define UPDATE_DELAY 4

static void (*rtc_dw_cb_fn)(void);

static void rtc_dw_clock_frequency(uint32_t frequency)
{
	SCSS_CCU->ccu_sys_clk_ctl &= ~RTC_CLK_DIV_EN;
	SCSS_CCU->ccu_sys_clk_ctl &= ~RTC_CLK_DIV_MASK;
	SCSS_CCU->ccu_sys_clk_ctl |= frequency;
	SCSS_CCU->ccu_sys_clk_ctl |= RTC_CLK_DIV_EN;

}

/**
 *  @brief   Function to enable clock gating for the RTC
 *  @return  N/A
 */
static void rtc_dw_enable(void)
{
	SCSS_CCU->ccu_periph_clk_gate_ctl |= CCU_RTC_PCLK_EN_SW;
}

/**
 *  @brief   Function to disable clock gating for the RTC
 *  @return  N/A
 */
static void rtc_dw_disable(void)
{
	SCSS_CCU->ccu_periph_clk_gate_ctl &= ~CCU_RTC_PCLK_EN_SW;
}

static void rtc_dw_clock_disable(void)
{
	RTC_DW->rtc_ccr &= ~RTC_CLK_DIV_EN;
}

/**
 *  @brief   RTC alarm ISR, if specified calls a user defined callback
 *  @return  N/A
 */
void rtc_dw_isr(void)
{
	/* clear interrupt */
	RTC_DW->rtc_eoi;

	if (rtc_dw_cb_fn)
	{
		(*rtc_dw_cb_fn)();
	}
}


/**
 *  @brief   Function to configure the RTC
 *  @param   config  pointer to a RTC configuration structure
 *  @return  0 on success
 */
static int rtc_dw_set_config(rtc_config_t *config)
{

	/*  Set RTC divider - 32.768khz / 32768 = 1 second.
	 *   Note: Divider not implemented in standard emulation image.
	 */
	rtc_dw_clock_frequency(RTC_CLK_DIV_1_HZ);

	RTC_DW->rtc_ccr |= RTC_INTERRUPT_MASK;

	/* set intial RTC value */
	RTC_DW->rtc_clr = config->init_val;

	/* wait UPDATE_DELAY second for ther rtc value to be written */
	while ((RTC_DW->rtc_clr + UPDATE_DELAY) != RTC_DW->rtc_ccvr) {
	}

	RTC_DW->rtc_ccr &= ~RTC_INTERRUPT_MASK;

	return 0;
}

IRQ_CONNECT_STATIC(rtc, INT_RTC_IRQ, 0, rtc_dw_isr, 0);

/**
 * @brief Read current RTC value
 * @return current rtc value
 */
static uint32_t rtc_dw_read(void)
{
	return RTC_DW->rtc_ccvr;
}

/**
 * @brief Sets an RTC alarm
 * @param alarm Alarm configuration
 * @return 0 on success
 */
static int rtc_dw_set_alarm(rtc_alarm_t *alarm)
{
	RTC_DW->rtc_ccr &= ~RTC_INTERRUPT_ENABLE;

	if (alarm->alarm_enable == 1) {
		if (alarm->cb_fn)
		{
			rtc_dw_cb_fn = alarm->cb_fn;
		}
		RTC_DW->rtc_eoi;
		RTC_DW->rtc_cmr = alarm->alarm_val;

		IRQ_CONFIG(rtc, INT_RTC_IRQ);
		irq_enable(INT_RTC_IRQ);

		/* unmask RTC interrupts to lmt  */
		SCSS_INTERRUPT->int_rtc_mask = INT_UNMASK_IA;

		RTC_DW->rtc_ccr |= RTC_INTERRUPT_ENABLE;
		RTC_DW->rtc_ccr &= ~RTC_INTERRUPT_MASK;
	} else {
		SCSS_INTERRUPT->int_rtc_mask = ~(0);
	}

	uint32_t t =  rtc_dw_read();
	while ((t + UPDATE_DELAY) !=  RTC_DW->rtc_ccvr) {
	}
	return 0;
}

#if 0
static void rtc_dw_clk_disable(void)
{
	RTC_DW->rtc_ccr  &= ~RTC_ENABLE;
}
#endif


static struct rtc_driver_api funcs = {
	.set_config = rtc_dw_set_config,
	.read = rtc_dw_read,
	.enable = rtc_dw_enable,
	.disable = rtc_dw_disable,
	.clock_disable = rtc_dw_clock_disable,
	.set_alarm = rtc_dw_set_alarm,
};

int rtc_dw_init(struct device* dev) {

	dev->driver_api = &funcs;

	const uint32_t expected_freq = RTC_CLK_DIV_1_HZ | RTC_CLK_DIV_EN;
	uint32_t curr_freq = SCSS_CCU->ccu_sys_clk_ctl & (RTC_CLK_DIV_EN | RTC_CLK_DIV_MASK);

	// disable interrupt
	RTC_DW->rtc_ccr &= ~RTC_INTERRUPT_ENABLE;
	RTC_DW->rtc_eoi;

	/* Reset initial value only if RTC wasn't enabled at right frequency at
	 * beginning of init
	 */
	if (expected_freq != curr_freq) {
		//  Set RTC divider 4096HZ for fast uptade
		rtc_dw_clock_frequency(RTC_CLK_DIV_4096_HZ);

		/* set intial RTC value 0 */
		RTC_DW->rtc_clr = 0;
		while (0 != RTC_DW->rtc_ccvr) {
			RTC_DW->rtc_clr = 0;
		}
	}
	//  Set RTC divider 1HZ
	rtc_dw_clock_frequency(RTC_CLK_DIV_1_HZ);
	return 0;
}

struct rtc_dw_dev_config rtc_dev = {
        .base_address = RTC_BASE_ADDR,
};

#ifdef CONFIG_RTC_DW
#include <init.h>
DECLARE_DEVICE_INIT_CONFIG(rtc,
                           RTC_DRV_NAME,
                           &rtc_dw_init,
                           &rtc_dev);

micro_early_init(rtc, NULL);
#endif
