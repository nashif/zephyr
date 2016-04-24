/*
 * {% copyright %}
 */

#include "qm_aon_counters.h"

static void (*callback)(void *) = NULL;
static void *callback_data;

static void pt_reset(const qm_scss_aon_t aonc)
{
	static bool first_run = true;
	uint32_t aonc_cfg;

	/* After POR, it is required to wait for one RTC clock cycle before
	 * asserting QM_AONPT_CTRL_RST.  Note the AON counter is enabled with an
	 * initial value of 0 at POR.
	 */
	if (first_run) {
		first_run = false;

		/* Ensure the AON counter is enabled */
		aonc_cfg = QM_SCSS_AON[aonc].aonc_cfg;
		QM_SCSS_AON[aonc].aonc_cfg = BIT(0);

		while (0 == QM_SCSS_AON[aonc].aonc_cnt) {
		}

		QM_SCSS_AON[aonc].aonc_cfg = aonc_cfg;
	}

	QM_SCSS_AON[aonc].aonpt_ctrl |= BIT(1);
}

QM_ISR_DECLARE(qm_aonpt_isr_0)
{
	if (callback) {
		(*callback)(callback_data);
	}

	QM_SCSS_AON[0].aonpt_ctrl |= BIT(0); /* Clear pending interrupts */
	QM_ISR_EOI(QM_IRQ_AONPT_0_VECTOR);
}

int qm_aonc_enable(const qm_scss_aon_t aonc)
{
	QM_CHECK(aonc < QM_SCSS_AON_NUM, -EINVAL);

	QM_SCSS_AON[aonc].aonc_cfg = 0x1;

	return 0;
}

int qm_aonc_disable(const qm_scss_aon_t aonc)
{
	QM_CHECK(aonc < QM_SCSS_AON_NUM, -EINVAL);

	QM_SCSS_AON[aonc].aonc_cfg = 0x0;

	return 0;
}

int qm_aonc_get_value(const qm_scss_aon_t aonc, uint32_t * const val)
{
	QM_CHECK(aonc < QM_SCSS_AON_NUM, -EINVAL);
	QM_CHECK(val != NULL, -EINVAL);

	*val = QM_SCSS_AON[aonc].aonc_cnt;
	return 0;
}

int qm_aonpt_set_config(const qm_scss_aon_t aonc,
			const qm_aonpt_config_t *const cfg)
{
	QM_CHECK(aonc < QM_SCSS_AON_NUM, -EINVAL);
	QM_CHECK(cfg != NULL, -EINVAL);

	QM_SCSS_AON[aonc].aonpt_ctrl |= BIT(0); /* Clear pending interrupts */
	QM_SCSS_AON[aonc].aonpt_cfg = cfg->count;
	if (cfg->int_en) {
		callback = cfg->callback;
		callback_data = cfg->callback_data;
	} else {
		callback = NULL;
	}
	pt_reset(aonc);

	return 0;
}

int qm_aonpt_get_value(const qm_scss_aon_t aonc, uint32_t *const val)
{
	QM_CHECK(aonc < QM_SCSS_AON_NUM, -EINVAL);
	QM_CHECK(val != NULL, -EINVAL);

	*val = QM_SCSS_AON[aonc].aonpt_cnt;
	return 0;
}

int qm_aonpt_get_status(const qm_scss_aon_t aonc, bool *const status)
{
	QM_CHECK(aonc < QM_SCSS_AON_NUM, -EINVAL);
	QM_CHECK(status != NULL, -EINVAL);

	*status = QM_SCSS_AON[aonc].aonpt_stat & BIT(0);
	return 0;
}

int qm_aonpt_clear(const qm_scss_aon_t aonc)
{
	QM_CHECK(aonc < QM_SCSS_AON_NUM, -EINVAL);

	QM_SCSS_AON[aonc].aonpt_ctrl |= BIT(0);

	return 0;
}

int qm_aonpt_reset(const qm_scss_aon_t aonc)
{
	QM_CHECK(aonc < QM_SCSS_AON_NUM, -EINVAL);

	pt_reset(aonc);

	return 0;
}
