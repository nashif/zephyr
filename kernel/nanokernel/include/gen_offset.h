/* gen_offset.h - macros to generate structure member offset definitions */

/*
 * Copyright (c) 2010, 2012, 2014 Wind River Systems, Inc.
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
This header contains macros to allow a nanokernel implementation to
generate absolute symbols whose values represents the member offsets for
various nanokernel structures.  These absolute symbols are typically
utilized by assembly source files rather than hardcoding the values in
some local header file.

WARNING: Absolute symbols can potentially be utilized by external tools --
for example, to locate a specific field within a data structure. Consequently,
changes made to such symbols may require modifications to the associated
tool(s). Typically, relocating a member of a structure merely requires
that a tool be rebuilt; however, moving a member to another structure
(or to a new sub-structure within an existing structure) may require that
the tool itself be modified. Likewise, deleting, renaming, or changing the
meaning of an absolute symbol may require modifications to a tool.


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

#include <gen_offset.h>

/@ include struct definitions for which offsets symbols are to be generated @/

#include <nano_private.h>

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

#include <toolchain.h>
#include <stddef.h>

/* definition of the GEN_OFFSET_SYM() macros is toolchain independant  */

#define GEN_OFFSET_SYM(S, M) \
	GEN_ABSOLUTE_SYM(__##S##_##M##_##OFFSET, offsetof(S, M))

#endif /* _GEN_OFFSET_H */
