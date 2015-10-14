/* test_adc.c - ADC test */

/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
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
#include <nanokernel.h>
#include <arch/cpu.h>

#if defined(CONFIG_STDOUT_CONSOLE)
#include <stdio.h>
#define PRINT           printf
#else
#include <misc/printk.h>
#define PRINT           printk
#endif

#include <device.h>
#include <adc.h>

#define MIN_CH          0
#define MAX_CH          18
#define CH_PARAM        2
#define TEST_DLY        50
#define TEST_RESOLUTION 12
#define TEST_CLK_RATIO  1024
#define LENGTH          80

struct adc_test_ctx {
	uint32_t channel;
	uint32_t data;
} adc_ctx;

static void rx_cbk(struct device *dev);
static void err_cbk(struct device *dev);

/**
 * @Brief Callback functions
 */
static void rx_cbk(struct device *dev)
{
	PRINT("ADC Sampling: %d %d", adc_ctx.channel, adc_ctx.data);
	adc_disable(dev);
	adc_unlock(dev);
}

static void err_cbk(struct device *dev)
{
	PRINT("ADC Sampling Error.");
	adc_disable(dev);
	adc_unlock(dev);
}


/**
 * @Brief Test command to get the channel 0 value.
 *
 */
void adc_get()
{
	struct device *dev;
	struct io_adc_seq_table seq_tbl;
	struct io_adc_seq_entry entrys;
	uint32_t data_len = 1;
	adc_ctx.channel = 0;
	adc_ctx.data = 0;
	entrys.channel_id = adc_ctx.channel;
	entrys.sample_dly = TEST_DLY;

	dev = device_get_binding(ADC_DRV_NAME);

	if(!dev)
	{
		PRINT("The device ADC does not exist.");
		return;
	}

	if ( adc_lock(dev)) {
		PRINT("Error: ADC in use.");
		return;
	}

	adc_set_cb(dev, rx_cbk, err_cbk);

	if (adc_ctx.channel >= MIN_CH && adc_ctx.channel <= MAX_CH) {
		adc_enable(dev);
		seq_tbl.entries = &entrys;
		seq_tbl.num_entries = data_len;
		if( adc_read(dev, &seq_tbl, &(adc_ctx.data), data_len) )
		{
			PRINT("Read status: OK");
			goto exit_disable;
		}
		else
		{
			PRINT("Invalid Channel %d", adc_ctx.channel);
		}
		goto exit_unlock;
	}

	return;

exit_disable:
	adc_disable(dev);

exit_unlock:
	adc_unlock(dev);
	return;
}

#define STACKSIZE 2000

char __stack fiberStack[STACKSIZE];

void fiberEntry(void)
{
	adc_get();
}

void main(void)
{
	task_fiber_start(&fiberStack[0], STACKSIZE,
			(nano_fiber_entry_t) fiberEntry, 0, 0, 7, 0);

	PRINT("%s: ADC Demo\n", __FUNCTION__);
}
