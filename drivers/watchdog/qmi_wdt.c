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
