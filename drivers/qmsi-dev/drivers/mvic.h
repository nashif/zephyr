/*
 * {% copyright %}
 */

#ifndef __MVIC_H__
#define __MVIC_H__

#include <stdint.h>

#include "qm_common.h"
#include "qm_soc_regs.h"

#define NUM_IRQ_LINES (32)

static uint32_t _mvic_get_irq_val(unsigned int irq)
{
	/*  Register Select - select which IRQ line we are configuring
	 *  Bits 0 and 4 are reserved
	 *  So, for IRQ 15 ( 0x01111 ) write 0x101110
	 */
	QM_IOAPIC->ioregsel.reg = ((irq & 0x7) << 1) | ((irq & 0x18) << 2);
	return QM_IOAPIC->iowin.reg;
}

static void _mvic_set_irq_val(unsigned int irq, uint32_t value)
{
	/*  Register Select - select which IRQ line we are configuring
	 *  Bits 0 and 4 are reserved
	 *  So, for IRQ 15 ( 0x01111 ) write 0x101110
	 */
	QM_IOAPIC->ioregsel.reg = ((irq & 0x7) << 1) | ((irq & 0x18) << 2);
	QM_IOAPIC->iowin.reg = value;
}

/**
 * Initialise MVIC.
 */
static __inline__ void mvic_init(void)
{
	uint32_t i;

	for (i = 0; i < NUM_IRQ_LINES; i++) {
		/* Clear up any spurious LAPIC interrupts, each call only
		 * clears one bit.
		 */
		QM_MVIC->eoi.reg = 0;

		/* Mask interrupt */
		_mvic_set_irq_val(i, BIT(16));
	}
}

/**
 * Register IRQ with MVIC.
 *
 * @param irq IRQ to register.
 */
static __inline__ void mvic_register_irq(uint32_t irq)
{
	/* Set IRQ triggering scheme and unmask the line. */

	switch (irq) {
	case QM_IRQ_RTC_0:
	case QM_IRQ_AONPT_0:
	case QM_IRQ_PIC_TIMER:
	case QM_IRQ_WDT_0:
		/* positive edge */
		_mvic_set_irq_val(irq, 0);
		break;
	default:
		/* high level */
		_mvic_set_irq_val(irq, BIT(15));
		break;
	}
}

/**
 * Unmask IRQ with MVIC.
 *
 * @param irq IRQ to unmask.
 */
static __inline__ void mvic_unmask_irq(uint32_t irq)
{
	uint32_t value = _mvic_get_irq_val(irq);

	value &= ~BIT(16);

	_mvic_set_irq_val(irq, value);
}

/**
 * Mask IRQ with MVIC.
 *
 * @param irq IRQ to mask.
 */
static __inline__ void mvic_mask_irq(uint32_t irq)
{
	uint32_t value = _mvic_get_irq_val(irq);

	value |= BIT(16);

	_mvic_set_irq_val(irq, value);
}

#endif /* __MVIC_H__ */
