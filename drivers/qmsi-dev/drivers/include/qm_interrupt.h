/*
 * {% copyright %}
 */

#ifndef __QM_INTERRUPT_H__
#define __QM_INTERRUPT_H__

#include "qm_common.h"
#include "qm_soc_regs.h"

/*
 * Linear mapping between IRQs and interrupt vectors
 */
#if (QUARK_SE)
#define QM_IRQ_TO_VECTOR(irq) (irq + 36) /**<  Get the vector of and irq. */

#elif(QUARK_D2000)
#define QM_IRQ_TO_VECTOR(irq) (irq + 32) /**<  Get the vector of and irq. */

#endif

/**
 * Interrupt driver.
 *
 * @defgroup groupINT Interrupt
 * @{
 */

/**
 * Interrupt service routine type
 */
typedef void (*qm_isr_t)(struct interrupt_frame *frame);

/**
 * Enable interrupt delivery for the SoC.
 */
void qm_irq_enable(void);

/**
 * Disable interrupt delivery for the SoC.
 */
void qm_irq_disable(void);

/**
 * Unmask a given interrupt line.
 *
 * @param[in] irq Which IRQ to unmask.
 */
void qm_irq_unmask(uint32_t irq);

/**
 * Mask a given interrupt line.
 *
 * @param[in] irq Which IRQ to mask.
 */
void qm_irq_mask(uint32_t irq);

void _qm_register_isr(uint32_t vector, qm_isr_t isr);

void _qm_irq_setup(uint32_t irq, uint16_t register_offset);

/**
 * Request a given IRQ and register Interrupt Service Routine to interrupt
 * vector.
 *
 * @param[in] irq IRQ number. Must be of type QM_IRQ_XXX.
 * @param[in] isr ISR to register to given IRQ.
 */
#if (UNIT_TEST)
#define qm_irq_request(irq, isr)
#elif(QM_SENSOR)
#define qm_irq_request(irq, isr)                                               \
	do {                                                                   \
		_qm_register_isr(irq##_VECTOR, isr);                           \
		_qm_irq_setup(irq, irq##_MASK_OFFSET);                         \
	} while (0);
#else
#define qm_irq_request(irq, isr)                                               \
	do {                                                                   \
		qm_int_vector_request(irq##_VECTOR, isr);                      \
                                                                               \
		_qm_irq_setup(irq, irq##_MASK_OFFSET);                         \
	} while (0)
#endif /* UNIT_TEST */

/**
 * Request an interrupt vector and register Interrupt Service Routine to it.
 *
 * @param[in] vector Vector number.
 * @param[in] isr ISR to register to given IRQ.
 */
#if (UNIT_TEST)
#define qm_int_vector_request(vector, isr)
#else
#if (__iamcu__)
/*
 * We assume that if the compiler supports the IAMCU ABI it also
 * supports the 'interrupt' attribute.
 */
static __inline__ void qm_int_vector_request(uint32_t vector, qm_isr_t isr)
{
	_qm_register_isr(vector, isr);
}

#else /* __iamcu__ */

/*
 * Using the standard SysV calling convention. A dummy (NULL in this case)
 * parameter is added to ISR handler, to maintain consistency with the API
 * imposed by the __attribute__((interrupt)) usage.
 */
#define qm_int_vector_request(vector, isr)                                     \
	do {                                                                   \
		__asm__ __volatile__("push $1f\n\t"                            \
				     "push %0\n\t"                             \
				     "call %P1\n\t"                            \
				     "add $8, %%esp\n\t"                       \
				     "jmp 2f\n\t"                              \
				     ".align 4\n\t"                            \
				     "1:\n\t"                                  \
				     "         pushal\n\t"                     \
				     "         push $0x00\n\t"                 \
				     "         call %P2\n\t"                   \
				     "         add $4, %%esp\n\t"              \
				     "         popal\n\t"                      \
				     "         iret\n\t"                       \
				     "2:\n\t" ::"g"(vector),                   \
				     "i"(_qm_register_isr), "i"(isr)           \
				     : "%eax", "%ecx", "%edx");                \
	} while (0)
#endif /* __iamcu__ */
#endif /* UNIT_TEST */

/**
 * @}
 */
#endif /* __QM_INTERRUPT_H__ */
