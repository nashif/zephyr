/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BOOTLOADER_H_
#define ZEPHYR_INCLUDE_BOOTLOADER_H_
/*
 * Raw (on-flash) representation of the v1 image header.
 */
struct image_raw_header {
	u32_t header_magic;
	u32_t image_load_address;
	u16_t header_size;
	u16_t header_offset;
	u16_t dev_id;
	u16_t pad;
	u32_t image_size;
	u32_t image_flags;
	struct {
		u8_t major;
		u8_t minor;
		u16_t revision;
		u32_t build_num;
	} version;
	u32_t pad2;
} __packed;

/* Header: */
#define BOOT_HEADER_MAGIC_V1 0x96f3b83d
#define BOOT_HEADER_SIZE_V1 32

int boot_read_image_header(u8_t area_id, struct image_raw_header *raw_header);
int boot_request_upgrade(int permanent);
int boot_erase_img_bank(u8_t area_id);
void start_bootloader(void);
#endif
