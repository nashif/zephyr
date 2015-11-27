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
#include <gpio/qmsi_gpio.h>
#include <board.h>
#include <sys_io.h>

#include "qm_gpio.h"

#define INTEN           0x30
#define INTMASK         0x34
#define PORTA_EOI       0x4c
#define INT_GPIO_MASK   0x6c

IRQ_CONNECT_STATIC(gpio_0, CONFIG_GPIO_DW_0_IRQ, 0, qm_gpio_isr, 0);

static gpio_callback_t app_callback;

/*
 * We will need one callback per GPIO block as soon as QMSI starts
 * supporting multiple instances of GPIO controllers. For now this
 * implementation only supports block 0 from DW GPIO IPs.
 *
 * TODO: Zephyr's API is not clear about the behavior of the this
 * application callback. QMSI's gpio ISR always delivers us a mask
 * and we just pass this through. Thus, we are not differing from
 * GPIO_ACCESS_BY_PIN x GPIO_ACCESS_BY_PORT use cases. This topic
 * is currently under discussion, so this implementation will be
 * fixed as soon as a decision is made.
 */
static void qmsi_gpio_int_callback(uint32_t pin)
{
	struct device *port = device_get_binding(CONFIG_GPIO_DW_0_NAME);
	if (app_callback)
		app_callback(port, pin);
}

static void qmsi_write_bit(uint32_t *target, uint8_t bit, uint8_t value)
{
	if (value) {
		sys_set_bit((uintptr_t) target, bit);
	} else {
		sys_clear_bit((uintptr_t) target, bit);
	}
}

static inline void qmsi_pin_config(uint32_t pin, int flags)
{
	/* Save int mask and mask this pin while we configure the port.
	 * We do this to avoid "spurious interrupts", which is a behavior
	 * we have observed on QMSI and that still needs investigation.
	 */
	uint32_t intmask = QM_GPIO->gpio_intmask;
	sys_set_bit(QM_GPIO_BASE + INTMASK, pin);

	qm_gpio_port_config_t cfg = { 0 };
	qm_gpio_get_config(QM_GPIO, &cfg);

	qmsi_write_bit(&cfg.direction, pin, (flags & GPIO_DIR_MASK));

	if (flags & GPIO_INT) {
		qmsi_write_bit(&cfg.int_type, pin, (flags & GPIO_INT_EDGE));
		qmsi_write_bit(&cfg.int_polarity, pin, (flags & GPIO_INT_ACTIVE_HIGH));
		qmsi_write_bit(&cfg.int_debounce, pin, (flags & GPIO_INT_DEBOUNCE));
		qmsi_write_bit(&cfg.int_bothedge, pin, (flags & GPIO_INT_DOUBLE_EDGE));
		qmsi_write_bit(&cfg.int_en, pin, 1);
	}

	cfg.cb_fn = qmsi_gpio_int_callback;
	qm_gpio_set_config(QM_GPIO, &cfg);

	/* Recover the original interrupt mask for this port. */
	sys_write32(intmask, QM_GPIO_BASE + INTMASK);
}

static inline void qmsi_port_config(int flags)
{
	int i;

	for (i = 0; i < NUM_GPIO_PINS; i++) {
		qmsi_pin_config(i, flags);
	}
}

static inline int qmsi_gpio_config(struct device *port, int access_op,
				 uint32_t pin, int flags)
{
	if (((flags & GPIO_INT) && (flags & GPIO_DIR_OUT)) ||
	    ((flags & GPIO_DIR_IN) && (flags & GPIO_DIR_OUT))) {
		return -1;
	}

	if (GPIO_ACCESS_BY_PIN == access_op) {
		qmsi_pin_config(pin, flags);
	} else {
		qmsi_port_config(flags);
	}
	return 0;
}

static inline int qmsi_gpio_write(struct device *port, int access_op,
				uint32_t pin, uint32_t value)
{
	if (GPIO_ACCESS_BY_PIN == access_op) {
		if (value) {
			qm_gpio_set_pin(QM_GPIO, pin);
		} else {
			qm_gpio_clear_pin(QM_GPIO, pin);
		}
	} else {
		qm_gpio_write_port(QM_GPIO, value);
	}

	return 0;
}

static inline int qmsi_gpio_read(struct device *port, int access_op,
				    uint32_t pin, uint32_t *value)
{
	if (GPIO_ACCESS_BY_PIN == access_op) {
		*value = qm_gpio_read_pin(QM_GPIO, pin);
	} else {
		*value = qm_gpio_read_port(QM_GPIO);
	}
	return 0;
}

static inline int qmsi_gpio_set_callback(struct device *port,
				       gpio_callback_t callback)
{
	app_callback = callback;

	return 0;
}

static inline int qmsi_gpio_enable_callback(struct device *port, int access_op,
					  uint32_t pin)
{
	sys_set_bit(QM_GPIO_BASE + PORTA_EOI, pin);
	sys_clear_bit(QM_GPIO_BASE + INTMASK, pin);

	return 0;
}

static inline int qmsi_gpio_disable_callback(struct device *port, int access_op,
					   uint32_t pin)
{
	sys_set_bit(QM_GPIO_BASE + INTMASK, pin);

	return 0;
}

static inline int qmsi_gpio_suspend_port(struct device *port)
{
	/* TODO: It's probably enough to just disable clock gating here. */
	return 0;
}

static inline int qmsi_gpio_resume_port(struct device *port)
{
	/* TODO: It's probably enough to just enable clock gating here. */
	return 0;
}

static struct gpio_driver_api api_funcs = {
	.config = qmsi_gpio_config,
	.write = qmsi_gpio_write,
	.read = qmsi_gpio_read,
	.set_callback = qmsi_gpio_set_callback,
	.enable_callback = qmsi_gpio_enable_callback,
	.disable_callback = qmsi_gpio_disable_callback,
	.suspend = qmsi_gpio_suspend_port,
	.resume = qmsi_gpio_resume_port
};

int qmsi_gpio_init(struct device *port)
{
	/* TODO: We probably want to enable clock gating here just to be safe. */

	/* mask and disable interrupts */
	sys_write32(~(0), QM_GPIO_BASE + INTMASK);
	sys_write32(0, QM_GPIO_BASE + INTEN);
	sys_write32(~(0), QM_GPIO_BASE + PORTA_EOI);

	/* Enable GPIO IRQ and unmask interrupts for Lakemont. */
	IRQ_CONFIG(gpio_0, CONFIG_GPIO_DW_0_IRQ);
	sys_clear_bit(QM_SCSS_INT_BASE + INT_GPIO_MASK, 0);
	irq_enable(CONFIG_GPIO_DW_0_IRQ);

	port->driver_api = &api_funcs;
	return 0;
}
