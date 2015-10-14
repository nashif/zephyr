/* exc.h - exception/interrupt context helpers for Cortex-M CPUs */

/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
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
DESCRIPTION
Exception/interrupt context helpers.
 */

#ifndef _ARM_CORTEXM_ISR__H_
#define _ARM_CORTEXM_ISR__H_

#include <arch/cpu.h>
#include <asm_inline.h>

#ifdef _ASMLANGUAGE

/* nothing */

#else

/**
 *
 * @brief Find out if running in an ISR context
 *
 * The current executing vector is found in the IPSR register. We consider the
 * IRQs (exception 16 and up), and the PendSV and SYSTICK exceptions, to be
 * interrupts. Taking a fault within an exception is also considered in
 * interrupt context.
 *
 * @return 1 if in ISR, 0 if not.
 *
 * \NOMANUAL
 */
static ALWAYS_INLINE int _IsInIsr(void)
{
	uint32_t vector = _IpsrGet();

	/* IRQs + PendSV + SYSTICK are interrupts */
	return (vector > 13) || (vector && _ScbIsNestedExc());
}

/**
 * @brief Setup system exceptions
 *
 * Set exception priorities to conform with the BASEPRI locking mechanism.
 * Set PendSV priority to lowest possible.
 *
 * Enable fault exceptions.
 *
 * @return N/A
 *
 * \NOMANUAL
 */

static ALWAYS_INLINE void _ExcSetup(void)
{
	_ScbExcPrioSet(_EXC_PENDSV, _EXC_PRIO(0xff));
	_ScbExcPrioSet(_EXC_SVC, _EXC_PRIO(0x01));
	_ScbExcPrioSet(_EXC_MPU_FAULT, _EXC_PRIO(0x01));
	_ScbExcPrioSet(_EXC_BUS_FAULT, _EXC_PRIO(0x01));
	_ScbExcPrioSet(_EXC_USAGE_FAULT, _EXC_PRIO(0x01));

	_ScbUsageFaultEnable();
	_ScbBusFaultEnable();
	_ScbMemFaultEnable();
}

#endif /* _ASMLANGUAGE */

#endif /* _ARM_CORTEXM_ISR__H_ */
