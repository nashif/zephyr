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
#include <watchdog.h>

#include "board.h"
#include "qm_wdt.h"

#define CYCLES_PER_MS (CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC / 1000)

IRQ_CONNECT_STATIC(wdt, INT_WDT_IRQ, 0, qm_wdt_isr, 0);

static void get_config(struct wdt_config *cfg)
{
	qm_wdt_config_t qm_cfg = { 0 };
	uint32_t cycles;

	qm_wdt_get_config(&qm_cfg);

	/* Translate timeout from QMI format to milliseconds. */
	cycles = 1 << (qm_cfg.timeout + 16);
	cfg->timeout = cycles / CYCLES_PER_MS;

	cfg->mode = (qm_cfg.mode == QM_WDT_MODE_RESET) ?
			WDT_MODE_RESET : WDT_MODE_INTERRUPT_RESET;
	cfg->interrupt_fn = qm_cfg.cb_fn;
}

/* From the SoC perspective, the watchdog timeout is configured in 'clock
 * cycles' and it assumes very specific values. So set_config() may not
 * be able configure the watchdog timeout to exactly the same value from
 * cfg->timeout. It configures the watchdog timeout to the most close
 * upper-bound value. For instance, if cfg->timeout is 10 ms, the timeout
 * will be set to 16.384 ms.
 */
static int set_config(struct wdt_config *cfg)
{
	qm_wdt_config_t qm_cfg;
	uint32_t cycles;
	uint32_t ref = 1 << 16;
	int i;

	/* Translate cfg->timeout (in milliseconds) to QMI timeout format
	 * (qm_wdt_clock_timeout_cycles_t).
	 */
	cycles = cfg->timeout * CYCLES_PER_MS;
	for (i = 0; i < 16; i++) {
		if (cycles <= ref)
			break;
		ref = ref << 1;
	}
	/* If i > 15, it means cfg->timeout has a invalid value. So we fail. */
	if (i > 15)
		return -1;

	qm_cfg.timeout = i;
	qm_cfg.mode = (cfg->mode == WDT_MODE_RESET) ?
			QM_WDT_MODE_RESET : QM_WDT_MODE_INTERRUPT_RESET;
	qm_cfg.cb_fn = cfg->interrupt_fn;

	qm_wdt_set_config(&qm_cfg);
	return 0;
}

static void dummy(void) {}

static struct wdt_driver_api api = {
	.enable = qm_wdt_start,
	/* QMI has no disable API for watchdog so we set the disable callback
	 * to a dummy function.
	 */
	.disable = dummy,
	.get_config = get_config,
	.set_config = set_config,
	.reload = qm_wdt_reload,
};

int qmi_wdt_init(struct device *dev)
{
	IRQ_CONFIG(wdt, INT_WDT_IRQ);

	/* Unmask watchdog interrupt */
	irq_enable(INT_WDT_IRQ);

	/* Route watchdog interrupt to Lakemont */
	QM_SCSS_INT->int_watchdog_mask &= ~BIT(0);

	dev->driver_api = &api;
	return 0;
}
