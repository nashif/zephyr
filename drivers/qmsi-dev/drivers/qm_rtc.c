/*
 * {% copyright %}
 */

#include "qm_rtc.h"

static void (*callback[QM_RTC_NUM])(void *data);
static void *callback_data[QM_RTC_NUM];

QM_ISR_DECLARE(qm_rtc_isr_0)
{
	/*  Disable RTC interrupt */
	QM_RTC[QM_RTC_0].rtc_ccr &= ~QM_RTC_CCR_INTERRUPT_ENABLE;

	if (callback[QM_RTC_0]) {
		(callback[QM_RTC_0])(callback_data[QM_RTC_0]);
	}

	/*  clear interrupt */
	QM_RTC[QM_RTC_0].rtc_eoi;
	QM_ISR_EOI(QM_IRQ_RTC_0_VECTOR);
}

int qm_rtc_set_config(const qm_rtc_t rtc, const qm_rtc_config_t *const cfg)
{
	QM_CHECK(rtc < QM_RTC_NUM, -EINVAL);
	QM_CHECK(cfg != NULL, -EINVAL);

	/* set rtc divider */
	clk_rtc_set_div(QM_RTC_DIVIDER);

	QM_RTC[rtc].rtc_clr = cfg->init_val;

	/*  clear any pending interrupts */
	QM_RTC[rtc].rtc_eoi;

	callback[rtc] = cfg->callback;
	callback_data[rtc] = cfg->callback_data;

	if (cfg->alarm_en) {
		qm_rtc_set_alarm(rtc, cfg->alarm_val);
	} else {
		/*  Disable RTC interrupt */
		QM_RTC[rtc].rtc_ccr &= ~QM_RTC_CCR_INTERRUPT_ENABLE;
	}

	return 0;
}

int qm_rtc_set_alarm(const qm_rtc_t rtc, const uint32_t alarm_val)
{
	QM_CHECK(rtc < QM_RTC_NUM, -EINVAL);

	/*  Enable RTC interrupt */
	QM_RTC[rtc].rtc_ccr |= QM_RTC_CCR_INTERRUPT_ENABLE;

	/*  set alarm val */
	QM_RTC[rtc].rtc_cmr = alarm_val;

	return 0;
}
