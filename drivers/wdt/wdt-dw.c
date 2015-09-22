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
#include <arch/cpu.h>
#include "wdt-dw.h"

void (*cb_fn)(void);

/**
 * Enables the clock for the peripheral watchdog
 */
static void wdt_dw_enable(void)
{
	SCSS_PERIPHERAL->periph_cfg0 |= SCSS_PERIPH_CFG0_WDT_ENABLE;
}


static void wdt_dw_disable(void)
{
	/* Disable the clock for the peripheral watchdog */
	SCSS_PERIPHERAL->periph_cfg0 &= ~SCSS_PERIPH_CFG0_WDT_ENABLE;
}


void wdt_dw_isr(void)
{
	if (cb_fn)
	{
		(*cb_fn)();
	}
}


static void wdt_dw_get_config(struct wdt_config *config)
{
}

IRQ_CONNECT_STATIC(wdt_dw, INT_WDT_IRQ, 0, wdt_dw_isr, 0);

static void wdt_dw_reload(void) {
	WDT_DW->wdt_crr = WDT_CRR_VAL;
}

static int wdt_dw_set_config(struct wdt_config *config)
{
	int ret = 0;

	wdt_dw_enable();
	/*  Set timeout value
	 *  [7:4] TOP_INIT - the initial timeout value is hardcoded in silicon,
	 *  only bits [3:0] TOP are relevant.
	 *  Once tickled TOP is loaded at the next expiration.
	 */
	uint32_t i;
	uint32_t ref = (1 << 16) / (CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / 1000000); /* 2^16/FREQ_CPU */
	uint32_t timeout = config->timeout * 1000;
	for(i = 0; i < 16; i++){
		if (timeout <= ref) break;
		ref = ref << 1;
	}
	if (i > 15){
		ret = -1;
		i = 15;
	}
	WDT_DW->wdt_torr = i;

	/* Set response mode */
	if (WDT_MODE_RESET == config->mode) {
		WDT_DW->wdt_cr &= ~WDT_CR_INT_ENABLE;
	} else {
		if (config->interrupt_fn)
		{
			cb_fn = config->interrupt_fn;
		} else {
			return -1;
		}

		WDT_DW->wdt_cr |= WDT_CR_INT_ENABLE;

		IRQ_CONFIG(wdt_dw, INT_WDT_IRQ);
		irq_enable(INT_WDT_IRQ);

		/* unmask WDT interrupts to lmt  */
		SCSS_INTERRUPT->int_watchdog_mask &= INT_UNMASK_IA;
	}

	/* Enable WDT, cannot be disabled until soc reset */
	WDT_DW->wdt_cr |= WDT_CR_ENABLE;

	wdt_dw_reload();
	return ret;
}


#if 0

static uint32_t wdt_dw_read_counter(void)
{
	return WDT_DW->wdt_ccvr;
}

static uint32_t wdt_dw_timeout(void)
{
	return WDT_DW->wdt_torr;
}

#endif

static struct wdt_driver_api wdt_dw_funcs = {
        .set_config = wdt_dw_set_config,
        .get_config = wdt_dw_get_config,
        .enable = wdt_dw_enable,
        .disable = wdt_dw_disable,
        .reload = wdt_dw_reload,
};

int wdt_dw_init(struct device *dev)
{
	dev->driver_api = &wdt_dw_funcs;
	return 0;
}

