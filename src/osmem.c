// SPDX-License-Identifier: BSD-3-Clause

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "osmem.h"
#include "helpers.h"

void *os_malloc(size_t size)
{
	if (size == 0)
		return NULL;

	return alloc_block(size, MMAP_THRESHOLD);
}

void os_free(void *ptr)
{
	if (!ptr)
		return;

	free_block(BLOCK(ptr));
}

void *os_calloc(size_t nmemb, size_t size)
{
	size_t total;
	void *ptr;

	if (nmemb == 0 || size == 0)
		return NULL;

	if (nmemb > SIZE_MAX / size)
		return NULL;

	total = nmemb * size;
	ptr = alloc_block(total, getpagesize());
	memset(ptr, 0, total);

	return ptr;
}

static void *move_block(void *ptr, size_t size)
{
	struct block_meta *block = BLOCK(ptr);
	size_t to_copy = block->size < size ? block->size : size;
	void *new_ptr;

	new_ptr = os_malloc(size);
	memcpy(new_ptr, ptr, to_copy);
	os_free(ptr);

	return new_ptr;
}

void *os_realloc(void *ptr, size_t size)
{
	struct block_meta *block;
	size_t aligned;

	if (!ptr)
		return os_malloc(size);

	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	block = BLOCK(ptr);
	if (block->status == STATUS_FREE)
		return NULL;

	aligned = ALIGN(size);

	if (block->status == STATUS_MAPPED || aligned + META_SIZE >= MMAP_THRESHOLD)
		return move_block(ptr, size);

	if (block->size >= aligned) {
		split_block(block, aligned);
		return ptr;
	}

	if (heap_expand_block(block, aligned))
		return ptr;

	while (block->next && block->next->status == STATUS_FREE) {
		coalesce_next(block);
		if (block->size >= aligned) {
			split_block(block, aligned);
			return ptr;
		}
	}

	if (heap_expand_block(block, aligned))
		return ptr;

	return move_block(ptr, size);
}
