/*
 * {% copyright %}
 */

#ifndef __QM_SS_INTERRUPT_H__
#define __QM_SS_INTERRUPT_H__

#include "qm_common.h"
#include "qm_sensor_regs.h"

/**
 * Interrupt driver for Sensor Subsystem.
 *
 * @defgroup groupSSINT SS Interrupt
 * @{
 */

/**
 * Interrupt service routine type.
 */
typedef void (*qm_ss_isr_t)(struct interrupt_frame *frame);

/**
 * Enable interrupt delivery for the Sensor Subsystem.
 */
void qm_ss_irq_enable(void);

/**
 * Disable interrupt delivery for the Sensor Subsystem.
 */
void qm_ss_irq_disable(void);

/**
 * Unmask a given interrupt line.
 *
 * @param [in] irq Which IRQ to unmask.
 */
void qm_ss_irq_unmask(uint32_t irq);

/**
 * Mask a given interrupt line.
 *
 * @param [in] irq Which IRQ to mask.
 */
void qm_ss_irq_mask(uint32_t irq);

/**
 * Request a given IRQ and register ISR to interrupt vector.
 *
 * @param [in] irq IRQ number.
 * @param [in] isr ISR to register to given IRQ.
 */
void qm_ss_irq_request(uint32_t irq, qm_ss_isr_t isr);

/**
 * Register an Interrupt Service Routine to a given interrupt vector.
 *
 * @param [in] vector Interrupt Vector number.
 * @param [in] isr ISR to register to given vector. Must be a valid Sensor
 *             Subsystem ISR.
 */
void qm_ss_int_vector_request(uint32_t vector, qm_ss_isr_t isr);

/**
 * @}
 */
#endif /* __QM_SS_INTERRUPT_H__ */
