#include <stdio.h>
#include <stdint.h>
#include <device.h>
#include <init.h>

#include "board.h"
#include <nanokernel.h>
#include <arch/cpu.h>

/******************************************************************************
 ***
 ***           PURE_INIT functions
 ***
 *****************************************************************************/
#ifdef CONFIG_PINMUX

#include <pinmux/pinmux.h>

struct pinmux_config atp_pmux = {
	.base_address = CONFIG_PINMUX_BASE,
};

DECLARE_DEVICE_INIT_CONFIG(pmux,			/* config name */
			   PINMUX_NAME,			/* driver name */
			   &pinmux_initialize,		/* init function */
			   &atp_pmux);			/* config options*/
pure_init(pmux, NULL);

#endif /* CONFIG_PINMUX */

#ifdef CONFIG_DW_RTC
#include <rtc/dw_rtc.h>

struct dw_rtc_dev_config rtc_dev = {
	.base_address = RTC_BASE_ADDR,
};

DECLARE_DEVICE_INIT_CONFIG(rtc,
			   RTC_DRV_NAME,
			   &dw_rtc_init,
			   &rtc_dev);

micro_early_init(rtc, NULL);

#endif

#ifdef CONFIG_DW_WDT
#include <wdt/dw_wdt.h>

struct dw_wdt_dev_config wdt_dev = {
	.base_address = WDT_BASE_ADDR,
};

DECLARE_DEVICE_INIT_CONFIG(wdt,
			   WDT_DRV_NAME,
			   &dw_wdt_init,
			   &wdt_dev);

micro_early_init(wdt, NULL);

#endif

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

#if CONFIG_GPIO_DW_1
void gpio_config_1_irq(struct device *port);

struct gpio_config_dw gpio_config_dw_1 = {
	.base_addr = CONFIG_GPIO_DW_1_BASE_ADDR,
	.bits = CONFIG_GPIO_DW_1_BITS,
	.irq_num = CONFIG_GPIO_DW_1_IRQ,
	.config_func = gpio_config_1_irq
};

struct gpio_runtime_dw gpio_1_runtime;

DECLARE_DEVICE_INIT_CONFIG(gpio_1, CONFIG_GPIO_DW_1_NAME,
			   gpio_initialize_dw, &gpio_config_dw_1);
pure_init(gpio_1, &gpio_1_runtime);

IRQ_CONNECT_STATIC(gpio_dw_1, CONFIG_GPIO_DW_1_IRQ,
		   CONFIG_GPIO_DW_1_PRI, gpio_dw_isr_1, 0);

void gpio_config_1_irq(struct device *port)
{
	struct gpio_config_dw *config = port->config->config_info;
	IRQ_CONFIG(gpio_dw_1, config->irq_num);
}

void gpio_dw_isr_1(void *unused)
{
	gpio_dw_isr(&__initconfig_gpio_11);
}

#endif /* CONFIG_GPIO_DW_1 */
#endif /* CONFIG_GPIO_DW */

#ifdef CONFIG_DW_AIO_COMPARATOR
#include <aio/dw_aio_comparator.h>

int dw_aio_cmp_config(struct device *dev);

struct dw_aio_cmp_dev_cfg_t dw_aio_cmp_dev_config = {
	.base_address = CONFIG_DW_AIO_COMPARATOR_BASE_ADDR,
	.interrupt_num = INT_AIO_CMP_IRQ,
	.config_func = dw_aio_cmp_config,
};

DECLARE_DEVICE_INIT_CONFIG(dw_aio_cmp,
			   DW_AIO_CMP_DRV_NAME,
			   &dw_aio_cmp_init,
			   &dw_aio_cmp_dev_config);

struct dw_aio_cmp_dev_data_t dw_aio_cmp_dev_data = {
	.num_cmp = DW_AIO_CMP_COUNT,
};

micro_early_init(dw_aio_cmp, &dw_aio_cmp_dev_data);

extern void dw_aio_cmp_isr(struct device *dev);

void dw_aio_cmp_isr_dispatcher(void *unused)
{
	dw_aio_cmp_isr(&__initconfig_dw_aio_cmp4);
}

IRQ_CONNECT_STATIC(dw_aio_cmp, INT_AIO_CMP_IRQ, 0, dw_aio_cmp_isr_dispatcher, 0);

int dw_aio_cmp_config(struct device *dev)
{
	ARG_UNUSED(dev);

	IRQ_CONFIG(dw_aio_cmp, INT_AIO_CMP_IRQ);

	return DEV_OK;
}
#endif

#ifdef CONFIG_NS16550
#include <drivers/uart.h>
#include <console/uart_console.h>
#include <serial/ns16550.h>

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
		.baud_rate = CONFIG_UART_BAUDRATE,
		.sys_clk_freq = UART_XTAL_FREQ,
		.int_pri = CONFIG_UART_CONSOLE_INT_PRI
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


/* UART 1 */
static struct uart_device_config_t ns16550_uart1_dev_cfg = {
	.port = CONFIG_UART_PORT_1_REGS,
	.irq = CONFIG_UART_PORT_1_IRQ,
	.int_pri = CONFIG_UART_PORT_1_IRQ_PRIORITY,

#if (CONFIG_UART_CONSOLE_INDEX == 0)
	.config_func = ns16550_uart_console_init,
#endif
};

DECLARE_DEVICE_INIT_CONFIG(ns16550_uart1,
			   CONFIG_UART_PORT_1_NAME,
			   &uart_platform_init,
			   &ns16550_uart1_dev_cfg);

static struct uart_ns16550_dev_data_t ns16550_uart1_dev_data;

pure_early_init(ns16550_uart1, &ns16550_uart1_dev_data);


/**
 * @brief UART devices
 *
 * Note that UART 0 is being used by OpenOCD, so the first
 * UART entry is actually the UART#1 on board.
 */
struct device * const uart_devs[] = {
	&__initconfig_ns16550_uart10,
};


#endif
