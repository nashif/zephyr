/* platform_config.c - Quark SE Configuration. */

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

#if CONFIG_IPI_QUARK_SE
#include <ipi.h>
#include <ipi/ipi_quark_se.h>

static int arc_quark_se_ipi_init(void) {
	irq_connect(QUARK_SE_IPI_INTERRUPT, QUARK_SE_IPI_INTERRUPT_PRI, quark_se_ipi_isr,
		    NULL);
	irq_enable(QUARK_SE_IPI_INTERRUPT);
	return DEV_OK;
}

static struct quark_se_ipi_controller_config_info ipi_controller_config = {
	.controller_init = arc_quark_se_ipi_init
};
DECLARE_DEVICE_INIT_CONFIG(quark_se_ipi, "", quark_se_ipi_controller_initialize,
			   &ipi_controller_config);
pre_kernel_late_init(quark_se_ipi, NULL);

#if CONFIG_IPI_CONSOLE_SENDER
#include <console/ipi_console.h>
QUARK_SE_IPI_DEFINE(quark_se_ipi4, 4, QUARK_SE_IPI_OUTBOUND);

struct ipi_console_sender_config_info quark_se_ipi_sender_config = {
	.bind_to = "quark_se_ipi4",
	.flags = IPI_CONSOLE_PRINTK | IPI_CONSOLE_STDOUT,
};
DECLARE_DEVICE_INIT_CONFIG(ipi_console, "ipi_console",
			   ipi_console_sender_init,
			   &quark_se_ipi_sender_config);
nano_early_init(ipi_console, NULL);

#endif /* CONFIG_IPI_CONSOLE_SENDER */
#endif /* CONFIG_IPI_QUARK_SE */
