#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <flash.h>
#include <flash_map.h>
#include <zephyr.h>

#include <misc/__assert.h>
#include <misc/byteorder.h>
#include <bootloader.h>

#include <logging/log.h>

LOG_MODULE_REGISTER(img_mgr);

int boot_read_image_header(u8_t area_id, struct image_raw_header *raw_header)
{
	const struct flash_area *fa;
	int rc;

	LOG_INF("Opening flash area: %d", area_id);
	rc = flash_area_open(area_id, &fa);
	if (rc) {
		LOG_ERR("Opening flash area: %d failed: %d", area_id, rc);
		return rc;
	}

	/*
	 * Read and sanity-check the raw header.
	 */
	rc = flash_area_read(fa, 0, raw_header, sizeof(*raw_header));
	flash_area_close(fa);
	if (rc) {
		return rc;
	}
	LOG_INF("Read flash area %d successfully", area_id);

	raw_header->header_magic = sys_le32_to_cpu(raw_header->header_magic);
	raw_header->image_load_address =
		sys_le32_to_cpu(raw_header->image_load_address);
	raw_header->header_size = sys_le16_to_cpu(raw_header->header_size);

	LOG_INF("Header size: %d", raw_header->header_size);

	raw_header->image_size = sys_le32_to_cpu(raw_header->image_size);
	raw_header->image_flags = sys_le32_to_cpu(raw_header->image_flags);
	raw_header->version.revision =
		sys_le16_to_cpu(raw_header->version.revision);
	raw_header->version.build_num =
		sys_le32_to_cpu(raw_header->version.build_num);

	/*
	 * Sanity checks.
	 *
	 * Larger values in header_size than BOOT_HEADER_SIZE_V1 are
	 * possible, e.g. if Zephyr was linked with
	 * CONFIG_TEXT_SECTION_OFFSET > BOOT_HEADER_SIZE_V1.
	 */
	if ((raw_header->header_magic != BOOT_HEADER_MAGIC_V1) ||
	    (raw_header->header_size < BOOT_HEADER_SIZE_V1)) {
		return -EIO;
	}

	return 0;
}

int boot_request_upgrade(int permanent)
{
	return 0;
}


int boot_erase_img_bank(u8_t area_id)
{
	const struct flash_area *fa;
	int rc;

	rc = flash_area_open(area_id, &fa);
	if (rc) {
		return rc;
	}

	rc = flash_area_erase(fa, 0, fa->fa_size);

	flash_area_close(fa);

	return rc;
}
