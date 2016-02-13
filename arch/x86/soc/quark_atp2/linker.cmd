/*
 * Copyright (c) 2015 Wind River Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2) Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
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

/**
 * @file
 * @brief Linker command/script file
 *
 * This is the linker script for both standard images and XIP images.
 */

#include <autoconf.h>

/* physical address of RAM (needed for correct __ram_phys_end symbol) */
#define PHYS_RAM_ADDR    CONFIG_PHYS_RAM_ADDR

/* physical address where the kernel is loaded */
#ifdef CONFIG_XIP
  #define PHYS_LOAD_ADDR   CONFIG_PHYS_LOAD_ADDR
#else  /* !CONFIG_XIP */
  #define PHYS_LOAD_ADDR   CONFIG_PHYS_RAM_ADDR
#endif /* CONFIG_XIP */


MEMORY
    {
#ifdef CONFIG_XIP
    ROM (rx)        : ORIGIN = PHYS_LOAD_ADDR, LENGTH = CONFIG_ROM_SIZE*1K
    RAM (wx)        : ORIGIN = PHYS_RAM_ADDR, LENGTH =  CONFIG_RAM_SIZE*1K
#else  /* !CONFIG_XIP */
    RAM (wx)        : ORIGIN = PHYS_LOAD_ADDR, LENGTH = CONFIG_RAM_SIZE*1K
#endif /* CONFIG_XIP */

    /*
     * It doesn't matter where this region goes as it is stripped from the
     * final ELF image. The address doesn't even have to be valid on the
     * target. However, it shouldn't overlap any other regions.
     */

    IDT_LIST        : ORIGIN = 2K, LENGTH = 2K
    }

#include <arch/x86/linker-common-sections.h>

/* start adding bsp specific linker sections here */

/* no sections should appear after linker-epilog.h */
#include <arch/x86/linker-epilog.h>
