/* system.c - system/hardware module for the Atlas Peak BSP */

/*
 * Copyright (c) 2015 Wind River Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2) Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3) Neither the name of Wind River Systems nor the names of its contributors
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

/*
DESCRIPTION
This module provides routines to initialize and support board-level
hardware for the Atlas Peak BSP.
*/

#include <nanokernel.h>
#include <misc/printk.h>
#include <misc/__assert.h>
#include "board.h"
#include <drivers/uart.h>
#include <drivers/ioapic.h>
#include <drivers/loapic.h>
#include <init.h>
#include <shared_mem.h>

#ifdef CONFIG_ARC_INIT
#define SCSS_REG_VAL(offset) \
	(*((volatile uint32_t *)(SCSS_REGISTER_BASE+offset)))

#ifdef CONFIG_ARC_INIT_DEBUG
#define arc_init_debug	printk
#else
#define arc_init_debug(x...) do { } while (0)
#endif

/**
 *
 * @brief ARC Init
 *
 * This routine initialize the ARC reset vector and
 * starts the ARC processor.
 * @return N/A
 */
static int arc_init(struct device *arg)
{
	uint32_t *reset_vector;

	ARG_UNUSED(arg);

	if (!SCSS_REG_VAL(SCSS_SS_STS)) {
		/* ARC shouldn't already be running! */
		printk("ARC core already running!");
		return DEV_FAIL;
	}

	/* Address of ARC side __reset stored in the first 4 bytes of arc.bin,
	 * we read the value and stick it in shared_mem->arc_start which is
	 * the beginning of the address space at 0xA8000000 */
	reset_vector = (uint32_t *)RESET_VECTOR;
	arc_init_debug("Reset vector address: %x\n", *reset_vector);
	shared_data->arc_start = *reset_vector;
	shared_data->flags = 0;
	/* Start the CPU */
	SCSS_REG_VAL(SCSS_SS_CFG) |= ARC_RUN_REQ_A;

	arc_init_debug("Waiting for arc to start...\n");
	/* Block until the ARC core actually starts up */
	while (SCSS_REG_VAL(SCSS_SS_STS) & 0x4000) { }

	/* Block until ARC's atp_init() sets a flag indicating it is ready,
	 * if we get stuck here ARC has run but has exploded very early */
	arc_init_debug("Waiting for arc to init...\n");
	while (!shared_data->flags & ARC_READY) { }

	return DEV_OK;
}

DECLARE_DEVICE_INIT_CONFIG(atp_ss_0, "", arc_init, NULL);
pre_kernel_late_init(atp_ss_0, NULL);

#endif /*CONFIG_ARC_INIT*/

#ifdef CONFIG_IOAPIC
DECLARE_DEVICE_INIT_CONFIG(ioapic_0, "", _ioapic_init, NULL);
pre_kernel_early_init(ioapic_0, NULL);

#endif /* CONFIG_IOAPIC */

#ifdef CONFIG_LOAPIC
DECLARE_DEVICE_INIT_CONFIG(loapic_0, "", _loapic_init, NULL);
pre_kernel_early_init(loapic_0, NULL);

#endif /* CONFIG_LOAPIC */

