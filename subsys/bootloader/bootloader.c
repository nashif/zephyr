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

#include <assert.h>
#include <zephyr.h>
#include <flash.h>
#include <drivers/system_timer.h>
#include <soc.h>

#include <bootloader.h>
#include "flash_map_backend.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(bootloader);


#if defined(CONFIG_ARM)
struct arm_vector_table {
	uint32_t msp;
	uint32_t reset;
};

extern void sys_clock_disable(void);

static void do_boot(struct image_raw_header *header)
{
	struct arm_vector_table *vt;
	uintptr_t flash_base;
	int rc;

	/* The beginning of the image is the ARM vector table, containing
     	 * the initial stack pointer address and the reset vector
     	 * consecutively. Manually set the stack pointer and jump into the
     	 * reset vector
     	 */
	rc = flash_device_base(header->dev_id, &flash_base);
	assert(rc == 0);

	vt = (struct arm_vector_table *)(flash_base + header->header_offset +
					 header->header_size);
	irq_lock();
	sys_clock_disable();
#ifdef CONFIG_BOOT_SERIAL_CDC_ACM
	/* Disable the USB to prevent it from firing interrupts */
	usb_disable();
#endif
	__set_MSP(vt->msp);
	((void (*)(void))vt->reset)();
}


#elif CONFIG_BOOT_FROM_FLASH

#define CONFIG_LOAD_TO_ADDRESS 0xBE030000

static void copy_img_to_sram(int slot, unsigned int hdr_offset)
{
	const struct flash_area *fap;
	int area_id;
	int rc;
	unsigned char *dst = (unsigned char *)(CONFIG_LOAD_TO_ADDRESS + hdr_offset);

	LOG_INF("Copying image to SRAM");

	area_id = flash_area_id_from_image_slot(slot);
	rc = flash_area_open(area_id, &fap);
	if (rc != 0) {
		LOG_ERR("flash_area_open failed with %d\n", rc);
		goto done;
	}

	rc = flash_area_read(fap, hdr_offset, dst, fap->fa_size - hdr_offset);
	if (rc != 0) {
		LOG_ERR("flash_area_read failed with %d\n", rc);
		goto done;
	}

done:
	flash_area_close(fap);
}

/* Entry point (.ResetVector) is at the very beginning of the image.
 * Simply copy the image to a suitable location and jump there.
 */
static void do_boot(struct image_raw_header *header)
{
	void *start;

	LOG_INF("ih_hdr_size = 0x%x\n", header->header_size);

	/* Copy from the flash to HP SRAM */
	copy_img_to_sram(0, header->header_size);

	/* Jump to entry point */
	start = (void *)(CONFIG_LOAD_TO_ADDRESS + header->header_size);
	((void (*)(void))start)();
}

#else
/* Default: Assume entry point is at the very beginning of the image. Simply
 * lock interrupts and jump there. This is the right thing to do for X86 and
 * possibly other platforms.
 */
static void do_boot(struct image_raw_header *header)
{
	uintptr_t flash_base;
	void *start;
	int rc;

	rc = flash_device_base(header->dev_id, &flash_base);
	assert(rc == 0);

	start = (void *)(flash_base + header->header_offset +
			 header->header_size);

	/* Lock interrupts and dive into the entry point */
	irq_lock();
	((void (*)(void))start)();
}
#endif

void start_bootloader(void)
{
	struct image_raw_header header;
	int rc;

	LOG_INF("Starting bootloader");

#if 0
	if (!flash_device_get_binding(DT_FLASH_DEV_NAME)) {
		LOG_ERR("Flash device %s not found", DT_FLASH_DEV_NAME);
		while (1) {
			;
		}
	}
#endif

	LOG_INF("Jumping to the first image slot");

	rc = boot_read_image_header(DT_FLASH_AREA_IMAGE_0_ID, &header);
	assert(rc == 0);
	//do_boot(header);

	LOG_ERR("Never should get here");
	while (1) {
		;
	}
}
