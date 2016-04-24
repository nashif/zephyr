/*
 * {% copyright %}
 */

#include "qm_gpio.h"

#ifndef UNIT_TEST
#if (QUARK_SE)
qm_gpio_reg_t *qm_gpio[QM_GPIO_NUM] = {(qm_gpio_reg_t *)QM_GPIO_BASE,
				       (qm_gpio_reg_t *)QM_AON_GPIO_BASE};
#elif(QUARK_D2000)
qm_gpio_reg_t *qm_gpio[QM_GPIO_NUM] = {(qm_gpio_reg_t *)QM_GPIO_BASE};
#endif
#endif

static void (*callback[QM_GPIO_NUM])(void *, uint32_t);
static void *callback_data[QM_GPIO_NUM];

static void gpio_isr(const qm_gpio_t gpio)
{
	const uint32_t int_status = QM_GPIO[gpio]->gpio_intstatus;

	if (callback[gpio]) {
		(*callback[gpio])(callback_data[gpio], int_status);
	}

	/* This will clear all pending interrupts flags in status */
	QM_GPIO[gpio]->gpio_porta_eoi = int_status;
	/* Read back EOI register to avoid a spurious interrupt due to EOI
	 * propagation delay */
	QM_GPIO[gpio]->gpio_porta_eoi;
}

QM_ISR_DECLARE(qm_gpio_isr_0)
{
	gpio_isr(QM_GPIO_0);
	QM_ISR_EOI(QM_IRQ_GPIO_0_VECTOR);
}

#if (HAS_AON_GPIO)
QM_ISR_DECLARE(qm_aon_gpio_isr_0)
{
	gpio_isr(QM_AON_GPIO_0);
	QM_ISR_EOI(QM_IRQ_AONGPIO_0_VECTOR);
}
#endif

int qm_gpio_set_config(const qm_gpio_t gpio,
		       const qm_gpio_port_config_t *const cfg)
{
	QM_CHECK(gpio < QM_GPIO_NUM, -EINVAL);
	QM_CHECK(cfg != NULL, -EINVAL);

	qm_gpio_reg_t *const controller = QM_GPIO[gpio];

	uint32_t mask = controller->gpio_intmask;
	controller->gpio_intmask = 0xffffffff;

	controller->gpio_swporta_ddr = cfg->direction;
	controller->gpio_inten = cfg->int_en;
	controller->gpio_inttype_level = cfg->int_type;
	controller->gpio_int_polarity = cfg->int_polarity;
	controller->gpio_debounce = cfg->int_debounce;
	controller->gpio_int_bothedge = cfg->int_bothedge;
	callback[gpio] = cfg->callback;
	callback_data[gpio] = cfg->callback_data;

	controller->gpio_intmask = mask;

	return 0;
}

int qm_gpio_read_pin(const qm_gpio_t gpio, const uint8_t pin,
		     qm_gpio_state_t *const state)
{
	QM_CHECK(gpio < QM_GPIO_NUM, -EINVAL);
	QM_CHECK(pin <= QM_NUM_GPIO_PINS, -EINVAL);
	QM_CHECK(state != NULL, -EINVAL);

	*state = ((QM_GPIO[gpio]->gpio_ext_porta) >> pin) & 1;

	return 0;
}

int qm_gpio_set_pin(const qm_gpio_t gpio, const uint8_t pin)
{
	QM_CHECK(gpio < QM_GPIO_NUM, -EINVAL);
	QM_CHECK(pin <= QM_NUM_GPIO_PINS, -EINVAL);

	QM_GPIO[gpio]->gpio_swporta_dr |= (1 << pin);

	return 0;
}

int qm_gpio_clear_pin(const qm_gpio_t gpio, const uint8_t pin)
{
	QM_CHECK(gpio < QM_GPIO_NUM, -EINVAL);
	QM_CHECK(pin <= QM_NUM_GPIO_PINS, -EINVAL);

	QM_GPIO[gpio]->gpio_swporta_dr &= ~(1 << pin);

	return 0;
}

int qm_gpio_set_pin_state(const qm_gpio_t gpio, const uint8_t pin,
			  const qm_gpio_state_t state)
{
	QM_CHECK(gpio < QM_GPIO_NUM, -EINVAL);
	QM_CHECK(pin <= QM_NUM_GPIO_PINS, -EINVAL);
	QM_CHECK(state < QM_GPIO_STATE_NUM, -EINVAL);

	uint32_t reg = QM_GPIO[gpio]->gpio_swporta_dr;
	reg ^= (-state ^ reg) & (1 << pin);
	QM_GPIO[gpio]->gpio_swporta_dr = reg;

	return 0;
}

int qm_gpio_read_port(const qm_gpio_t gpio, uint32_t *const port)
{
	QM_CHECK(gpio < QM_GPIO_NUM, -EINVAL);
	QM_CHECK(port != NULL, -EINVAL);

	*port = QM_GPIO[gpio]->gpio_ext_porta;

	return 0;
}

int qm_gpio_write_port(const qm_gpio_t gpio, const uint32_t val)
{
	QM_CHECK(gpio < QM_GPIO_NUM, -EINVAL);

	QM_GPIO[gpio]->gpio_swporta_dr = val;

	return 0;
}
