/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <stddef.h>
#include <stdlib.h>
#include "block_meta.h"

#define ALIGNMENT		8
#define ALIGN(size)		(((size) + (ALIGNMENT - 1)) & ~(size_t)(ALIGNMENT - 1))

#define META_SIZE		ALIGN(sizeof(struct block_meta))
#define MMAP_THRESHOLD		(128 * 1024)
#define HEAP_PREALLOC_SIZE	(128 * 1024)

#define PAYLOAD(block)		((void *)((char *)(block) + META_SIZE))
#define BLOCK(ptr)		((struct block_meta *)((char *)(ptr) - META_SIZE))

void *alloc_block(size_t size, size_t threshold);
void free_block(struct block_meta *block);
struct block_meta *heap_expand_block(struct block_meta *block, size_t size);
void split_block(struct block_meta *block, size_t size);
void coalesce_next(struct block_meta *block);
