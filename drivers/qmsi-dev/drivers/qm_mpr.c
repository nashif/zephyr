/*
 * {% copyright %}
 */

#include "qm_mpr.h"
#include "qm_interrupt.h"

#define ADDRESS_MASK_7_BIT (0x7F)

static void (*callback)(void *data);
static void *callback_data;

QM_ISR_DECLARE(qm_mpr_isr)
{
	if (callback) {
		(*callback)(callback_data);
	}
	QM_MPR->mpr_vsts = QM_MPR_VSTS_VALID;

	QM_ISR_EOI(QM_IRQ_SRAM_VECTOR);
}

int qm_mpr_set_config(const qm_mpr_id_t id, const qm_mpr_config_t *const cfg)
{
	QM_CHECK(id < QM_MPR_NUM, -EINVAL);
	QM_CHECK(cfg != NULL, -EINVAL);

	QM_MPR->mpr_cfg[id] &= ~QM_MPR_EN_LOCK_MASK;

	QM_MPR->mpr_cfg[id] =
	    (cfg->agent_write_en_mask << QM_MPR_WR_EN_OFFSET) |
	    (cfg->agent_read_en_mask << QM_MPR_RD_EN_OFFSET) |
	    /*   MPR Upper bound 16:10 */
	    ((cfg->up_bound & ADDRESS_MASK_7_BIT) << QM_MPR_UP_BOUND_OFFSET)
	    /*   MPR Lower bound 6:0 */
	    |
	    cfg->low_bound;

	/* enable/lock */
	QM_MPR->mpr_cfg[id] |= (cfg->en_lock_mask << QM_MPR_EN_LOCK_OFFSET);
	return 0;
}

int qm_mpr_set_violation_policy(const qm_mpr_viol_mode_t mode,
				qm_mpr_callback_t callback_fn,
				void *callback_data)
{
	QM_CHECK(mode <= MPR_VIOL_MODE_PROBE, -EINVAL);
	/*  interrupt mode */
	if (MPR_VIOL_MODE_INTERRUPT == mode) {
		callback = callback_fn;
		callback_data = callback_data;

		/* unmask interrupt */
		qm_irq_unmask(QM_IRQ_SRAM);
#if defined(QM_SENSOR)
		QM_SCSS_INT->int_sram_controller_mask |=
		    QM_INT_SRAM_CONTROLLER_SS_HALT_MASK;
#else  /* QM_SENSOR */
		QM_SCSS_INT->int_sram_controller_mask |=
		    QM_INT_SRAM_CONTROLLER_HOST_HALT_MASK;
#endif /* QM_SENSOR */
	}

	/* probe or reset mode */
	else {
		/* mask interrupt */
		qm_irq_mask(QM_IRQ_SRAM);
#if defined(QM_SENSOR)
		QM_SCSS_INT->int_sram_controller_mask &=
		    ~QM_INT_SRAM_CONTROLLER_SS_HALT_MASK;
#else  /* QM_SENSOR */
		QM_SCSS_INT->int_sram_controller_mask &=
		    ~QM_INT_SRAM_CONTROLLER_HOST_HALT_MASK;
#endif /* QM_SENSOR */

		if (MPR_VIOL_MODE_PROBE == mode) {

			/* When an enabled host halt interrupt occurs, this bit
			* determines if the interrupt event triggers a warm
			* reset
			* or an entry into Probe Mode.
			* 0b : Warm Reset
			* 1b : Probe Mode Entry
			*/
			QM_SCSS_PMU->p_sts |=
			    QM_P_STS_HALT_INTERRUPT_REDIRECTION;
		}

		else {
			QM_SCSS_PMU->p_sts &=
			    ~QM_P_STS_HALT_INTERRUPT_REDIRECTION;
		}
	}
	return 0;
}
