/* ipi_atp.c - Atlas Peak mailbox driver */

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
 * 3) Neither the name of Intel Corporation nor the names of its contributors
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

#include <nanokernel.h>
#include <stdint.h>
#include <string.h>
#include <device.h>
#include <init.h>
#include <ipi.h>
#include <arch/cpu.h>
#include <misc/printk.h>
#include <misc/__assert.h>
#include <errno.h>
#include "ipi_atp.h"


/* We have a single ISR for all channels, so in order to properly handle
 * messages we need to figure out which device object corresponds to
 * in incoming channel */
static struct device *device_by_channel[ATP_IPI_CHANNELS];
static uint32_t inbound_channels;

static uint32_t atp_ipi_sts_get(void)
{
	return sys_read32(ATP_IPI_CHALL_STS) & inbound_channels;
}

static void mailbox_handle(struct device *d)
{
	struct atp_ipi_config_info *config;
	struct atp_ipi_driver_data *driver_data;
	volatile struct atp_ipi *ipi;

	config = d->config->config_info;
	driver_data = d->driver_data;
	ipi = config->ipi;

	if (driver_data->callback) {
		driver_data->callback(driver_data->callback_ctx,
				      ipi->ctrl.ctrl, &ipi->data);
	}

	ipi->sts.irq = 1; /* Clear the interrupt bit */
	ipi->sts.sts = 1; /* Clear channel status bit */
}


static void set_channel_irq_state(int channel, int enable)
{
	mem_addr_t addr = ATP_IPI_MASK;
	int bit = channel + ATP_IPI_MASK_START_BIT;

	if (enable) {
		sys_clear_bit(addr, bit);
	} else {
		sys_set_bit(addr, bit);
	}
}


/* Interrupt handler, gets messages on all incoming enabled mailboxes */
void atp_ipi_isr(void *param)
{
	int channel;
	int sts;
	struct device *d;

	ARG_UNUSED(param);

	/* Find out which mailbox channel has an incoming message */
	while (1) {
		sts = atp_ipi_sts_get();

		/* FIXME: for every message sent, there are two interrupts
		 * generated, the second has empty sts. Probably an IRQ
		 * triggering issue */
		if (!sts) {
			break;
		}

		channel = (find_msb_set(sts) - 1) / 2;
		d = device_by_channel[channel];
		if (d) {
			mailbox_handle(d);
		}
	}

}


static int atp_ipi_send(struct device *d, int wait, uint32_t id,
			const void *data, int size)
{
	struct atp_ipi_config_info *config = d->config->config_info;
	volatile struct atp_ipi *ipi = config->ipi;
	const uint8_t *data8;
	int i;
	int flags;

	if (id > ATP_IPI_MAX_ID_VAL) {
		return -EINVAL;
	}

	if (config->direction != ATP_IPI_OUTBOUND) {
		return -EINVAL;
	}

	if (size > ATP_IPI_DATA_BYTES) {
		return -EMSGSIZE;
	}

	flags = irq_lock();

	if (ipi->sts.sts != 0) {
		irq_unlock(flags);
		return -EBUSY;
	}

	/* Populate the data, memcpy doesn't take volatiles */
	data8 = (const uint8_t *)data;

	for (i = 0; i < size; ++i) {
		ipi->data[i] = data8[i];
	}
	ipi->ctrl.ctrl = id;

	/* Cause the interrupt to assert on the remote side */
	ipi->ctrl.irq = 1;

	/* Wait for HW to set the sts bit */
	while (ipi->sts.sts == 0) { }

	if (wait) {
		/* Loop until remote clears the status bit */
		while(ipi->sts.sts != 0) { }
	}
	irq_unlock(flags);
	return 0;
}


static int atp_ipi_max_data_size_get(struct device *d)
{
	ARG_UNUSED(d);

	return ATP_IPI_DATA_BYTES;
}


static uint32_t atp_ipi_max_id_val_get(struct device *d)
{
	ARG_UNUSED(d);

	return ATP_IPI_MAX_ID_VAL;
}

static void atp_ipi_register_callback(struct device *d, ipi_callback_t cb,
				       void *context)
{
	struct atp_ipi_driver_data *driver_data = d->driver_data;

	driver_data->callback = cb;
	driver_data->callback_ctx = context;
}


static int atp_ipi_set_enabled(struct device *d, int enable)
{
	struct atp_ipi_config_info *config_info = d->config->config_info;

	if (config_info->direction != ATP_IPI_INBOUND) {
		return -EINVAL;
	}
	set_channel_irq_state(config_info->channel, enable);
	return 0;
}

static struct ipi_driver_api api_funcs = {
	.send = atp_ipi_send,
	.register_callback = atp_ipi_register_callback,
	.max_data_size_get = atp_ipi_max_data_size_get,
	.max_id_val_get = atp_ipi_max_id_val_get,
	.set_enabled = atp_ipi_set_enabled
};

int atp_ipi_controller_initialize(struct device *d)
{
	struct atp_ipi_controller_config_info *config = d->config->config_info;
#if CONFIG_IPI_ATP_MASTER
	int i;

	/* Mask all mailbox interrupts, we'll enable them
	 * individually later. Clear out any pending messages */
	sys_write32(0xFFFFFFFF, ATP_IPI_MASK);
	for (i = 0; i < ATP_IPI_CHANNELS; ++i) {
		volatile struct atp_ipi *ipi = ATP_IPI(i);
		ipi->sts.sts = 0;
		ipi->sts.irq = 0;
	}
#endif

	if (config->controller_init) {
		return config->controller_init();
	}
	return DEV_OK;
}


int atp_ipi_initialize(struct device *d)
{
	struct atp_ipi_config_info *config = d->config->config_info;

	device_by_channel[config->channel] = d;
	d->driver_api = &api_funcs;
	if (config->direction == ATP_IPI_INBOUND) {
		inbound_channels |= (0x3 << (config->channel * 2));
	}

	return DEV_OK;
}


