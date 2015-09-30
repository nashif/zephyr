/* ipi_atp.h - Atlas Peak mailbox driver */

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


#ifndef __INCatp_mailboxh
#define __INCatp_mailboxh

#include <nanokernel.h>
#include <board.h> /* for SCSS_REGISTER_BASE */
#include <ipi.h>
#include <device.h>
#include <init.h>

#define ATP_IPI_OUTBOUND	0
#define ATP_IPI_INBOUND		1

#if defined(CONFIG_PLATFORM_QUARK_SE_X86)
/* First byte of the ATP_IPI_MASK register is for the Lakemont */
#define ATP_IPI_MASK_START_BIT		0
#define ATP_IPI_INTERRUPT		21
#define ATP_IPI_ARC_LMT_DIR		ATP_IPI_INBOUND
#define ATP_IPI_LMT_ARC_DIR		ATP_IPI_OUTBOUND

#elif defined(CONFIG_PLATFORM_QUARK_SE_ARC)
/* Second byte is for ARC */
#define ATP_IPI_MASK_START_BIT		8
#define ATP_IPI_INTERRUPT		57
#define ATP_IPI_ARC_LMT_DIR		ATP_IPI_OUTBOUND
#define ATP_IPI_LMT_ARC_DIR		ATP_IPI_INBOUND

#else
#error "Unsupported platform for ipi_atp driver"
#endif

#define ATP_IPI_CHANNELS	8
#define ATP_IPI_DATA_BYTES	(4 * sizeof(uint32_t))
#define ATP_IPI_MAX_ID_VAL	0x7FFFFFFF

/* ATP EAS section 28.5.1.123 */
struct __packed atp_ipi_ch_ctrl {
	uint32_t ctrl : 31;
	uint32_t irq : 1;
};

struct __packed atp_ipi_ch_sts {
	uint32_t sts : 1;
	uint32_t irq : 1;
	uint32_t reserved : 30;
};

struct __packed atp_ipi {
	struct atp_ipi_ch_ctrl ctrl;
	uint8_t data[ATP_IPI_DATA_BYTES]; /* contiguous 32-bit registers */
	struct atp_ipi_ch_sts sts;
};

/* Base address for mailboxes
 *
 * Layout:
 *
 * atp_ipi[8]
 * ATP_IPI_CHALL_STS
 */
#define ATP_IPI_BASE		(SCSS_REGISTER_BASE + 0xa00)

/* 28.5.1.73 Host processor Interrupt routing mask 21
 *
 * Bits		Description
 * 31:24	Mailbox SS Halt interrupt maskIUL
 * 23:16	Mailbox Host Halt interrupt mask
 * 15:8		Mailbox SS interrupt mask
 * 7:0		Mailbox Host interrupt mask
 */
#define ATP_IPI_MASK		(SCSS_REGISTER_BASE + 0x4a0)

/* All status bits of the mailboxes
 *
 * Bits		Description
 * 31:16	Reserved
 * 15:0		CHn_STS bits (sts/irq) for all channels
 */
#define ATP_IPI_CHALL_STS	(SCSS_REGISTER_BASE + 0x0AC0)

#define ATP_IPI(channel)	((volatile struct atp_ipi *)(ATP_IPI_BASE + \
					((channel) * sizeof(struct atp_ipi))))


/* XXX I pulled this number out of thin air -- how to choose
 * the right priority? */
#define ATP_IPI_INTERRUPT_PRI		2

struct atp_ipi_controller_config_info {
	int (*controller_init)(void);
};

struct atp_ipi_config_info {
	int channel;
	int direction;
	volatile struct atp_ipi *ipi;
};


struct atp_ipi_driver_data {
	ipi_callback_t callback;
	void *callback_ctx;
};

void atp_ipi_isr(void *param);

int atp_ipi_initialize(struct device *d);
int atp_ipi_controller_initialize(struct device *d);

#define ATP_IPI_DEFINE(name, ch, dir) \
	struct atp_ipi_config_info atp_ipi_config_##name = { \
		.ipi = ATP_IPI(ch), \
		.channel = ch, \
		.direction = dir \
	}; \
	struct atp_ipi_driver_data atp_ipi_runtime_##name; \
	DECLARE_DEVICE_INIT_CONFIG(name, _STRINGIFY(name), \
				   atp_ipi_initialize, \
				   &atp_ipi_config_##name); \
	pre_kernel_late_init(name, &atp_ipi_runtime_##name);


#endif /* __INCatp_mailboxh */
