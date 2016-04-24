/*
 * {% copyright %}
 */

#include "qm_ss_interrupt.h"
#include "qm_soc_regs.h"
#include "qm_sensor_regs.h"

/* SCSS base addr for Sensor Subsystem interrupt routing, for linear IRQ
 * mapping */
#define SCSS_SS_INT_MASK_BASE (&QM_SCSS_INT->int_ss_adc_err_mask)
#define SCSS_SS_INT_MASK BIT(8) /* Sensor Subsystem interrupt masking */

#if (UNIT_TEST)
qm_ss_isr_t __ivt_vect_table[QM_SS_INT_VECTOR_NUM];
#else
extern qm_ss_isr_t __ivt_vect_table[];
#endif

void qm_ss_irq_disable(void)
{
	__builtin_arc_clri();
}

void qm_ss_irq_enable(void)
{
	__builtin_arc_seti(0);
}

void qm_ss_irq_mask(uint32_t irq)
{
	__builtin_arc_sr(irq, QM_SS_AUX_IRQ_SELECT);
	__builtin_arc_sr(QM_SS_INT_DISABLE, QM_SS_AUX_IRQ_ENABLE);
}

void qm_ss_irq_unmask(uint32_t irq)
{
	__builtin_arc_sr(irq, QM_SS_AUX_IRQ_SELECT);
	__builtin_arc_sr(QM_SS_INT_ENABLE, QM_SS_AUX_IRQ_ENABLE);
}

void qm_ss_int_vector_request(uint32_t vector, qm_ss_isr_t isr)
{
	/* Invalidate the I-cache line which contains the irq vector. This
	 * will bypass I-Cach and set vector with the good isr. */
	__builtin_arc_sr((uint32_t)&__ivt_vect_table[0] + (vector * 4),
			 QM_SS_AUX_IC_IVIL);
	/* All SR accesses to the IC_IVIL register must be followed by three
	 * NOP instructions, see chapter 3.3.59 in the datasheet
	 * "ARC_V2_ProgrammersReference.pdf" */
	__builtin_arc_nop();
	__builtin_arc_nop();
	__builtin_arc_nop();
	__ivt_vect_table[vector] = isr;
}

void qm_ss_irq_request(uint32_t irq, qm_ss_isr_t isr)
{
	uint32_t *scss_intmask;
	uint32_t vector = irq + (QM_SS_EXCEPTION_NUM + QM_SS_INT_TIMER_NUM);

	/* Guarding the IRQ set-up */
	qm_ss_irq_mask(vector);

	qm_ss_int_vector_request(vector, isr);

	/* Route peripheral interrupt to Sensor Subsystem */
	scss_intmask = (uint32_t *)SCSS_SS_INT_MASK_BASE + irq;
	*scss_intmask &= ~SCSS_SS_INT_MASK;

	qm_ss_irq_unmask(vector);
}
