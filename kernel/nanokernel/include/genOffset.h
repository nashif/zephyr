/* genOffset.h - macros to generate structure member offset definitions */

/*
 * Copyright (c) 2010, 2012, 2014 Wind River Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3) Neither the name of Wind River Systems nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
DESCRIPTION
This header contains macros to allow a nanokernel implementation to
generate absolute symbols whose values represents the member offsets for
various nanokernel structures.  These absolute symbols are typically
utilized by assembly source files rather than hardcoding the values in
some local header file.

The macro "GEN_OFFSET_SYM(structure, member)" is used to generate a single
absolute symbol.  The absolute symbol will appear in the object module
generated from the source file that utilizes the GEN_OFFSET_SYM() macro.
Absolute symbols representing a structure member offset have the following
form:

	__<structure>_<member>_OFFSET

This header also defines the GEN_ABSOLUTE_SYM macro to simply define an
absolute symbol, irrespective of whether the value represents a structure
or offset.

The following sample file illustrates the usage of the macros available
in this file:

<START of sample source file: offsets.c>

#include <genOffset.h>

/@ include struct definitions for which offsets symbols are to be generated @/

#include <nanok.h>

GEN_ABS_SYM_BEGIN (_OffsetAbsSyms)	/@ the name parameter is arbitrary @/

/@ tNANO structure member offsets @/

GEN_OFFSET_SYM (tNANO, fiber);
GEN_OFFSET_SYM (tNANO, task);
GEN_OFFSET_SYM (tNANO, current);
GEN_OFFSET_SYM (tNANO, nested);
GEN_OFFSET_SYM (tNANO, common_isp);

GEN_ABSOLUTE_SYM (__tNANO_SIZEOF, sizeof(tNANO));

GEN_ABS_SYM_END

<END of sample source file: offsets.c>

Compiling the sample offsets.c results in the following symbols in offsets.o:

$ nm offsets.o
00000010 A __tNANO_common_isp_OFFSET
00000008 A __tNANO_current_OFFSET
0000000c A __tNANO_nested_OFFSET
00000000 A __tNANO_fiber_OFFSET
00000004 A __tNANO_task_OFFSET


\NOMANUAL
*/

#ifndef _GEN_OFFSET_H
#define _GEN_OFFSET_H

/*#include "../../../../../include/absSym.h"*/
#include <absSym.h>
#include <stddef.h>

#ifdef __GNUC__

/* create an extern reference to the absolute symbol */

#define GEN_OFFSET_EXTERN(name) extern const char name[]

#elif defined(__DCC__)

/* create an extern reference to the absolute symbol */

#define GEN_OFFSET_EXTERN(name) extern const char name[]

#else /* unsupported toolchain */

#define GEN_OFFSET_EXTERN(name) \
	#error GEN_OFFSET_EXTERN macro used with unsupported toolchain

#define GEN_OFFSET_SYM(name) \
	#error GEN_OFFSET_SYM macro used with unsupported toolchain

#endif /* end of "unsupported toolchain" */

/* definition of the GEN_OFFSET_SYM() macros is toolchain independant  */

#define GEN_OFFSET_SYM(S, M) \
	GEN_ABSOLUTE_SYM(__##S##_##M##_##OFFSET, offsetof(S, M))

/*
 * Macro used to generate an absolute symbol utilized by host-side tools.
 *
 * IMPORTANT: Changes to such symbols MUST be coordinated with the host-side
 * tool that uses each symbol.  Do NOT delete or change the meaning
 * of structure members used by host-side tools without proper consultation.
 * Relocating a member within a structure can be done freely, as long as it
 * only changes the offset of the member; however, relocating a member used
 * by host-side tools to another structure (including a new sub-structure
 * within an existing structure) requires consultation.
 */

#if defined(CONFIG_CONTEXT_MONITOR)
#define GEN_OFFSET_SYM_HOST(s, m) GEN_OFFSET_SYM(s, m)
#else
#define GEN_OFFSET_SYM_HOST(s, m)
#endif /* CONFIG_CONTEXT_MONITOR */

#endif /* _GEN_OFFSET_H */
