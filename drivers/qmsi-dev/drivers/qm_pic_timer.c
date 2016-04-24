/*
 * {% copyright %}
 */

#include "qm_pic_timer.h"

/*
 * PIC timer access layer. Supports both Local APIC timer and MVIC timer.
 *
 * The MVIC timer differs from the LAPIC specs in that:
 * - it does not support TSC deadline mode
 * - vector table lvttimer[3:0] holds the IRQ (not the vector) of the timer
 */

#define LVTTIMER_MODE_PERIODIC_OFFS (17)
#define LVTTIMER_INT_MASK_OFFS (16)

static void (*callback)(void *data);
static void *callback_data;

#if (HAS_APIC)
#define PIC_TIMER (QM_LAPIC)
#else
#define PIC_TIMER (QM_PIC_TIMER)
#endif

QM_ISR_DECLARE(qm_pic_timer_isr)
{
	if (callback) {
		callback(callback_data);
	}

#if (HAS_APIC)
	/* Use an invalid vector to avoid acknowledging a valid IRQ */
	QM_ISR_EOI(0);
#else
	QM_ISR_EOI(QM_IRQ_PIC_TIMER_VECTOR);
#endif
}

int qm_pic_timer_set_config(const qm_pic_timer_config_t *const cfg)
{
	QM_CHECK(cfg != NULL, -EINVAL);
	QM_CHECK(cfg->mode <= QM_PIC_TIMER_MODE_PERIODIC, -EINVAL);

	/* Stop timer, mask interrupt and program interrupt vector */
	PIC_TIMER->timer_icr.reg = 0;
	PIC_TIMER->lvttimer.reg = BIT(LVTTIMER_INT_MASK_OFFS) |
#if (HAS_APIC)
				  QM_INT_VECTOR_PIC_TIMER;
#else
				  QM_IRQ_PIC_TIMER;
#endif

#if (HAS_APIC)
	/* LAPIC has a timer clock divisor, POR default: 2. Set it to 1. */
	QM_LAPIC->timer_dcr.reg = 0xB;
#endif

	PIC_TIMER->lvttimer.reg |= cfg->mode << LVTTIMER_MODE_PERIODIC_OFFS;
	callback = cfg->callback;
	callback_data = cfg->callback_data;
	if (cfg->int_en) {
		PIC_TIMER->lvttimer.reg &= ~BIT(LVTTIMER_INT_MASK_OFFS);
	}
	return 0;
}

int qm_pic_timer_set(const uint32_t count)
{
	PIC_TIMER->timer_icr.reg = count;

	return 0;
}

int qm_pic_timer_get(uint32_t *const count)
{
	QM_CHECK(count != NULL, -EINVAL);

	*count = PIC_TIMER->timer_ccr.reg;

	return 0;
}
