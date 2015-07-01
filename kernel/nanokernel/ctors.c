/* ctors.c - constructor module for BSPs */

/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
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
This module provides the C++ style constructor machanism used by various
components to initialize themselves automatically. They do this by including
a tag of the form NANO_INIT_xxx in the declaration of the init function;
these tags are defined in nanokernel.h. The _xxx portion of the tag indicates
the priority of the associated constructor, allowing them to execute in a
deterministic order.

The _Ctors() routine is called from a BSP's _Cstart() routine after hardware
initialization has completed.

Although ctors are traditionally a C++ feature, normal C code can use them too.
No destructor support (dtors) is provided.
 */

/* What a constructor function pointer looks like */

typedef void (*CtorFuncPtr)(void);

/*
 * The list of constructor function pointers.
 *
 * Generated by the linker script, and ordered from highest numbered priority
 * ("do last") to lowest numbered priority ("do first"). List element 0 is the
 * number of constructors that follow (N); list element N+1 is NULL.
 */

extern CtorFuncPtr __CTOR_LIST__[];
extern CtorFuncPtr __CTOR_END__[];

/**
 *
 * @brief Invoke all C++ style global object constructors
 *
 * This function is invoked by _Cstart(), which is implemented in the BSP. It
 * invokes all routines that have been tagged using NANO_INIT_xxx, in order
 * of priority (i.e. lowest numbered to highest numbered).
 */

void _Ctors(void)
{
	unsigned int nCtors;

	nCtors = (unsigned int)__CTOR_LIST__[0];

	while (nCtors >= 1) {
		__CTOR_LIST__[nCtors--]();
	}
}
