/*
 * {% copyright %}
 */

#include "qm_pinmux.h"
#include "qm_common.h"

#define MASK_1BIT (0x1)
#define MASK_2BIT (0x3)

/**
 * Calculate the register index for a specific pin.
 *
 * @param[in] pin The pin to be used.
 * @param[in] width The width in bits for each pin in the register.
 *
 * @return The register index of the given pin.
 */
static uint32_t pin_to_register(uint32_t pin, uint32_t width)
{
	return (pin / (32 / width));
}

/**
 * Calculate the offset for a pin within a register.
 *
 * @param[in] pin The pin to be used.
 * @param[in] width The width in bits for each pin in the register.
 *
 * @return The offset for the pin within the register.
 */
static uint32_t pin_to_offset(uint32_t pin, uint32_t width)
{
	return ((pin % (32 / width)) * width);
}

int qm_pmux_select(const qm_pin_id_t pin, const qm_pmux_fn_t fn)
{
	QM_CHECK(pin < QM_PIN_ID_NUM, -EINVAL);
	QM_CHECK(fn <= QM_PMUX_FN_3, -EINVAL);

	uint32_t reg = pin_to_register(pin, 2);
	uint32_t offs = pin_to_offset(pin, 2);

	QM_SCSS_PMUX->pmux_sel[reg] &= ~(MASK_2BIT << offs);
	QM_SCSS_PMUX->pmux_sel[reg] |= (fn << offs);

	return 0;
}

int qm_pmux_set_slew(const qm_pin_id_t pin, const qm_pmux_slew_t slew)
{
	QM_CHECK(pin < QM_PIN_ID_NUM, -EINVAL);
	QM_CHECK(slew < QM_PMUX_SLEW_NUM, -EINVAL);

	uint32_t reg = pin_to_register(pin, 1);
	uint32_t mask = MASK_1BIT << pin_to_offset(pin, 1);

	if (slew == 0) {
		QM_SCSS_PMUX->pmux_slew[reg] &= ~mask;
	} else {
		QM_SCSS_PMUX->pmux_slew[reg] |= mask;
	}
	return 0;
}

int qm_pmux_input_en(const qm_pin_id_t pin, const bool enable)
{
	QM_CHECK(pin < QM_PIN_ID_NUM, -EINVAL);

	uint32_t reg = pin_to_register(pin, 1);
	uint32_t mask = MASK_1BIT << pin_to_offset(pin, 1);

	if (enable == false) {
		QM_SCSS_PMUX->pmux_in_en[reg] &= ~mask;
	} else {
		QM_SCSS_PMUX->pmux_in_en[reg] |= mask;
	}
	return 0;
}

int qm_pmux_pullup_en(const qm_pin_id_t pin, const bool enable)
{
	QM_CHECK(pin < QM_PIN_ID_NUM, -EINVAL);

	uint32_t reg = pin_to_register(pin, 1);
	uint32_t mask = MASK_1BIT << pin_to_offset(pin, 1);

	if (enable == false) {
		QM_SCSS_PMUX->pmux_pullup[reg] &= ~mask;
	} else {
		QM_SCSS_PMUX->pmux_pullup[reg] |= mask;
	}
	return 0;
}
