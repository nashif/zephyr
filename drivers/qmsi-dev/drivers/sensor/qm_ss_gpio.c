/*
 * {% copyright %}
 */

#include "qm_ss_gpio.h"

static void (*callback[QM_SS_GPIO_NUM])(void *data, uint32_t int_status);
static void *callback_data[QM_SS_GPIO_NUM];

static uint32_t gpio_base[QM_SS_GPIO_NUM] = {QM_SS_GPIO_0_BASE,
					     QM_SS_GPIO_1_BASE};

static void ss_gpio_isr_handler(qm_ss_gpio_t gpio)
{
	uint32_t int_status = 0;
	uint32_t controller = gpio_base[gpio];

	int_status = __builtin_arc_lr(controller + QM_SS_GPIO_INTSTATUS);

	if (callback[gpio]) {
		callback[gpio](callback_data, int_status);
	}

	__builtin_arc_sr(int_status, controller + QM_SS_GPIO_PORTA_EOI);
}

QM_ISR_DECLARE(qm_ss_gpio_isr_0)
{
	ss_gpio_isr_handler(QM_SS_GPIO_0);
}

QM_ISR_DECLARE(qm_ss_gpio_isr_1)
{
	ss_gpio_isr_handler(QM_SS_GPIO_1);
}

int qm_ss_gpio_set_config(const qm_ss_gpio_t gpio,
			  const qm_ss_gpio_port_config_t *const cfg)
{
	uint32_t controller;
	QM_CHECK(gpio < QM_SS_GPIO_NUM, -EINVAL);
	QM_CHECK(cfg != NULL, -EINVAL);

	controller = gpio_base[gpio];
	__builtin_arc_sr(0xFFFFFFFF, controller + QM_SS_GPIO_INTMASK);

	__builtin_arc_sr(cfg->direction, controller + QM_SS_GPIO_SWPORTA_DDR);
	__builtin_arc_sr(cfg->int_type, controller + QM_SS_GPIO_INTTYPE_LEVEL);
	__builtin_arc_sr(cfg->int_polarity,
			 controller + QM_SS_GPIO_INT_POLARITY);
	__builtin_arc_sr(cfg->int_debounce, controller + QM_SS_GPIO_DEBOUNCE);

	callback[gpio] = cfg->callback;
	callback_data[gpio] = cfg->callback_data;
	__builtin_arc_sr(cfg->int_en, controller + QM_SS_GPIO_INTEN);

	__builtin_arc_sr(~cfg->int_en, controller + QM_SS_GPIO_INTMASK);

	return 0;
}

int qm_ss_gpio_read_pin(const qm_ss_gpio_t gpio, const uint8_t pin,
			qm_ss_gpio_state_t *const state)
{
	QM_CHECK(gpio < QM_SS_GPIO_NUM, -EINVAL);
	QM_CHECK(pin <= QM_SS_GPIO_NUM_PINS, -EINVAL);
	QM_CHECK(state != NULL, -EINVAL);

	*state =
	    ((__builtin_arc_lr(gpio_base[gpio] + QM_SS_GPIO_EXT_PORTA) >> pin) &
	     1);

	return 0;
}

int qm_ss_gpio_set_pin(const qm_ss_gpio_t gpio, const uint8_t pin)
{
	uint32_t val;
	QM_CHECK(gpio < QM_SS_GPIO_NUM, -EINVAL);
	QM_CHECK(pin <= QM_SS_GPIO_NUM_PINS, -EINVAL);

	val = __builtin_arc_lr(gpio_base[gpio] + QM_SS_GPIO_SWPORTA_DR) |
	      BIT(pin);
	__builtin_arc_sr(val, gpio_base[gpio] + QM_SS_GPIO_SWPORTA_DR);

	return 0;
}

int qm_ss_gpio_clear_pin(const qm_ss_gpio_t gpio, const uint8_t pin)
{
	uint32_t val;
	QM_CHECK(gpio < QM_SS_GPIO_NUM, -EINVAL);
	QM_CHECK(pin <= QM_SS_GPIO_NUM_PINS, -EINVAL);

	val = __builtin_arc_lr(gpio_base[gpio] + QM_SS_GPIO_SWPORTA_DR);
	val &= ~BIT(pin);
	__builtin_arc_sr(val, gpio_base[gpio] + QM_SS_GPIO_SWPORTA_DR);

	return 0;
}

int qm_ss_gpio_set_pin_state(const qm_ss_gpio_t gpio, const uint8_t pin,
			 const qm_ss_gpio_state_t state)
{
	uint32_t val;
	QM_CHECK(gpio < QM_SS_GPIO_NUM, -EINVAL);
	QM_CHECK(state < QM_SS_GPIO_STATE_NUM, -EINVAL);

	val = __builtin_arc_lr(gpio_base[gpio] + QM_SS_GPIO_SWPORTA_DR);
	val ^= (-state ^ val) & (1 << pin);
	__builtin_arc_sr(val, gpio_base[gpio] + QM_SS_GPIO_SWPORTA_DR);

	return 0;
}

int qm_ss_gpio_read_port(const qm_ss_gpio_t gpio, uint32_t *const port)
{
	QM_CHECK(gpio < QM_SS_GPIO_NUM, -EINVAL);
	QM_CHECK(port != NULL, -EINVAL);

	*port = (__builtin_arc_lr(gpio_base[gpio] + QM_SS_GPIO_EXT_PORTA));

	return 0;
}

int qm_ss_gpio_write_port(const qm_ss_gpio_t gpio, const uint32_t val)
{
	QM_CHECK(gpio < QM_SS_GPIO_NUM, -EINVAL);

	__builtin_arc_sr(val, gpio_base[gpio] + QM_SS_GPIO_SWPORTA_DR);

	return 0;
}
