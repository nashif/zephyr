/*
 * Copyright (c) 2013-2015, Wind River Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * @file
 * @brief  system module for variants with LOAPIC
 *
 * This module provides routines to initialize and support
 * board-level hardware for the basic_atom configuration of
 * ia32 platform.
 */

#include <misc/__assert.h>
#include "board.h"
#include <nanokernel.h>
#include <arch/cpu.h>
#include <drivers/ioapic.h>
#include <drivers/loapic.h>

/**
 *
 * @brief Allocate interrupt vector
 *
 * This routine is used by the x86's irq_connect().  It performs the following
 * functions:
 *
 *  a) Allocates a vector satisfying the requested priority.  The utility
 *     routine _IntVecAlloc() provided by the nanokernel will be used to
 *     perform the the allocation since the local APIC prioritizes interrupts
 *     as assumed by _IntVecAlloc().
 *  b) Provides End of Interrupt (EOI) and Beginning of Interrupt (BOI) related
 *     information to be used when generating the interrupt stub code.
 *  c) If an interrupt vector can be allocated, and the <irq> argument is not
 *     equal to NANO_SOFT_IRQ, the IOAPIC redirection table (RED) or the
 *     LOAPIC local vector table (LVT) will be updated with the allocated
 *     interrupt vector.
 *
 * The board virtualizes IRQs as follows:
 *
 * - The first CONFIG_IOAPIC_NUM_RTES IRQs are provided by the IOAPIC
 * - The remaining IRQs are provided by the LOAPIC.
 *
 * Thus, for example, if the IOAPIC supports 24 IRQs:
 *
 * - IRQ0 to IRQ23   map to IOAPIC IRQ0 to IRQ23
 * - IRQ24 to IRQ29  map to LOAPIC LVT entries as follows:
 *
 *       IRQ24 -> LOAPIC_TIMER
 *       IRQ25 -> LOAPIC_THERMAL
 *       IRQ26 -> LOAPIC_PMC
 *       IRQ27 -> LOAPIC_LINT0
 *       IRQ28 -> LOAPIC_LINT1
 *       IRQ29 -> LOAPIC_ERROR
 *
 * @return the allocated interrupt vector
 *
 * @internal
 * For debug kernels, this routine will return -1 if there are no vectors
 * remaining in the specified <priority> level, or if the <priority> or <irq>
 * parameters are invalid.
 * @endinternal
 */
int _SysIntVecAlloc(
	unsigned int irq,		 /* virtualized IRQ */
	unsigned int priority,		 /* get vector from <priority> group */
	NANO_EOI_GET_FUNC * boiRtn,       /* ptr to BOI routine; NULL if none */
	NANO_EOI_GET_FUNC * eoiRtn,       /* ptr to EOI routine; NULL if none */
	void **boiRtnParm,		 /* BOI routine parameter, if any */
	void **eoiRtnParm,		 /* EOI routine parameter, if any */
	unsigned char *boiParamRequired, /* BOI routine parameter req? */
	unsigned char *eoiParamRequired  /* BOI routine parameter req? */
	)
{
	int vector;

	ARG_UNUSED(boiRtnParm);
	ARG_UNUSED(boiParamRequired);

#if defined(CONFIG_LOAPIC_DEBUG)
	if ((priority > 15) ||
	    ((irq > (CONFIG_IOAPIC_NUM_RTES + 5)) && (irq != NANO_SOFT_IRQ)))
		return -1;
#endif

	/*
	 * Use the nanokernel utility function _IntVecAlloc().  A value of
	 * -1 will be returned if there are no free vectors in the requested
	 * priority.
	 */

	vector = _IntVecAlloc(priority);
	__ASSERT(vector != -1, "No free vectors in the requested priority");

	/*
	 * Set up the appropriate interrupt controller to generate the allocated
	 * interrupt vector for the specified IRQ.  Also, provide the required
	 * EOI and BOI related information for the interrupt stub code
	 *generation
	 * step.
	 *
	 * For software interrupts (NANO_SOFT_IRQ), skip the interrupt
	 *controller
	 * programming step, and indicate that a BOI and EOI handler is not
	 * required.
	 *
	 * Skip both steps if a vector could not be allocated.
	 */

	*boiRtn = (NANO_EOI_GET_FUNC)NULL; /* a BOI handler is never required */
	*eoiRtn = (NANO_EOI_GET_FUNC)NULL; /* assume NANO_SOFT_IRQ */

#if defined(CONFIG_LOAPIC_DEBUG)
	if ((vector != -1) && (irq != NANO_SOFT_IRQ))
#else
	if (irq != NANO_SOFT_IRQ)
#endif
	{
		if (irq < CONFIG_IOAPIC_NUM_RTES) {
			_ioapic_int_vec_set(irq, vector);

			/*
			 * query IOAPIC driver to obtain EOI handler information
			 * for the
			 * interrupt vector that was just assigned to the
			 * specified IRQ
			 */

			*eoiRtn = (NANO_EOI_GET_FUNC)_ioapic_eoi_get(
				irq, (char *)eoiParamRequired, eoiRtnParm);
		} else {
			_loapic_int_vec_set(irq - CONFIG_IOAPIC_NUM_RTES, vector);

			/* specify that the EOI handler in loApicIntr.c driver
			 * be invoked */

			*eoiRtn = (NANO_EOI_GET_FUNC)_loapic_eoi;
			*eoiParamRequired = 0;
		}
	}

	return vector;
}

/**
 *
 * @brief Program interrupt controller
 *
 * This routine programs the interrupt controller with the given vector
 * based on the given IRQ parameter.
 *
 * Drivers call this routine instead of irq_connect() when interrupts are
 * configured statically.
 *
 * The Clanton board virtualizes IRQs as follows:
 *
 * - The first CONFIG_IOAPIC_NUM_RTES IRQs are provided by the IOAPIC so the
 *     IOAPIC is programmed for these IRQs
 * - The remaining IRQs are provided by the LOAPIC and hence the LOAPIC is
 *     programmed.
 */
void _SysIntVecProgram(unsigned int vector, /* vector number */
		       unsigned int irq     /* virtualized IRQ */
		       )
{

	if (irq < CONFIG_IOAPIC_NUM_RTES) {
		_ioapic_int_vec_set(irq, vector);
	} else {
		_loapic_int_vec_set(irq - CONFIG_IOAPIC_NUM_RTES, vector);
	}
}


/**
 *
 * @brief Enable an individual interrupt (IRQ)
 *
 * The public interface for enabling/disabling a specific IRQ for the IA-32
 * architecture is defined as follows in include/arch/x86/arch.h
 *
 *   extern void  irq_enable  (unsigned int irq);
 *   extern void  irq_disable (unsigned int irq);
 *
 * The irq_enable() routine is provided by the interrupt controller driver due
 * to the IRQ virtualization that is performed by this platform.  See the
 * comments in _SysIntVecAlloc() for more information regarding IRQ
 * virtualization.
 *
 * @return N/A
 */
void irq_enable(unsigned int irq)
{
	if (irq < CONFIG_IOAPIC_NUM_RTES) {
		_ioapic_irq_enable(irq);
	} else {
		_loapic_irq_enable(irq - CONFIG_IOAPIC_NUM_RTES);
	}
}

/**
 *
 * @brief Disable an individual interrupt (IRQ)
 *
 * The irq_disable() routine is provided by the interrupt controller driver due
 * to the IRQ virtualization that is performed by this platform.  See the
 * comments in _SysIntVecAlloc() for more information regarding IRQ
 * virtualization.
 *
 * @return N/A
 */
void irq_disable(unsigned int irq)
{
	if (irq < CONFIG_IOAPIC_NUM_RTES) {
		_ioapic_irq_disable(irq);
	} else {
		_loapic_irq_disable(irq - CONFIG_IOAPIC_NUM_RTES);
	}
}
