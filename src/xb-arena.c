/*
 * Copyright 2025 Owen Chiaventone <ochiaventone@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "xb-arena.h"

#include <glib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

struct XbArena {
	size_t n_chunks;
	size_t chunks_cap;
	/* Array of pointers to chunks */
	uint8_t **chunks;
	/* Points to first free byte in current chunk */
	uint8_t *tail;
	uint8_t *current_chunk_end;
};

XbArena *
xb_arena_new(void)
{
	XbArena *arena = malloc(sizeof(XbArena));
	*arena = (XbArena){
	    .n_chunks = 0,
	    .chunks_cap = 8,
	    .chunks = (uint8_t **)malloc(8 * sizeof(uint8_t *)),
	    .tail = NULL,
	    .current_chunk_end = NULL,
	};
	return arena;
}

static void
xb_arena_add_chunk(XbArena *arena, uint8_t *chunk)
{
	/* Grow chunklist if needed */
	if (arena->n_chunks + 1 >= arena->chunks_cap) {
		void *new_chunklist = malloc(arena->chunks_cap * sizeof(uint8_t *) * 2);
		memcpy(new_chunklist, (void *)arena->chunks, arena->n_chunks * sizeof(uint8_t *));
		free((void *)arena->chunks);
		arena->chunks = (uint8_t **)new_chunklist;
		arena->chunks_cap *= 2;
	}
	arena->chunks[arena->n_chunks] = chunk;
	arena->n_chunks++;
}

void *
xb_arena_alloc(XbArena *arena, size_t len)
{
	void *alloc = NULL;
	if (len == 0)
		return NULL;

	/* Treat allocations larger than a chunk size as their own chunk
	 * It's not worth trying to optimize those. If this case hits a lot, try
	 * increasing the arena chunk size. */
	if (len > XB_ARENA_CHUNKSIZE) {
		uint8_t *chunk = malloc(len);
		xb_arena_add_chunk(arena, chunk);
		/* No need to invalidate tail or current_chunk_end, there's still space
		in the current chunk for more allocations */
		return chunk;
	}
	/* Round allocations up to nearest whole machine word
	 * If we didn't do this, allocated structs may cost extra
	 * to access because of unaligned load/stores and may
	 * straddle multiple cache lines */
	len = (len + sizeof(ssize_t) - 1) & ~(sizeof(ssize_t) - 1);

	/* Allocate another chunk if the the requested size won't fit in the current one.
	 * This will always be hit for the first allocation on a new arena
	 * because tail and current_chunk_end are initialized to zero. */
	if (arena->tail + len > arena->current_chunk_end) {
		uint8_t *chunk = malloc(XB_ARENA_CHUNKSIZE);
		xb_arena_add_chunk(arena, chunk);
		arena->tail = chunk;
		arena->current_chunk_end = chunk + (XB_ARENA_CHUNKSIZE);
	}

	alloc = arena->tail;
	arena->tail += len;
	return alloc;
}

void
xb_arena_free(XbArena *arena)
{
	for (uint32_t i = 0; i < arena->n_chunks; i++) {
		free(arena->chunks[i]);
	}
	free((void *)arena->chunks);
	free(arena);
}

char *
xb_arena_strdup(XbArena *arena, const char *src)
{
	size_t n = 0;
	char *out = NULL;
	if (!src)
		return NULL;
	n = strlen(src) + 1;
	out = xb_arena_alloc(arena, n);
	memcpy(out, src, n);
	out[n] = '\0';
	return out;
}

char *
xb_arena_strndup(XbArena *arena, const char *src, size_t strsz)
{
	size_t n = 0;
	char *out = NULL;
	if (!src)
		return NULL;
	/* strnlen isn't in the c99/posix standard even though it's widely supported
	 * easy enough to do it ourselves */
	for (n = 0; n < strsz; n++) {
		if (src[n] == '\0')
			break;
	}
	out = xb_arena_alloc(arena, n + 1);
	memcpy(out, src, n);
	out[n] = '\0';
	return out;
}

XbArenaPtrArray *
xb_arena_ptr_array_new(XbArena *arena)
{
	XbArenaPtrArray *self = xb_arena_alloc(arena, sizeof(XbArenaPtrArray));
	*self = (XbArenaPtrArray){
	    .arena = arena,
	    .pointers = (void **)xb_arena_alloc(arena, 4 * sizeof(void *)),
	    .len = 0,
	    .cap = 4,
	};
	return self;
}
void
xb_arena_ptr_array_add(XbArenaPtrArray *self, void *data)
{
	/* Grow array if necessary */
	if (self->len + 1 > self->cap) {
		void *newptrs = NULL;
		self->cap *= 2;
		newptrs = xb_arena_alloc(self->arena, self->cap * sizeof(void *));
		memcpy(newptrs, (void *)self->pointers, self->len * sizeof(void *));
		self->pointers = (void **)newptrs;
	}
	/* Insert new pointer */
	self->pointers[self->len] = data;
	self->len++;
}
void
xb_arena_ptr_array_remove(XbArenaPtrArray *self, void *data)
{
	for (guint i = 0; i < self->len; i++) {
		if (self->pointers[i] == data) {
			xb_arena_ptr_array_remove_index(self, i);
			return;
		}
	}
}

void
xb_arena_ptr_array_remove_index(XbArenaPtrArray *self, size_t index)
{
	/* Must preserve order of elements. Slide down all elements after the removed one */
    for (guint i = index; i < self->len - 1; i++) {
        self->pointers[i] = self->pointers[i+1];
    }
    self->len--;
}