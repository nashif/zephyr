/*
 * Copyright (c) 2015 Wind River Systems, Inc.
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

/**
 * @file
 * @brief System/hardware module for the Quark SE BSP
 *
 * This module provides routines to initialize and support board-level
 * hardware for the Quark SE BSP.
 */

#include <nanokernel.h>
#include <misc/printk.h>
#include <misc/__assert.h>
#include "soc.h"
#include <uart.h>
#include <init.h>
#include "shared_mem.h"

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
#ifndef CONFIG_ARC_INIT_DEBUG
	/* Start the CPU */
	SCSS_REG_VAL(SCSS_SS_CFG) |= ARC_RUN_REQ_A;

	arc_init_debug("Waiting for arc to start...\n");
	/* Block until the ARC core actually starts up */
	while (SCSS_REG_VAL(SCSS_SS_STS) & 0x4000) {
	}

	/* Block until ARC's quark_se_init() sets a flag indicating it is ready,
	 * if we get stuck here ARC has run but has exploded very early */
	arc_init_debug("Waiting for arc to init...\n");
	while (!shared_data->flags & ARC_READY) {
	}
#endif

	return DEV_OK;
}

SYS_INIT(arc_init, SECONDARY, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

#endif /*CONFIG_ARC_INIT*/

/**
 * TODO - TH - RTOS-1084: Update Quark SE 2 soc header files for Quark SE 2 SCU.
 *
 * SCU replaces SCSS.
 *
 * To Replace Silicon bringup function ATP2 bringup functions soc_evt_router_unmask
 * & soc_init with standard zephyr Quark SE config functions
 * platform_uart_init / uart_ns16550_init.
 */
#define SOC_EVENT_ROUTER_BASE            (0xB0805000)
#define SOC_UART0_INT_MASK_OFF           (0x00e4)
#define SOC_UART1_INT_MASK_OFF           (0x0158)

static __inline void soc_evt_router_unmask(const uint32_t offset)
{
    uint32_t  *mask = (uint32_t *)(SOC_EVENT_ROUTER_BASE + offset);
    *mask = (*mask) & ~0x1;
}

static int soc_init(struct device *arg)
{
    ARG_UNUSED(arg);

    /* Unmask Uart irqs for ATP2 bring-up. */
#ifdef CONFIG_UART_NS16550_PORT_0
    soc_evt_router_unmask(SOC_UART0_INT_MASK_OFF);
#endif /* CONFIG_UART_NS16550_PORT_0 */

#ifdef CONFIG_UART_NS16550_PORT_1
    soc_evt_router_unmask(SOC_UART1_INT_MASK_OFF);
#endif /* CONFIG_UART_NS16550_PORT_1 */

    return DEV_OK;
}

SYS_INIT(soc_init, PRIMARY, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
