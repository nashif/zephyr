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
		.irq_pri = CONFIG_UART_CONSOLE_INT_PRI
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
#endif


/* UART 1 */
static struct uart_device_config_t ns16550_uart1_dev_cfg = {
	.port = CONFIG_UART_PORT_1_REGS,
	.irq = CONFIG_UART_PORT_1_IRQ,
	.irq_pri = CONFIG_UART_PORT_1_IRQ_PRIORITY,

	.port_init = ns16550_uart_port_init,

#if (defined(CONFIG_UART_CONSOLE) && (CONFIG_UART_CONSOLE_INDEX == 0))
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
 */
struct device * const uart_devs[] = {
	&__initconfig_ns16550_uart10,
};


#endif


#ifdef CONFIG_CLOCK_CONTROL_ATP
#include <clock_control/atp_clock_control.h>

#ifdef CONFIG_CLOCK_CONTROL_ATP_PERIPHERAL

struct atp_clock_control_config clock_atp_peripheral_config = {
	.base_address = CLOCK_PERIPHERAL_BASE_ADDR
};

DECLARE_DEVICE_INIT_CONFIG(clock_atp_peripheral,
			   CONFIG_CLOCK_CONTROL_ATP_PERIPHERAL_DRV_NAME,
			   &atp_clock_control_init,
			   &clock_atp_peripheral_config);

pure_early_init(clock_atp_peripheral, NULL);

#endif /* CONFIG_CLOCK_CONTROL_ATP_PERIPHERAL */
#ifdef CONFIG_CLOCK_CONTROL_ATP_EXTERNAL

struct atp_clock_control_config clock_atp_external_config = {
	.base_address = CLOCK_EXTERNAL_BASE_ADDR
};

DECLARE_DEVICE_INIT_CONFIG(clock_atp_external,
			   CONFIG_CLOCK_CONTROL_ATP_EXTERNAL_DRV_NAME,
			   &atp_clock_control_init,
			   &clock_atp_external_config);

pure_early_init(clock_atp_external, NULL);

#endif /* CONFIG_CLOCK_CONTROL_ATP_EXTERNAL */
#ifdef CONFIG_CLOCK_CONTROL_ATP_SENSOR

struct atp_clock_control_config clock_atp_sensor_config = {
	.base_address = CLOCK_SENSOR_BASE_ADDR
};

DECLARE_DEVICE_INIT_CONFIG(clock_atp_sensor,
			   CONFIG_CLOCK_CONTROL_ATP_SENSOR_DRV_NAME,
			   &atp_clock_control_init,
			   &clock_atp_sensor_config);

pure_early_init(clock_atp_sensor, NULL);

#endif /* CONFIG_CLOCK_CONTROL_ATP_SENSOR */
#endif /* CONFIG_CLOCK_CONTROL_ATP */

#ifdef CONFIG_SPI_DW

#include <spi/dw_spi.h>
#include <misc/util.h>

#ifdef CONFIG_SPI_DW_PORT_0

void spi_config_0_irq(struct device *dev);

struct spi_dw_data spi_dw_data_port_0;

struct spi_dw_config spi_dw_config_0 = {
	.regs = CONFIG_SPI_DW_PORT_0_REGS,
	.irq = CONFIG_SPI_DW_PORT_0_IRQ,
	.int_mask = SPI_DW_PORT_0_INT_MASK,
	.clock = &__initconfig_clock_atp_peripheral0,
	.clock_data = UINT_TO_POINTER(ATP_CCU_SPI_M0_PCLK_EN_SW),
	.config_func = spi_config_0_irq
};

DECLARE_DEVICE_INIT_CONFIG(spi_dw_port_0, CONFIG_SPI_DW_PORT_0_DRV_NAME,
			   spi_dw_init, &spi_dw_config_0);

pure_init(spi_dw_port_0, &spi_dw_data_port_0);

void spi_dw_isr_0(void *unused)
{
	spi_dw_isr(&__initconfig_spi_dw_port_01);
}

IRQ_CONNECT_STATIC(spi_dw_irq_port_0, CONFIG_SPI_DW_PORT_0_IRQ,
		   CONFIG_SPI_DW_PORT_0_PRI, spi_dw_isr_0, 0);

void spi_config_0_irq(struct device *dev)
{
	struct spi_dw_config *config = dev->config->config_info;
	IRQ_CONFIG(spi_dw_irq_port_0, config->irq);
}

#endif /* CONFIG_SPI_DW_PORT_0 */
#ifdef CONFIG_SPI_DW_PORT_1

void spi_config_1_irq(struct device *dev);

struct spi_dw_data spi_dw_data_port_1;

struct spi_dw_config spi_dw_config_1 = {
	.regs = CONFIG_SPI_DW_PORT_1_REGS,
	.irq = CONFIG_SPI_DW_PORT_1_IRQ,
	.int_mask = SPI_DW_PORT_1_INT_MASK,
	.clock = &__initconfig_clock_atp_peripheral0,
	.clock_data = UINT_TO_POINTER(ATP_CCU_SPI_M1_PCLK_EN_SW),
	.config_func = spi_config_1_irq
};

DECLARE_DEVICE_INIT_CONFIG(spi_dw_port_1, CONFIG_SPI_DW_PORT_1_DRV_NAME,
			   spi_dw_init, &spi_dw_config_1);

pure_init(spi_dw_port_1, &spi_dw_data_port_1);

void spi_dw_isr_1(void *unused)
{
	spi_dw_isr(&__initconfig_spi_dw_port_11);
}

IRQ_CONNECT_STATIC(spi_dw_irq_port_1, CONFIG_SPI_DW_PORT_1_IRQ,
		   CONFIG_SPI_DW_PORT_1_PRI, spi_dw_isr_1, 0);

void spi_config_1_irq(struct device *dev)
{
	struct spi_dw_config *config = dev->config->config_info;
	IRQ_CONFIG(spi_dw_irq_port_1, config->irq);
}

#endif /* CONFIG_SPI_DW_PORT_1 */
#endif /* CONFIG_SPI_DW */
