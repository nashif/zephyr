/*
 * {% copyright %}
 */

#include "qm_ss_timer.h"

static void (*callback[QM_SS_TIMER_NUM])(void *data);
static void *callback_data[QM_SS_TIMER_NUM];
static uint32_t qm_ss_timer_base[QM_SS_TIMER_NUM] = {QM_SS_TIMER_0_BASE};

static __inline__ void qm_ss_timer_isr(qm_ss_timer_t timer)
{
	uint32_t ctrl = 0;

	if (callback[timer]) {
		callback[timer](callback_data[timer]);
	}

	ctrl = __builtin_arc_lr(qm_ss_timer_base[timer] + QM_SS_TIMER_CONTROL);
	ctrl &= ~BIT(QM_SS_TIMER_CONTROL_INT_PENDING_OFFSET);
	__builtin_arc_sr(ctrl, qm_ss_timer_base[timer] + QM_SS_TIMER_CONTROL);
}

QM_ISR_DECLARE(qm_ss_timer_isr_0)
{
	qm_ss_timer_isr(QM_SS_TIMER_0);
}

int qm_ss_timer_set_config(const qm_ss_timer_t timer,
			   const qm_ss_timer_config_t *const cfg)
{
	uint32_t ctrl = 0;
	QM_CHECK(cfg != NULL, -EINVAL);
	QM_CHECK(timer < QM_SS_TIMER_NUM, -EINVAL);

	ctrl = cfg->watchdog_mode << QM_SS_TIMER_CONTROL_WATCHDOG_OFFSET;
	ctrl |= cfg->inc_run_only << QM_SS_TIMER_CONTROL_NON_HALTED_OFFSET;
	ctrl |= cfg->int_en << QM_SS_TIMER_CONTROL_INT_EN_OFFSET;

	__builtin_arc_sr(ctrl, qm_ss_timer_base[timer] + QM_SS_TIMER_CONTROL);
	__builtin_arc_sr(cfg->count,
			 qm_ss_timer_base[timer] + QM_SS_TIMER_LIMIT);

	callback[timer] = cfg->callback;
	callback_data[timer] = cfg->callback_data;

	return 0;
}

int qm_ss_timer_set(const qm_ss_timer_t timer, const uint32_t count)
{
	QM_CHECK(timer < QM_SS_TIMER_NUM, -EINVAL);

	__builtin_arc_sr(count, qm_ss_timer_base[timer] + QM_SS_TIMER_COUNT);

	return 0;
}

int qm_ss_timer_get(const qm_ss_timer_t timer, uint32_t *const count)
{
	QM_CHECK(timer < QM_SS_TIMER_NUM, -EINVAL);
	QM_CHECK(count != NULL, -EINVAL);

	*count = __builtin_arc_lr(qm_ss_timer_base[timer] + QM_SS_TIMER_COUNT);

	return 0;
}
