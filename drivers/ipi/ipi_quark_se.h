/* ipi_quark_se.h - Quark SE mailbox driver */

/*
 * Copyright (c) 2015 Intel Corporation
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


#ifndef __INCquark_se_mailboxh
#define __INCquark_se_mailboxh

#include <nanokernel.h>
#include <board.h> /* for SCSS_REGISTER_BASE */
#include <ipi.h>
#include <device.h>
#include <init.h>

#define QUARK_SE_IPI_OUTBOUND	0
#define QUARK_SE_IPI_INBOUND		1

#if defined(CONFIG_PLATFORM_QUARK_SE_X86)
/* First byte of the QUARK_SE_IPI_MASK register is for the Lakemont */
#define QUARK_SE_IPI_MASK_START_BIT		0
#define QUARK_SE_IPI_INTERRUPT		21
#define QUARK_SE_IPI_ARC_LMT_DIR		QUARK_SE_IPI_INBOUND
#define QUARK_SE_IPI_LMT_ARC_DIR		QUARK_SE_IPI_OUTBOUND

#elif defined(CONFIG_PLATFORM_QUARK_SE_ARC)
/* Second byte is for ARC */
#define QUARK_SE_IPI_MASK_START_BIT		8
#define QUARK_SE_IPI_INTERRUPT		57
#define QUARK_SE_IPI_ARC_LMT_DIR		QUARK_SE_IPI_OUTBOUND
#define QUARK_SE_IPI_LMT_ARC_DIR		QUARK_SE_IPI_INBOUND

#else
#error "Unsupported platform for ipi_quark_se driver"
#endif

#define QUARK_SE_IPI_CHANNELS	8
#define QUARK_SE_IPI_DATA_BYTES	(4 * sizeof(uint32_t))
#define QUARK_SE_IPI_MAX_ID_VAL	0x7FFFFFFF

/* QUARK_SE EAS section 28.5.1.123 */
struct __packed quark_se_ipi_ch_ctrl {
	uint32_t ctrl : 31;
	uint32_t irq : 1;
};

struct __packed quark_se_ipi_ch_sts {
	uint32_t sts : 1;
	uint32_t irq : 1;
	uint32_t reserved : 30;
};

struct __packed quark_se_ipi {
	struct quark_se_ipi_ch_ctrl ctrl;
	uint8_t data[QUARK_SE_IPI_DATA_BYTES]; /* contiguous 32-bit registers */
	struct quark_se_ipi_ch_sts sts;
};

/* Base address for mailboxes
 *
 * Layout:
 *
 * quark_se_ipi[8]
 * QUARK_SE_IPI_CHALL_STS
 */
#define QUARK_SE_IPI_BASE		(SCSS_REGISTER_BASE + 0xa00)

/* 28.5.1.73 Host processor Interrupt routing mask 21
 *
 * Bits		Description
 * 31:24	Mailbox SS Halt interrupt maskIUL
 * 23:16	Mailbox Host Halt interrupt mask
 * 15:8		Mailbox SS interrupt mask
 * 7:0		Mailbox Host interrupt mask
 */
#define QUARK_SE_IPI_MASK		(SCSS_REGISTER_BASE + 0x4a0)

/* All status bits of the mailboxes
 *
 * Bits		Description
 * 31:16	Reserved
 * 15:0		CHn_STS bits (sts/irq) for all channels
 */
#define QUARK_SE_IPI_CHALL_STS	(SCSS_REGISTER_BASE + 0x0AC0)

#define QUARK_SE_IPI(channel)	((volatile struct quark_se_ipi *)(QUARK_SE_IPI_BASE + \
					((channel) * sizeof(struct quark_se_ipi))))


/* XXX I pulled this number out of thin air -- how to choose
 * the right priority? */
#define QUARK_SE_IPI_INTERRUPT_PRI		2

struct quark_se_ipi_controller_config_info {
	int (*controller_init)(void);
};

struct quark_se_ipi_config_info {
	int channel;
	int direction;
	volatile struct quark_se_ipi *ipi;
};


struct quark_se_ipi_driver_data {
	ipi_callback_t callback;
	void *callback_ctx;
};

void quark_se_ipi_isr(void *param);

int quark_se_ipi_initialize(struct device *d);
int quark_se_ipi_controller_initialize(struct device *d);

#define QUARK_SE_IPI_DEFINE(name, ch, dir) \
	struct quark_se_ipi_config_info quark_se_ipi_config_##name = { \
		.ipi = QUARK_SE_IPI(ch), \
		.channel = ch, \
		.direction = dir \
	}; \
	struct quark_se_ipi_driver_data quark_se_ipi_runtime_##name; \
	DECLARE_DEVICE_INIT_CONFIG(name, _STRINGIFY(name), \
				   quark_se_ipi_initialize, \
				   &quark_se_ipi_config_##name); \
	pre_kernel_late_init(name, &quark_se_ipi_runtime_##name);


#endif /* __INCquark_se_mailboxh */
