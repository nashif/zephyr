/* drvIntStubs.s - interrupt stubs for Quark SE board */

/*
 * Copyright (c) 2015 Wind River Systems, Inc.
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
This module contains the static interrupt stubs for the various drivers employed
by the Quark SE BSP.
*/

#define _ASMLANGUAGE

#include <autoconf.h>

#ifndef CONFIG_DYNAMIC_INT_STUBS

#include <arch/x86/asm.h>

	/* exports (internal APIs) */
	GTEXT(_loApicTimerIntStub)

	/* externs (internal APIs) */

	GTEXT(_IntEnt)
	GTEXT(_IntExit)
	GTEXT(_loApicTimerIntHandler)
	GTEXT(_LoApicIntEoi)

	GTEXT(__IoApicIntEoi)

SECTION_FUNC (TEXT, _loApicTimerIntStub)
	call    _IntEnt			/* Inform kernel interrupt has begun */
	pushl   $0			/* Push dummy parameter */
	call    _timer_int_handler	/* Call actual interrupt handler */
	call    _loapic_eoi		/* Inform loapic interrupt is done */
	addl    $4, %esp		/* Clean-up stack from push above */
	jmp     _IntExit	/* Inform kernel interrupt is done */
#endif /* !CONFIG_DYNAMIC_INT_STUBS */
