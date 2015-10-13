/*
 * Copyright (c) 2015 Intel Corporation.
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

#include <nanokernel.h>
#include <gpio.h>
#include <gpio/gpio-dw.h>
#include <board.h>
#include <sys_io.h>
#include <init.h>

#ifdef CONFIG_SHARED_IRQ
#include <shared_irq.h>
#endif

#define SWPORTA_DR	0x00
#define SWPORTA_DDR	0x04
#define SWPORTB_DR	0x0c
#define SWPORTB_DDR	0x10
#define SWPORTC_DR	0x18
#define SWPORTC_DDR	0x1c
#define SWPORTD_DR	0x24
#define SWPORTD_DDR	0x28
#define INTEN		0x30
#define INTMASK		0x34
#define INTTYPE_LEVEL	0x38
#define INT_POLARITY	0x3c
#define INTSTATUS	0x40
#define PORTA_DEBOUNCE	0x48
#define PORTA_EOI	0x4c
#define EXT_PORTA	0x50
#define EXT_PORTB	0x54
#define EXT_PORTC	0x58
#define EXT_PORTD	0x5c
#define INT_CLOCK_SYNC	0x60
#define INT_BOTHEDGE	0x68

#define BIT(n)	(1UL << (n))

static inline uint32_t dw_read(uint32_t base_addr, uint32_t offset)
{
	return sys_read32(base_addr + offset);
}

static inline void dw_write(uint32_t base_addr, uint32_t offset,
			   uint32_t val)
{
	sys_write32(val, base_addr + offset);
}


static void dw_set_bit(uint32_t base_addr, uint32_t offset,
			      uint32_t bit, uint8_t value)
{
	if (!value) {
		sys_clear_bit(base_addr + offset, bit);
	} else {
		sys_set_bit(base_addr + offset, bit);
	}
}

static inline void dw_interrupt_config(struct device *port, int access_op,
			 uint32_t pin, int flags)
{
	struct gpio_config_dw *config = port->config->config_info;
	uint32_t base_addr = config->base_addr;

	uint8_t flag_is_set;

	/* set as an input pin */
	dw_set_bit(base_addr, SWPORTA_DDR, pin, 0);

	/* level or edge */
	flag_is_set = (flags & GPIO_INT_EDGE);
	dw_set_bit(base_addr, INTTYPE_LEVEL, pin, flag_is_set);

	/* Active low/high */
	flag_is_set = (flags & GPIO_INT_ACTIVE_HIGH);
	dw_set_bit(base_addr, INT_POLARITY, pin, flag_is_set);

	/* both edges */
	flag_is_set = (flags & GPIO_INT_DOUBLE_EDGE);
	if (flag_is_set) {
		dw_set_bit(base_addr, INT_BOTHEDGE, pin, flag_is_set);
		dw_set_bit(base_addr, INTTYPE_LEVEL, pin, flag_is_set);
	}

	/* use built-in debounce  */
	flag_is_set = (flags & GPIO_INT_DEBOUNCE );
	dw_set_bit(base_addr, PORTA_DEBOUNCE, pin, flag_is_set);

	/* level triggered int synchronous with clock */
	flag_is_set = (flags & GPIO_INT_CLOCK_SYNC );
	dw_set_bit(base_addr, INT_CLOCK_SYNC, pin, flag_is_set);
	dw_set_bit(base_addr, INTEN, pin, 1);
}

static inline void dw_pin_config(struct device *port,
			 uint32_t pin, int flags)
{
	struct gpio_config_dw *config = port->config->config_info;
	uint32_t base_addr = config->base_addr;

	/* clear interrupt enable */
	dw_set_bit(base_addr, INTEN, pin, 0);

	/* set direction */
	dw_set_bit(base_addr, SWPORTA_DDR, pin, (flags & GPIO_DIR_MASK));

	if (flags &  GPIO_INT)
		dw_interrupt_config(port, GPIO_ACCESS_BY_PIN, pin, flags);
}

static inline void dw_port_config(struct device *port, int flags)
{
	struct gpio_config_dw *config = port->config->config_info;
	int i;

	for (i=0; i < config->bits; i++) {
		dw_pin_config(port, i, flags);
	}
}

static inline int gpio_config_dw(struct device *port, int access_op,
				 uint32_t pin, int flags)
{
	if (((flags & GPIO_INT) && (flags & GPIO_DIR_OUT)) ||
	    ((flags & GPIO_DIR_IN) && (flags & GPIO_DIR_OUT))) {
		return -1;
	}

	if (GPIO_ACCESS_BY_PIN == access_op) {
		dw_pin_config(port, pin, flags);
	} else {
		dw_port_config(port, flags);
	}
	return 0;
}

static inline int gpio_write_dw(struct device *port, int access_op,
				uint32_t pin, uint32_t value)
{
	struct gpio_config_dw *config = port->config->config_info;
	uint32_t base_addr = config->base_addr;

	if (GPIO_ACCESS_BY_PIN == access_op) {
		dw_set_bit(base_addr, SWPORTA_DR, pin, value);
	} else {
		dw_write(base_addr, SWPORTA_DR, value);
	}

	return 0;
}

static inline int gpio_read_dw(struct device *port, int access_op,
				    uint32_t pin, uint32_t *value)
{
	struct gpio_config_dw *config = port->config->config_info;
	uint32_t base_addr = config->base_addr;

	*value = dw_read(base_addr, EXT_PORTA);

	if (GPIO_ACCESS_BY_PIN == access_op) {
		*value = !!(*value & BIT(pin));
	}
	return 0;
}

static inline int gpio_set_callback_dw(struct device *port,
				       gpio_callback_t callback)
{
	struct gpio_runtime_dw *context = port->driver_data;

	context->callback = callback;

	return 0;
}

static inline int gpio_enable_callback_dw(struct device *port, int access_op,
					  uint32_t pin)
{
	struct gpio_config_dw *config = port->config->config_info;
	struct gpio_runtime_dw *context = port->driver_data;
	uint32_t base_addr = config->base_addr;

	if (GPIO_ACCESS_BY_PIN == access_op) {
		context->enabled_callbacks |= BIT(pin);
	} else {
		context->port_callback = 1;
	}
	dw_write(base_addr, PORTA_EOI, BIT(pin));
	dw_set_bit(base_addr, INTMASK, pin, 0);
	return 0;
}

static inline int gpio_disable_callback_dw(struct device *port, int access_op,
					   uint32_t pin)
{
	struct gpio_config_dw *config = port->config->config_info;
	struct gpio_runtime_dw *context = port->driver_data;
	uint32_t base_addr = config->base_addr;

	if (GPIO_ACCESS_BY_PIN == access_op) {
		context->enabled_callbacks &= ~(BIT(pin));
	} else {
		context->port_callback = 0;
	}
	dw_set_bit(base_addr, INTMASK, pin, 1);

	return 0;
}

static inline int gpio_suspend_port_dw(struct device *port)
{
	return 0;
}

static inline int gpio_resume_port_dw(struct device *port)
{
	return 0;
}

void gpio_dw_isr(struct device *port)
{
	struct gpio_runtime_dw *context = port->driver_data;
	struct gpio_config_dw *config = port->config->config_info;
	uint32_t base_addr = config->base_addr;
	uint32_t enabled_int, int_status, bit;

	int_status = dw_read(base_addr, INTSTATUS);

#ifdef CONFIG_SHARED_IRQ
	/* If using with shared IRQ, this function will be called
	 * by the shared IRQ driver. So check here if the interrupt
	 * is coming from the GPIO controller (or somewhere else).
	 */
	if (!int_status) {
		return;
	}
#endif

	dw_write(base_addr, PORTA_EOI, -1);

	if (!context->callback) {
		return;
	}
	if (context->port_callback) {
		context->callback(port, int_status);
		return;
	}

	if (context->enabled_callbacks) {
		enabled_int = int_status & context->enabled_callbacks;
		for (bit = 0; bit < config->bits; bit++) {
			if (enabled_int & (1 << bit)) {
				context->callback(port, (1 << bit));
			}
		}
	}

}

static struct gpio_driver_api api_funcs = {
	.config = gpio_config_dw,
	.write = gpio_write_dw,
	.read = gpio_read_dw,
	.set_callback = gpio_set_callback_dw,
	.enable_callback = gpio_enable_callback_dw,
	.disable_callback = gpio_disable_callback_dw,
	.suspend = gpio_suspend_port_dw,
	.resume = gpio_resume_port_dw
};

#ifdef CONFIG_PCI
static inline int gpio_dw_setup(struct device *dev)
{
	struct gpio_config_dw *config = dev->config->config_info;

	pci_bus_scan_init();

	if (!pci_bus_scan(&config->pci_dev)) {
		return 0;
	}

#ifdef CONFIG_PCI_ENUMERATION
	config->base_addr = config->pci_dev.addr;
	config->irq_num = config->pci_dev.irq;
#endif
	pci_enable_regs(&config->pci_dev);

	pci_show(&config->pci_dev);

	return 1;
}
#else
#define gpio_dw_setup(_unused_) (1)
#endif /* CONFIG_PCI */


int gpio_initialize_dw(struct device *port)
{
	struct gpio_config_dw *config = port->config->config_info;
	uint32_t base_addr = config->base_addr;

	if (!gpio_dw_setup(port)) {
		return DEV_NOT_CONFIG;
	}

	/* interrupts in sync with system clock */
	dw_set_bit(base_addr, INT_CLOCK_SYNC, 0, 1);

	/* mask and disable interrupts */
	dw_write(base_addr, INTMASK, ~(0));
	dw_write(base_addr, INTEN, 0);
	dw_write(base_addr, PORTA_EOI, ~(0));

	port->driver_api = &api_funcs;

	config->config_func(port);

	return 0;
}

/* Bindings to the plaform */
#if CONFIG_GPIO_DW_0
void gpio_config_0_irq(struct device *port);

struct gpio_config_dw gpio_config_dw_0 = {
	.base_addr = CONFIG_GPIO_DW_0_BASE_ADDR,
	.bits = CONFIG_GPIO_DW_0_BITS,
#ifdef CONFIG_GPIO_DW_0_IRQ_DIRECT
	.irq_num = CONFIG_GPIO_DW_0_IRQ,
#endif
#if CONFIG_PCI
	.pci_dev.class = CONFIG_GPIO_DW_CLASS,
	.pci_dev.bus = CONFIG_GPIO_DW_0_BUS,
	.pci_dev.dev = CONFIG_GPIO_DW_0_DEV,
	.pci_dev.vendor_id = CONFIG_GPIO_DW_VENDOR_ID,
	.pci_dev.device_id = CONFIG_GPIO_DW_DEVICE_ID,
	.pci_dev.function = CONFIG_GPIO_DW_0_FUNCTION,
	.pci_dev.bar = CONFIG_GPIO_DW_0_BAR,
#endif
	.config_func = gpio_config_0_irq,

#ifdef CONFIG_GPIO_DW_0_IRQ_SHARED
	.shared_irq_dev_name = CONFIG_GPIO_DW_0_IRQ_SHARED_NAME,
#endif
};

struct gpio_runtime_dw gpio_0_runtime;

DECLARE_DEVICE_INIT_CONFIG(gpio_0, CONFIG_GPIO_DW_0_NAME,
			   gpio_initialize_dw, &gpio_config_dw_0);
pre_kernel_late_init(gpio_0, &gpio_0_runtime);

#ifdef CONFIG_GPIO_DW_0_IRQ_DIRECT
IRQ_CONNECT_STATIC(gpio_dw_0, CONFIG_GPIO_DW_0_IRQ,
		   CONFIG_GPIO_DW_0_PRI, gpio_dw_isr_0, 0);
#endif

void gpio_config_0_irq(struct device *port)
{
	struct gpio_config_dw *config = port->config->config_info;
	struct device *shared_irq_dev;

#ifdef CONFIG_GPIO_DW_0_IRQ_DIRECT
	ARG_UNUSED(shared_irq_dev);
	IRQ_CONFIG(gpio_dw_0, config->irq_num);
	irq_enable(config->irq_num);
#elif defined(CONFIG_GPIO_DW_0_IRQ_SHARED)
	ARG_UNUSED(config);
	shared_irq_dev = device_get_binding(config->shared_irq_dev_name);
	shared_irq_isr_register(shared_irq_dev, (isr_t)gpio_dw_isr, port);
	shared_irq_enable(shared_irq_dev, port);
#endif
}

#ifdef CONFIG_GPIO_DW_0_IRQ_DIRECT
void gpio_dw_isr_0(void *unused)
{
	gpio_dw_isr(&__initconfig_gpio_02);
}
#endif /* CONFIG_GPIO_DW_0_IRQ_DIRECT */

#endif /* CONFIG_GPIO_DW_0 */


#if CONFIG_GPIO_DW_1
void gpio_config_1_irq(struct device *port);

struct gpio_config_dw gpio_config_dw_1 = {
	.base_addr = CONFIG_GPIO_DW_1_BASE_ADDR,
	.bits = CONFIG_GPIO_DW_1_BITS,
#ifdef CONFIG_GPIO_DW_1_IRQ_DIRECT
	.irq_num = CONFIG_GPIO_DW_1_IRQ,
#endif
#if CONFIG_PCI
	.pci_dev.class = CONFIG_GPIO_DW_CLASS,
	.pci_dev.bus = CONFIG_GPIO_DW_1_BUS,
	.pci_dev.dev = CONFIG_GPIO_DW_1_DEV,
	.pci_dev.vendor_id = CONFIG_GPIO_DW_VENDOR_ID,
	.pci_dev.device_id = CONFIG_GPIO_DW_DEVICE_ID,
	.pci_dev.function = CONFIG_GPIO_DW_1_FUNCTION,
	.pci_dev.bar = CONFIG_GPIO_DW_1_BAR,
#endif
	.config_func = gpio_config_1_irq,

#ifdef CONFIG_GPIO_DW_1_IRQ_SHARED
	.shared_irq_dev_name = CONFIG_GPIO_DW_1_IRQ_SHARED_NAME,
#endif
};

struct gpio_runtime_dw gpio_1_runtime;

DECLARE_DEVICE_INIT_CONFIG(gpio_1, CONFIG_GPIO_DW_1_NAME,
			   gpio_initialize_dw, &gpio_config_dw_1);
pre_kernel_late_init(gpio_1, &gpio_1_runtime);

#ifdef CONFIG_GPIO_DW_1_IRQ_DIRECT
IRQ_CONNECT_STATIC(gpio_dw_1, CONFIG_GPIO_DW_1_IRQ,
		   CONFIG_GPIO_DW_1_PRI, gpio_dw_isr_1, 0);
#endif

void gpio_config_1_irq(struct device *port)
{
	struct gpio_config_dw *config = port->config->config_info;
	struct device *shared_irq_dev;

#ifdef CONFIG_GPIO_DW_1_IRQ_DIRECT
	ARG_UNUSED(shared_irq_dev);
	IRQ_CONFIG(gpio_dw_1, config->irq_num);
	irq_enable(config->irq_num);
#elif defined(CONFIG_GPIO_DW_1_IRQ_SHARED)
	ARG_UNUSED(config);
	shared_irq_dev = device_get_binding(config->shared_irq_dev_name);
	shared_irq_isr_register(shared_irq_dev, (isr_t)gpio_dw_isr, port);
	shared_irq_enable(shared_irq_dev, port);
#endif
}

#ifdef CONFIG_GPIO_DW_1_IRQ_DIRECT
void gpio_dw_isr_1(void *unused)
{
	gpio_dw_isr(&__initconfig_gpio_12);
}
#endif

#endif /* CONFIG_GPIO_DW_1 */
