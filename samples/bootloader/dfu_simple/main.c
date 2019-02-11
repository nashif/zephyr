/*
 * Copyright (c) 2016 Intel Corporation
 * Copyright (c) 2017 Phytec Messtechnik GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Sample app for USB DFU class driver. */

#include <zephyr.h>
#include <logging/log.h>
#include <flash_map.h>
#include <bootloader.h>

LOG_MODULE_REGISTER(main);

extern const struct flash_area *flash_map;

static void fa_cb(const struct flash_area *fa, void *user_data)
{
	LOG_INF("%-4d %-8d %-20s  %-12u %-12zu", fa->fa_id, fa->fa_device_id,
		fa->fa_dev_name, fa->fa_off, fa->fa_size);
}

static int flash_map_list(void)
{
	LOG_INF("ID | Device | Device Name"
		"       |   Offset   |   Size");
	LOG_INF("-------------------------"
		"------------------------------");
	flash_area_foreach(fa_cb, NULL);
	return 0;
}

void main(void)
{
	/* Nothing to be done other than the selecting appropriate build
	 * config options. Use dfu-util to update the device.
	 */
	LOG_INF("This device supports USB DFU class.");
	flash_map_list();

	printk("Sleeping for a while...\n");
	k_sleep(K_SECONDS(5));
	start_bootloader();

	printk("Never should get here\n");
	while (1) {
		;
	}
}
