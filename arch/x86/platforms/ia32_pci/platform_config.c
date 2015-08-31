/*
 * Copyright (c) 2015 Intel Corporation.
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

/**
 * @file Contains configuration for ia32_pci platforms.
 */

#include <stdio.h>
#include <stdint.h>
#include <device.h>
#include <init.h>

#include "board.h"

#ifdef CONFIG_NS16550
#include <drivers/uart.h>
#include <console/uart_console.h>
#include <serial/ns16550.h>


#if defined(CONFIG_UART_CONSOLE)
#if defined(CONFIG_PRINTK) || defined(CONFIG_STDOUT_CONSOLE)

/**
 * @brief Initialize NS16550 serial port #1
 *
 * UART #1 is being used as console. So initialize it
 * for console I/O.
 *
 * @param dev The UART device struct
 *
 * @return DEV_OK if successful, otherwise failed.
 */
static int ns16550_uart_console_init(struct device *dev)
{
	struct uart_init_info info = {
		.baud_rate = CONFIG_UART_CONSOLE_BAUDRATE,
		.sys_clk_freq = UART_XTAL_FREQ,
	};

	uart_init(UART_CONSOLE_DEV, &info);

	return DEV_OK;
}

#else

static int ns16550_uart_console_init(struct device *dev)
{
	ARG_UNUSED(dev);

	return DEV_OK;
}

#endif
#endif /* CONFIG_UART_CONSOLE */

/**
 * @brief UART Device configuration.
 *
 * This contains the device configuration for UART.
 */
struct uart_device_config_t ns16550_uart_dev_cfg[] = {
	{
		.port = CONFIG_NS16550_PORT_0_BASE_ADDR,

		.port_init = ns16550_uart_port_init,

		#if (defined(CONFIG_UART_CONSOLE) \
		     && (CONFIG_UART_CONSOLE_INDEX == 0))
			.config_func = ns16550_uart_console_init,
		#endif
		.pci_dev.class = PCI_CLASS_COMM_CTLR,
		.pci_dev.bus = CONFIG_UART_PCI_BUS,
		.pci_dev.dev = CONFIG_UART_PCI_DEV,
		.pci_dev.vendor_id = CONFIG_UART_PCI_VENDOR_ID,
		.pci_dev.device_id = CONFIG_UART_PCI_DEVICE_ID,
		.pci_dev.function = CONFIG_UART_PORT_0_FUNCTION,
		.pci_dev.bar = CONFIG_UART_PCI_BAR,
	},
	{
		.port = CONFIG_NS16550_PORT_1_BASE_ADDR,

		.port_init = ns16550_uart_port_init,

		#if (defined(CONFIG_UART_CONSOLE) \
		     && (CONFIG_UART_CONSOLE_INDEX == 1))
			.config_func = ns16550_uart_console_init,
		#endif
		.pci_dev.class = PCI_CLASS_COMM_CTLR,
		.pci_dev.bus = CONFIG_UART_PCI_BUS,
		.pci_dev.dev = CONFIG_UART_PCI_DEV,
		.pci_dev.vendor_id = CONFIG_UART_PCI_VENDOR_ID,
		.pci_dev.device_id = CONFIG_UART_PCI_DEVICE_ID,
		.pci_dev.function = CONFIG_UART_PORT_1_FUNCTION,
		.pci_dev.bar = CONFIG_UART_PCI_BAR,
	},
	/* Add pre-configured ports after this. */
};

/**< Device data */
static struct uart_ns16550_dev_data_t ns16550_uart_dev_data[2];

/* UART 0 */
DECLARE_DEVICE_INIT_CONFIG(ns16550_uart0,
			   CONFIG_UART_PORT_0_NAME,
			   &uart_platform_init,
			   &ns16550_uart_dev_cfg[0]);

#if (defined(CONFIG_EARLY_CONSOLE) && \
		defined(CONFIG_UART_CONSOLE) && \
		(CONFIG_UART_CONSOLE_INDEX == 0))
pure_early_init(ns16550_uart0, &ns16550_uart_dev_data[0]);
#else
pure_init(ns16550_uart0, &ns16550_uart_dev_data[0]);
#endif /* CONFIG_EARLY_CONSOLE */


/* UART 1 */
DECLARE_DEVICE_INIT_CONFIG(ns16550_uart1,
			   CONFIG_UART_PORT_1_NAME,
			   &uart_platform_init,
			   &ns16550_uart_dev_cfg[1]);

#if (defined(CONFIG_EARLY_CONSOLE) && \
		defined(CONFIG_UART_CONSOLE) && \
		(CONFIG_UART_CONSOLE_INDEX == 1))
pure_early_init(ns16550_uart1, &ns16550_uart_dev_data[1]);
#else
pure_init(ns16550_uart1, &ns16550_uart_dev_data[1]);
#endif /* CONFIG_EARLY_CONSOLE */


/**< UART Devices */
struct device * const uart_devs[] = {
#if (defined(CONFIG_EARLY_CONSOLE) && \
		defined(CONFIG_UART_CONSOLE) && \
		(CONFIG_UART_CONSOLE_INDEX == 0))
	&__initconfig_ns16550_uart00,
#else
	&__initconfig_ns16550_uart01,
#endif /* CONFIG_EARLY_CONSOLE */
#if (defined(CONFIG_EARLY_CONSOLE) && \
		defined(CONFIG_UART_CONSOLE) && \
		(CONFIG_UART_CONSOLE_INDEX == 1))
	&__initconfig_ns16550_uart10,
#else
	&__initconfig_ns16550_uart11,
#endif /* CONFIG_EARLY_CONSOLE */
};

#endif

#ifdef CONFIG_SPI_INTEL

#include <spi/intel_spi.h>

#ifdef CONFIG_SPI_INTEL_PORT_0

void spi_config_0_irq(struct device *dev);

struct spi_intel_data spi_intel_data_port_0;

struct spi_intel_config spi_intel_config_0 = {
	.regs = CONFIG_SPI_INTEL_PORT_0_REGS,
	.irq = CONFIG_SPI_INTEL_PORT_0_IRQ,
	.pci_dev.class = CONFIG_SPI_INTEL_CLASS,
	.pci_dev.bus = CONFIG_SPI_INTEL_PORT_0_BUS,
	.pci_dev.dev = CONFIG_SPI_INTEL_PORT_0_DEV,
	.pci_dev.vendor_id = CONFIG_SPI_INTEL_VENDOR_ID,
	.pci_dev.device_id = CONFIG_SPI_INTEL_DEVICE_ID,
	.pci_dev.function = CONFIG_SPI_INTEL_PORT_0_FUNCTION,
	.config_func = spi_config_0_irq
};

DECLARE_DEVICE_INIT_CONFIG(spi_intel_port_0, CONFIG_SPI_INTEL_PORT_0_DRV_NAME,
			   spi_intel_init, &spi_intel_config_0);

pure_init(spi_intel_port_0, &spi_intel_data_port_0);

void spi_intel_isr_0(void *unused)
{
	spi_intel_isr(&__initconfig_spi_intel_port_01);
}

IRQ_CONNECT_STATIC(spi_intel_irq_port_0, CONFIG_SPI_INTEL_PORT_0_IRQ,
		   CONFIG_SPI_INTEL_PORT_0_PRI, spi_intel_isr_0, 0);

void spi_config_0_irq(struct device *dev)
{
	struct spi_intel_config *config = dev->config->config_info;
	IRQ_CONFIG(spi_intel_irq_port_0, config->irq);
}

#endif /* CONFIG_SPI_INTEL_PORT_0 */
#ifdef CONFIG_SPI_INTEL_PORT_1

void spi_config_1_irq(struct device *dev);

struct spi_intel_data spi_intel_data_port_1;

struct spi_intel_config spi_intel_config_1 = {
	.regs = CONFIG_SPI_INTEL_PORT_1_REGS,
	.irq = CONFIG_SPI_INTEL_PORT_1_IRQ,
	.pci_dev.class = CONFIG_SPI_INTEL_CLASS,
	.pci_dev.bus = CONFIG_SPI_INTEL_PORT_1_BUS,
	.pci_dev.dev = CONFIG_SPI_INTEL_PORT_1_DEV,
	.pci_dev.function = CONFIG_SPI_INTEL_PORT_1_FUNCTION,
	.pci_dev.vendor_id = CONFIG_SPI_INTEL_VENDOR_ID,
	.pci_dev.device_id = CONFIG_SPI_INTEL_DEVICE_ID,
	.config_func = spi_config_1_irq
};

DECLARE_DEVICE_INIT_CONFIG(spi_intel_port_1, CONFIG_SPI_INTEL_PORT_1_DRV_NAME,
			   spi_intel_init, &spi_intel_config_1);

pure_init(spi_intel_port_1, &spi_intel_data_port_1);

void spi_intel_isr_1(void *unused)
{
	spi_intel_isr(&__initconfig_spi_intel_port_11);
}

IRQ_CONNECT_STATIC(spi_intel_irq_port_1, CONFIG_SPI_INTEL_PORT_1_IRQ,
		   CONFIG_SPI_INTEL_PORT_1_PRI, spi_intel_isr_1, 0);

void spi_config_1_irq(struct device *dev)
{
	struct spi_intel_config *config = dev->config->config_info;
	IRQ_CONFIG(spi_intel_irq_port_1, config->irq);
}

#endif /* CONFIG_SPI_INTEL_PORT_1 */
#endif /* CONFIG_SPI_INTEL */

#if CONFIG_GPIO_DW

#include <gpio.h>
#include <gpio/gpio-dw.h>
#include <pic.h>
#include <ioapic.h>

extern void gpio_dw_isr(struct device *port);

#if CONFIG_GPIO_DW_0
void gpio_config_0_irq(struct device *port);

struct gpio_config_dw gpio_config_dw_0 = {
	.base_addr = CONFIG_GPIO_DW_0_BASE_ADDR,
	.bits = CONFIG_GPIO_DW_0_BITS,
	.irq_num = CONFIG_GPIO_DW_0_IRQ,
	.pci_dev.class = CONFIG_GPIO_DW_CLASS,
	.pci_dev.bus = CONFIG_GPIO_DW_0_BUS,
	.pci_dev.dev = CONFIG_GPIO_DW_0_DEV,
	.pci_dev.vendor_id = CONFIG_GPIO_DW_VENDOR_ID,
	.pci_dev.device_id = CONFIG_GPIO_DW_DEVICE_ID,
	.pci_dev.function = CONFIG_GPIO_DW_0_FUNCTION,
	.pci_dev.bar = CONFIG_GPIO_DW_0_BAR,
	.config_func = gpio_config_0_irq
};

struct gpio_runtime_dw gpio_0_runtime;

DECLARE_DEVICE_INIT_CONFIG(gpio_0, CONFIG_GPIO_DW_0_NAME,
			   gpio_initialize_dw, &gpio_config_dw_0);
pure_init(gpio_0, &gpio_0_runtime);

IRQ_CONNECT_STATIC(gpio_dw_0, CONFIG_GPIO_DW_0_IRQ,
		   CONFIG_GPIO_DW_0_PRI, gpio_dw_isr_0, 0);

void gpio_config_0_irq(struct device *port)
{
	struct gpio_config_dw *config = port->config->config_info;
	IRQ_CONFIG(gpio_dw_0, config->irq_num);
}

void gpio_dw_isr_0(void *unused)
{
	gpio_dw_isr(&__initconfig_gpio_01);
}

#endif /* CONFIG_GPIO_DW_0 */
#endif /* CONFIG_GPIO_DW */
