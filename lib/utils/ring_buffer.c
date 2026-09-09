/* ring_buffer.c: deprecated item API on top of the ring buffer primitives */

/*
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/util.h>
#include <string.h>

/**
 * Internal data structure for a buffer header.
 *
 * We want all of this to fit in a single uint32_t. Every item stored in the
 * ring buffer will be one of these headers plus any extra data supplied
 */
struct ring_element {
	uint32_t  type   :16; /**< Application-specific */
	uint32_t  length :8;  /**< length in 32-bit chunks */
	uint32_t  value  :8;  /**< Room for small integral values */
};

/*
 * Item buffers are sized in 32-bit words and every item is a multiple of
 * four bytes, so the header never straddles the end of the buffer.
 */
int ring_buf_item_put(struct ring_buf *rb, uint16_t type, uint8_t value, uint32_t *data32,
		      uint8_t size32)
{
	uint8_t *dst, *data = (uint8_t *)data32;
	struct ring_element *header;
	uint32_t size, chunk, avail, total;

	size = (uint32_t)size32 * 4U;
	if (size + sizeof(struct ring_element) > ring_buf_space_get(rb)) {
		return -EMSGSIZE;
	}

	avail = ring_buf_put_ptr(rb, &dst, 0);
	__ASSERT_NO_MSG(avail >= sizeof(struct ring_element));

	header = (struct ring_element *)dst;
	header->type = type;
	header->length = size32;
	header->value = value;
	total = sizeof(struct ring_element);

	while (size > 0U) {
		avail = ring_buf_put_ptr(rb, &dst, total);
		chunk = MIN(avail, size);
		__ASSERT_NO_MSG(chunk > 0U);
		memcpy(dst, data, chunk);
		data += chunk;
		total += chunk;
		size -= chunk;
	}

	ring_buf_commit(rb, total);

	return 0;
}

int ring_buf_item_get(struct ring_buf *rb, uint16_t *type, uint8_t *value, uint32_t *data32,
		      uint8_t *size32)
{
	uint8_t *src, *data = (uint8_t *)data32;
	struct ring_element *header;
	uint32_t size, chunk, avail, total;

	if (ring_buf_is_empty(rb)) {
		return -EAGAIN;
	}

	avail = ring_buf_get_ptr(rb, &src, 0);
	__ASSERT_NO_MSG(avail >= sizeof(struct ring_element));

	header = (struct ring_element *)src;

	if ((data != NULL) && (header->length > *size32)) {
		*size32 = header->length;
		return -EMSGSIZE;
	}

	*size32 = header->length;
	*type = header->type;
	*value = header->value;
	total = sizeof(struct ring_element);

	size = (uint32_t)*size32 * 4U;

	while (size > 0U) {
		avail = ring_buf_get_ptr(rb, &src, total);
		chunk = MIN(avail, size);
		__ASSERT_NO_MSG(chunk > 0U);
		if (data != NULL) {
			memcpy(data, src, chunk);
			data += chunk;
		}
		total += chunk;
		size -= chunk;
	}

	ring_buf_consume(rb, total);

	return 0;
}
