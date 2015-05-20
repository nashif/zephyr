/* system.c - system/hardware module for the Quark BSP */

/*
 * Copyright (c) 2013-2015, Wind River Systems, Inc.
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
This module provides routines to initialize and support board-level hardware
for the Quark BSP.

Implementation Remarks:
Handlers for the secondary serial port have not been added.
*/

#include <cputype.h>
#include <nanokernel.h>
#include <nanokernel/cpu.h>
#include <misc/printk.h>
#include <misc/__assert.h>
#include "board.h"
#include <drivers/uart.h>
#include <drivers/ioapic.h>
#include <drivers/loapic.h>
#include <pci/pci.h>
#include <pci/pci_mgr.h>

#if defined(CONFIG_PRINTK) || defined(CONFIG_STDOUT_CONSOLE)
#define DO_CONSOLE_INIT
#endif



/*******************************************************************************
 *
 * _SysPciMap - maps PCI memory region
 *
 * This routine is defined in the BSP as the memory layout of the board is
 * board specific. However, the prototype is located in pci.h.
 *
 * RETURNS: virtual address
 *
 */

uint32_t _SysPciMap(uint32_t addr, uint32_t size)
{
	ARG_UNUSED(size);
	return addr;
}

#if defined(DO_CONSOLE_INIT)

/*******************************************************************************
*
* uartGenericInfoInit - initialize initialization information for one UART
*
* RETURNS: N/A
*
*/

static void uartGenericInfoInit(struct uart_init_info *p_info)
{
	p_info->options = 0;
	p_info->sys_clk_freq = UART_XTAL_FREQ;
	p_info->baud_rate = CONFIG_UART_BAUDRATE;
}

#endif /* DO_CONSOLE_INIT */

#if defined(DO_CONSOLE_INIT)

/*******************************************************************************
*
* consoleInit - initialize target-only console
*
* Only used for debugging.
*
* RETURNS: N/A
*
*/

#include <console/uart_console.h>

static void consoleInit(void)
{
	struct pci_dev_info dev_info = {
		.class = PCI_CLASS_COMM_CTLR,
		.vendor_id = 0x8086,
		.device_id = 0x0936,
	};
	struct uart_init_info info;
	int i;

	uartGenericInfoInit(&info);

	pci_bus_scan_init();

	i = 0;
	while (pci_bus_scan(&dev_info) && i < CONFIG_UART_CONSOLE_PCI_IDX) {
		i++;
	}

	info.regs = _SysPciMap(dev_info.addr, dev_info.size);
	info.irq = dev_info.irq;

	uart_init(CONFIG_UART_CONSOLE_INDEX, &info);
	uart_console_init();

#ifdef PCI_DEBUG
	pci_show(&dev_info);
#endif /* PCI_DEBUG */
}

#else
#define consoleInit()     \
	do {/* nothing */ \
	} while ((0))
#endif /* DO_CONSOLE_INIT */

/*******************************************************************************
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

	_ioapic_irq_set(HPET_TIMER0_IRQ, HPET_TIMER0_VEC, HPET_IOAPIC_FLAGS);

	consoleInit(); /* NOP if not needed */

#ifdef PCI_DEBUG
	pci_show();
#endif		       /* PCI_DEBUG */
}
