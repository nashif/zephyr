/*
 * Copyright (c) 2015 Intel Corporation
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

#include <device.h>
#include <init.h>
#include "soc.h"

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
