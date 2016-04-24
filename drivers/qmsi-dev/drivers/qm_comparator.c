/*
 * {% copyright %}
 */

#include "qm_comparator.h"

static void (*callback)(void *, uint32_t) = NULL;
static void *callback_data;

QM_ISR_DECLARE(qm_ac_isr)
{
	uint32_t int_status = QM_SCSS_CMP->cmp_stat_clr;

#if (QUARK_D2000)
	/*
	 * If the SoC is in deep sleep mode, all the clocks are gated, if the
	 * interrupt source is cleared before the oscillators are ungated, the
	 * oscillators return to a powered down state and the SoC will not
	 * return to an active state then.
	 */
	if ((QM_SCSS_GP->gps1 & QM_SCSS_GP_POWER_STATES_MASK) ==
	    QM_SCSS_GP_POWER_STATE_DEEP_SLEEP) {
		/* Return the oscillators to an active state. */
		QM_SCSS_CCU->osc0_cfg1 &= ~QM_OSC0_PD;
		QM_SCSS_CCU->osc1_cfg0 &= ~QM_OSC1_PD;

		/* HYB_OSC_PD_LATCH_EN = 1, RTC_OSC_PD_LATCH_EN=1 */
		QM_SCSS_CCU->ccu_lp_clk_ctl |=
		    (QM_HYB_OSC_PD_LATCH_EN | QM_RTC_OSC_PD_LATCH_EN);
	}
#endif
	if (callback) {
		(*callback)(callback_data, int_status);
	}

	/* Clear all pending interrupts */
	QM_SCSS_CMP->cmp_stat_clr = int_status;

	QM_ISR_EOI(QM_IRQ_AC_VECTOR);
}

int qm_ac_set_config(const qm_ac_config_t *const config)
{
	QM_CHECK(config != NULL, -EINVAL);

	callback = config->callback;
	callback_data = config->callback_data;
	QM_SCSS_CMP->cmp_ref_sel = config->reference;
	QM_SCSS_CMP->cmp_ref_pol = config->polarity;
	QM_SCSS_CMP->cmp_pwr = config->power;

	/* Clear all pending interrupts before we enable */
	QM_SCSS_CMP->cmp_stat_clr = 0x7FFFF;
	QM_SCSS_CMP->cmp_en = config->int_en;

	return 0;
}
