/*
 * Copyright (c) 2010-2015, Wind River Systems, Inc.
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
 * @brief Board configuration macros for the ia32 platform
 *
 * This header file is used to specify and describe board-level aspects for
 * the 'ia32' platform.
 */

#ifndef __SOC_H_
#define __SOC_H_

#include <misc/util.h>

#ifndef _ASMLANGUAGE
#include <device.h>
#include <drivers/rand32.h>
#endif

#ifdef CONFIG_IOAPIC
#include <drivers/ioapic.h>
#if defined(CONFIG_UART_IRQ_FALLING_EDGE)
	#define UART_IRQ_FLAGS (IOAPIC_EDGE | IOAPIC_LOW)
#elif defined(CONFIG_UART_IRQ_RISING_EDGE)
	#define UART_IRQ_FLAGS (IOAPIC_EDGE | IOAPIC_HIGH)
#elif defined(CONFIG_UART_IRQ_LEVEL_HIGH)
	#define UART_IRQ_FLAGS (IOAPIC_LEVEL | IOAPIC_HIGH)
#elif defined(CONFIG_UART_IRQ_LEVEL_LOW)
	#define UART_IRQ_FLAGS (IOAPIC_LEVEL | IOAPIC_LOW)
#endif
#endif /* CONFIG_IOAPIC */

#define INT_VEC_IRQ0 0x20 /* vector number for IRQ0 */

#endif /* __SOC_H_ */
