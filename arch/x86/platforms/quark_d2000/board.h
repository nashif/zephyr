/* board.h - board configuration macros for the Quark D2000 BSP */

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

/*
DESCRIPTION
This header file is used to specify and describe board-level aspects for
the Quark D2000 BSP.
*/

#ifndef __INCboardh
#define __INCboardh

#include <stdint.h>
#include <misc/util.h>
#include <uart.h>
#include <drivers/ioapic.h>

#define INT_VEC_IRQ0  0x20 /* Vector number for IRQ0 */
#define FIXED_HARDWARE_IRQ_TO_VEC_MAPPING(x) (INT_VEC_IRQ0 + x)
#define IOAPIC_LO32_RTE_SUPPORTED_MASK (IOAPIC_INT_MASK | IOAPIC_TRIGGER_MASK)

/*
 * IO APIC (IOAPIC) device information (Intel ioapic)
 */
#define IOAPIC_BASE_ADRS_PHYS   0xFEC00000      /* base physical address */
#define IOAPIC_SIZE             MB(1)
#define IOAPIC_BASE_ADRS        IOAPIC_BASE_ADRS_PHYS

/*
 * Local APIC (LOAPIC) device information (Intel loapic)
 */

#define LOAPIC_BASE_ADRS_PHYS   0xFEE00000      /* base physical address */
#define LOAPIC_SIZE             KB(4)
#define LOAPIC_BASE_ADRS        LOAPIC_BASE_ADRS_PHYS
#define LOAPIC_TIMER_VEC	      (INT_VEC_IRQ0 + CONFIG_LOAPIC_TIMER_IRQ)
#define LOAPIC_TIMER_VEC_LIMIT	((INT_VEC_IRQ0 + 16) - 1)
#if ((LOAPIC_TIMER_VEC < INT_VEC_IRQ0) || (LOAPIC_TIMER_VEC > LOAPIC_TIMER_VEC_LIMIT))
#error CONFIG_LOAPIC_TIMER_IRQ out of valid range.
#endif
#define LOAPIC_IRQ_BASE			CONFIG_LOAPIC_TIMER_IRQ
#define LOAPIC_IRQ_COUNT		1
#define LOAPIC_LVT_REG_SPACING  0x10

/* serial port (aka COM port) information */
#define SYNOPSIS_UART_DLF_OFFSET		0xc0
#define SYNOPSIS_UART_DLF_115200_VAL	0x06

#define COM1_BASE_ADRS		0xB0002000
#define COM1_INT_LVL		0x08		/* UART_A connected to IRQ8 */
#define COM1_INT_VEC		(INT_VEC_IRQ0 + COM1_INT_LVL)
#define COM1_INT_PRI		3
#define COM1_BAUD_RATE		115200
#define COM1_DLF			SYNOPSIS_UART_DLF_115200_VAL

#define COM2_BASE_ADRS		0xB0002400
#define COM2_INT_LVL		0x06		/* UART_B connected to IRQ6 */
#define COM2_INT_VEC		(INT_VEC_IRQ0 + COM2_INT_LVL)
#define COM2_INT_PRI		3
#define COM2_BAUD_RATE		115200
#define COM2_DLF			SYNOPSIS_UART_DLF_115200_VAL

#define UART_REG_ADDR_INTERVAL  4       /* address diff of adjacent regs. */

/*
 * On the board the UART works on the same clock frequency as CPU.
 */
#define UART_XTAL_FREQ	        CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC

/* UART uses level triggered interrupt, low level */
#define UART_IOAPIC_FLAGS       (IOAPIC_LEVEL)

/* uart configuration settings */

#define CONFIG_UART0_CONSOLE_REGS	COM1_BASE_ADRS
#define CONFIG_UART0_CONSOLE_IRQ	COM1_INT_LVL
#define CONFIG_UART0_CONSOLE_INT_PRI	COM1_INT_PRI

#define CONFIG_UART1_CONSOLE_REGS	COM2_BASE_ADRS
#define CONFIG_UART1_CONSOLE_IRQ	COM2_INT_LVL
#define CONFIG_UART1_CONSOLE_INT_PRI	COM2_INT_PRI

#ifndef _ASMLANGUAGE
extern struct device * const uart_devs[];
#endif

/* Setup console from config value, */
#if defined(CONFIG_UART_CONSOLE)

#if (CONFIG_UART_CONSOLE_INDEX == 0)
#define CONFIG_UART_BAUDRATE		COM1_BAUD_RATE
#define CONFIG_UART_CONSOLE_INT_PRI	CONFIG_UART0_CONSOLE_INT_PRI
#define CONFIG_UART_CONSOLE_IRQ CONFIG_UART0_CONSOLE_IRQ
#elif (CONFIG_UART_CONSOLE_INDEX == 1)
#define CONFIG_UART_BAUDRATE		COM2_BAUD_RATE
#define CONFIG_UART_CONSOLE_IRQ CONFIG_UART1_CONSOLE_IRQ
#define CONFIG_UART_CONSOLE_INT_PRI	CONFIG_UART1_CONSOLE_INT_PRI
#endif  /* CONFIG_UART_CONSOLE_INDEX */

#define UART_CONSOLE_DEV (uart_devs[CONFIG_UART_CONSOLE_INDEX])

#endif /* CONFIG_UART_CONSOLE */

#ifndef _ASMLANGUAGE

/*
 * The <pri> parameter is deliberately ignored. For this BSP, the macro just has
 * to make sure that unique vector numbers are generated.
 */
#define SYS_INT_REGISTER(s, irq, pri) \
	NANO_CPU_INT_REGISTER(s, INT_VEC_IRQ0 + (irq), 0)
#endif

#ifndef _ASMLANGUAGE

/**************************************************************************
*
* outByte - output byte to memory location
*
* RETURNS: N/A
*
* NOMANUAL
*/

static __inline__ void outByte(uint8_t data, uint32_t addr)
{
	*(volatile uint8_t *)addr = data;
}

/**************************************************************************
*
* inByte - obtain byte value from memory location
*
* This function issues the 'move' instruction to read a byte from the
* specified memory address.
*
* RETURNS: the byte read from the specified memory address
*
* NOMANUAL
*/

static __inline__ uint8_t inByte(uint32_t addr)
{
	return *((volatile uint8_t *)addr);
}

/*
 * Device drivers utilize the macros PLB_WORD_REG_WRITE() and
 * PLB_WORD_REG_READ() to access shortword-wide registers on the processor
 * local bus (PLB), as opposed to a PCI bus, for example.  Boards are
 * expected to provide implementations of these macros.
 */

/**************************************************************************
*
* outWord - output word to memory location
*
* RETURNS: N/A
*
* NOMANUAL
*/

static __inline__ void outWord(uint16_t data, uint32_t addr)
{
	*(volatile uint16_t *)addr = data;
}

/**************************************************************************
*
* inWord - obtain word value from memory location
*
* This function issues the 'move' instruction to read a word from the
* specified memory address.
*
* RETURNS: the word read from the specified memory address
*
* NOMANUAL
*/

static __inline__ uint16_t inWord(uint32_t addr)
{
	return *((volatile uint16_t *)addr);
}

/**************************************************************************
*
* outLong - output long word to memory location
*
* RETURNS: N/A
*
* NOMANUAL
*/

static __inline__ void outLong(uint32_t data, uint32_t addr)
{
	*(volatile uint32_t *)addr = data;
}

/**************************************************************************
*
* inLong - obtain long word value from memory location
*
* This function issues the 'move' instruction to read a word from the
* specified memory address.
*
* RETURNS: the long word read from the specified memory address
*
* NOMANUAL
*/

static __inline__ uint32_t inLong(uint32_t addr)
{
	return *((volatile uint32_t *)addr);
}
#endif /* !_ASMLANGUAGE */

extern void _SysIntVecProgram(unsigned int vector, unsigned int);

#endif /* __INCboardh */
