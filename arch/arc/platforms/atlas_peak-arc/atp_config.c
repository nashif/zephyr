/* atp_config.c - AtP Configuration. */

/*
 * Copyright (c) 2015 Intel Corporation
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

#include <device.h>
#include <init.h>
#include "board.h"

#ifdef CONFIG_DW_ADC

#include <adc.h>
#include <dw_adc.h>

struct adc_info adc_info_dev =
	{
		.rx_len = 0,
		.seq_size = 1,
		.state = ADC_STATE_IDLE
	};

struct adc_config adc_config_dev =
	{
		.reg_base = PERIPH_ADDR_BASE_ADC,
		.reg_irq_mask = SCSS_REGISTER_BASE + INT_SS_ADC_IRQ_MASK,
		.reg_err_mask = SCSS_REGISTER_BASE + INT_SS_ADC_ERR_MASK,
		.rx_vector = IO_ADC0_INT_IRQ,
		.err_vector = IO_ADC0_INT_ERR,
		.fifo_tld = IO_ADC0_FS/2,
		.in_mode      = CONFIG_ADC_INPUT_MODE,
		.out_mode     = CONFIG_ADC_OUTPUT_MODE,
		.capture_mode = CONFIG_ADC_CAPTURE_MODE,
		.seq_mode     = CONFIG_ADC_SEQ_MODE,
		.sample_width = CONFIG_ADC_WIDTH,
		.clock_ratio  = CONFIG_ADC_CLOCK_RATIO,
		.serial_dly   = CONFIG_ADC_SERIAL_DELAY
	};

DECLARE_DEVICE_INIT_CONFIG(adc,		/* config name*/
			ADC_DRV_NAME,	/* driver name*/
			&dw_adc_init,	/* init function*/
			&adc_config_dev); /* config options*/

pre_kernel_late_init(adc, &adc_info_dev);

#endif /* CONFIG_DW_ADC */

#if CONFIG_IPI_ATP
#include <ipi.h>
#include <ipi/ipi_atp.h>

static int arc_atp_ipi_init(void) {
	irq_connect(ATP_IPI_INTERRUPT, ATP_IPI_INTERRUPT_PRI, atp_ipi_isr,
		    NULL);
	irq_enable(ATP_IPI_INTERRUPT);
	return DEV_OK;
}

static struct atp_ipi_controller_config_info ipi_controller_config = {
	.controller_init = arc_atp_ipi_init
};
DECLARE_DEVICE_INIT_CONFIG(atp_ipi, "", atp_ipi_controller_initialize,
			   &ipi_controller_config);
pre_kernel_late_init(atp_ipi, NULL);

#if CONFIG_IPI_CONSOLE_SENDER
#include <console/ipi_console.h>
ATP_IPI_DEFINE(atp_ipi4, 4, ATP_IPI_OUTBOUND);

struct ipi_console_sender_config_info atp_ipi_sender_config = {
	.bind_to = "atp_ipi4",
	.flags = IPI_CONSOLE_PRINTK | IPI_CONSOLE_STDOUT,
};
DECLARE_DEVICE_INIT_CONFIG(ipi_console, "ipi_console",
			   ipi_console_sender_init,
			   &atp_ipi_sender_config);
nano_early_init(ipi_console, NULL);

#endif /* CONFIG_IPI_CONSOLE_SENDER */
#endif /* CONFIG_IPI_ATP */
