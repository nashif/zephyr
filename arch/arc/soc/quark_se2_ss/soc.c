/* system.c - system/hardware module for quark_se_ss BSP */

/*
 * Copyright (c) 2014-2015 Wind River Systems, Inc.
 * Copyright (c) 2016 Intel Corporation
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
 * This module provides routines to initialize and support board-level hardware
 * for the Quark SE platform.
 */

#include <nanokernel.h>
#include "soc.h"
#include <init.h>
#include <quark_se/shared_mem.h>

/* Cannot use microkernel, since only nanokernel is supported */
#if defined(CONFIG_MICROKERNEL)
#error "Microkernel support is not available"
#endif

#if (CONFIG_HW_EXPECT_IVT_RAM_ADDR < (CONFIG_SRAM_BASE_ADDRESS + (CONFIG_SRAM_SIZE*1024)))
#error CONFIG_SRAM_SIZE invalid and overwriting HW expected IVT RAM address.
#endif

extern uint32_t _VectorTable[];

/**
 * @brief Copy ARC IVT from FLASH to RAM
 */
static void copy_ivt_to_ram(void)
{
	volatile uint32_t *flash_table = (uint32_t *)_VectorTable;
	volatile uint32_t *ram_table = (uint32_t *)CONFIG_HW_EXPECT_IVT_RAM_ADDR;
	unsigned int n = 0;

	/**
	 * Copy the flash persisted vector table for the 16 processor internal
	 * interrupts to the RAM address expected by the HW.
	 */
	for (n = 0; n < 16; n++) {
		ram_table[n] = flash_table[n];
	}
}
/**
 *
 * @brief perform basic hardware initialization
 *
 * Hardware initialized:
 * - interrupt unit
 *
 * RETURNS: N/A
 */
static int quark_se_arc_init(struct device *arg)
{
	ARG_UNUSED(arg);
	copy_ivt_to_ram();
	_arc_v2_irq_unit_init();
	shared_data->flags |= ARC_READY;
	return 0;
}

SYS_INIT(quark_se_arc_init, PRIMARY, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
