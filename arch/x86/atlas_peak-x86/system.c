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

#if defined(CONFIG_PRINTK) || defined(CONFIG_STDOUT_CONSOLE)
  #define DO_CONSOLE_INIT
#endif

#if defined(DO_CONSOLE_INIT)

/**************************************************************************
*
* uart_info_init - initialize initialization information for one UART
*
* RETURNS: N/A
*
*/

static void uart_info_init(struct uart_init_info *p_info)
{
	p_info->options = 0;
	p_info->sys_clk_freq = UART_XTAL_FREQ;
	p_info->baud_rate = CONFIG_UART_BAUDRATE;
}

#endif /* DO_CONSOLE_INIT */


#if defined(DO_CONSOLE_INIT)

/**************************************************************************
*
* consoleInit - initialize target-only console
*
* Only used for debugging, no host driver involved.
*
* RETURNS: N/A
*
*/

#include <console/uart_console.h>

static void consoleInit(void)
{
	struct uart_init_info info;

	uart_info_init(&info);

	/*
	 * Need type casting to avoid compiler warnings about assigning a
	 * pointer to a smaller integer. We know the size is right...
	 */
	info.int_pri = CONFIG_UART_CONSOLE_INT_PRI;
	uart_init(CONFIG_UART_CONSOLE_INDEX, &info);

	uart_console_init();
}

#else
#define consoleInit() do { /* nothing */ } while ((0))
#endif /* DO_CONSOLE_INIT */




#ifdef CONFIG_BSP_ATLAS_PEAK_X86_FPGA
/**************************************************************************
 * _rom_flash_delay - delay hardware initialization
 *
 * Add a delay to address the delay between the ROM code and the FLASH code
 *
 * RETURNS: N/A
 */
static inline void _rom_flash_delay(void)
{
	int x = 255;

	do {
		*((uint32_t *)(LOAPIC_BASE_ADRS + LOAPIC_EOI)) = 0;
		x--;
	} while (x > 0);
}
#endif /* CONFIG_BSP_ATLAS_PEAK_X86_FPGA */


/**************************************************************************
*
* _InitHardware - perform basic hardware initialization
*
* Initialize the Intel LOAPIC and IOAPIC device driver and the
* Intel 8250 UART device driver.
* Also initialize the timer device driver, if required.
*
* RETURNS: N/A
*/

void _InitHardware(void)
{
	_loapic_init();
	_ioapic_init();

	consoleInit(); /* NOP if not needed */
#ifdef CONFIG_BSP_ATLAS_PEAK_X86_FPGA
	_rom_flash_delay();
#endif
}
