/* quark_se_clock_control.c - Clock controller driver for Quark SE */

/*
 * Copyright (c) 2015 Intel Corporation.
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
#include <arch/cpu.h>

#include <misc/__assert.h>
#include <board.h>
#include <device.h>

#include <sys_io.h>

#include <clock_control.h>
#include <clock_control/quark_se_clock_control.h>

#ifndef CONFIG_CLOCK_DEBUG
#define DBG(...) {;}
#else
#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define DBG printf
#else
#include <misc/printk.h>
#define DBG printk
#endif /* CONFIG_STDOUT_CONSOLE */
#endif /* CONFIG_CLOCK_DEBUG */

static inline int quark_se_clock_control_on(struct device *dev,
					clock_control_subsys_t sub_system)
{
	struct quark_se_clock_control_config *info = dev->config->config_info;
	uint32_t subsys = POINTER_TO_INT(sub_system);

	if (sub_system == CLOCK_CONTROL_SUBSYS_ALL) {
		DBG("Enabling all clock gates on dev %p\n", dev);
		sys_write32(0xffffffff, info->base_address);

		return DEV_OK;
	}

	DBG("Enabling clock gate on dev %p subsystem %u\n", dev, subsys);

	return sys_test_and_set_bit(info->base_address, subsys);
}

static inline int quark_se_clock_control_off(struct device *dev,
					clock_control_subsys_t sub_system)
{
	struct quark_se_clock_control_config *info = dev->config->config_info;
	uint32_t subsys = POINTER_TO_INT(sub_system);

	if (sub_system == CLOCK_CONTROL_SUBSYS_ALL) {
		DBG("Disabling all clock gates on dev %p\n", dev);
		sys_write32(0x00000000, info->base_address);

		return DEV_OK;
	}

	DBG("clock gate on dev %p subsystem %u\n", dev, subsys);

	return sys_test_and_clear_bit(info->base_address, subsys);
}

static struct clock_control_driver_api quark_se_clock_control_api = {
	.on = quark_se_clock_control_on,
	.off = quark_se_clock_control_off
};

int quark_se_clock_control_init(struct device *dev)
{
	dev->driver_api = &quark_se_clock_control_api;

	DBG("Quark Se clock controller driver initialized on device: %p\n",
									dev);
	return DEV_OK;
}
