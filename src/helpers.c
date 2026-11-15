// SPDX-License-Identifier: BSD-3-Clause

#include <unistd.h>
#include <sys/mman.h>
#include "helpers.h"

static struct block_meta *heap_head;
static struct block_meta *heap_tail;

static void *heap_grow(size_t increment)
{
	void *old_brk = sbrk(increment);

	DIE(old_brk == (void *)-1, "sbrk");

	return old_brk;
}

static void heap_append(struct block_meta *block)
{
	block->next = NULL;
	block->prev = heap_tail;

	if (heap_tail)
		heap_tail->next = block;
	else
		heap_head = block;

	heap_tail = block;
}

static void heap_preallocate(void)
{
	struct block_meta *block = heap_grow(HEAP_PREALLOC_SIZE);

	block->size = HEAP_PREALLOC_SIZE - META_SIZE;
	block->status = STATUS_FREE;
	heap_append(block);
}

static void *map_block(size_t size)
{
	size_t total = size + META_SIZE;
	struct block_meta *block;

	block = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	DIE(block == MAP_FAILED, "mmap");

	block->size = size;
	block->status = STATUS_MAPPED;
	block->prev = NULL;
	block->next = NULL;

	return PAYLOAD(block);
}

void coalesce_next(struct block_meta *block)
{
	struct block_meta *next = block->next;

	block->size += META_SIZE + next->size;
	block->next = next->next;

	if (next->next)
		next->next->prev = block;
	else
		heap_tail = block;
}

static void coalesce_all(void)
{
	struct block_meta *block = heap_head;

	while (block) {
		if (block->status == STATUS_FREE) {
			while (block->next && block->next->status == STATUS_FREE)
				coalesce_next(block);
		}
		block = block->next;
	}
}

void split_block(struct block_meta *block, size_t size)
{
	struct block_meta *rest;

	if (block->size < size + META_SIZE + ALIGN(1))
		return;

	rest = (struct block_meta *)((char *)PAYLOAD(block) + size);
	rest->size = block->size - size - META_SIZE;
	rest->status = STATUS_FREE;
	rest->prev = block;
	rest->next = block->next;

	if (block->next)
		block->next->prev = rest;
	else
		heap_tail = rest;

	block->next = rest;
	block->size = size;
}

static struct block_meta *find_best(size_t size)
{
	struct block_meta *best = NULL;
	struct block_meta *block;

	for (block = heap_head; block; block = block->next) {
		if (block->status != STATUS_FREE || block->size < size)
			continue;
		if (!best || block->size < best->size)
			best = block;
		if (best->size == size)
			break;
	}

	return best;
}

struct block_meta *heap_expand_block(struct block_meta *block, size_t size)
{
	if (block != heap_tail || block->size >= size)
		return NULL;

	heap_grow(size - block->size);
	block->size = size;

	return block;
}

static void *heap_alloc(size_t size)
{
	struct block_meta *block;

	if (!heap_head)
		heap_preallocate();

	coalesce_all();

	block = find_best(size);
	if (block) {
		split_block(block, size);
		block->status = STATUS_ALLOC;
		return PAYLOAD(block);
	}

	if (heap_tail->status == STATUS_FREE) {
		block = heap_expand_block(heap_tail, size);
		block->status = STATUS_ALLOC;
		return PAYLOAD(block);
	}

	block = heap_grow(size + META_SIZE);
	block->size = size;
	block->status = STATUS_ALLOC;
	heap_append(block);

	return PAYLOAD(block);
}

void *alloc_block(size_t size, size_t threshold)
{
	size = ALIGN(size);

	if (size + META_SIZE >= threshold)
		return map_block(size);

	return heap_alloc(size);
}

void free_block(struct block_meta *block)
{
	int ret;

	if (block->status == STATUS_ALLOC) {
		block->status = STATUS_FREE;
	} else if (block->status == STATUS_MAPPED) {
		ret = munmap(block, block->size + META_SIZE);
		DIE(ret == -1, "munmap");
	}
}
