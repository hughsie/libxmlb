/*
 * Copyright 2025 Owen Chiaventone <ochiaventone@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

/* Simple arena allocator used to construct nodes during parsing pass
 * If the nodes and strings on them were individually heap allocated instead, we would churn the
 * heap with a large number of small and medium sized allocations. This is bad for performance in
 * a short lived program, but more importantly can trigger serious heap fragmentation if you parse
 * a large XML in the middle of a long-lived program.
 * This was implicated in https://gitlab.gnome.org/GNOME/gnome-software/-/issues/941
 *
 * We don't know how large the XML is when we start parsing, so the arena may have to grow.
 * This arena allocator uses a chunking strategy - if we run out, allocate another large block.
 *
 * This arena is *not* threadsafe. It is expected that it will be created and used from a single
 * thread. Making the arena threadsafe would signficantly change its performance characteristics
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* default to 1MB chunks for arena allocation.
 * Most allocators have heuristics to optimize large allocations
 * For example with glibc malloc this should be above DEFAULT_MMAP_THRESHOLD
 * see  https://sourceware.org/glibc/wiki/MallocInternals */
#define XB_ARENA_CHUNKSIZE 1 << 20

typedef struct XbArena XbArena;

XbArena *
xb_arena_new(void);

/* Allocate bytes from the arena */
void *
xb_arena_alloc(XbArena *arena, size_t len);

/* Free the entire arena at once. Deactivates the arena object. */
void
xb_arena_free(XbArena *arena);

/*
 * Arena friendly alternatives for GLib data structures
 */
char *
xb_arena_strdup(XbArena *arena, const char *src);
char *
xb_arena_strndup(XbArena *arena, const char *src, size_t strsz);

typedef struct XbArenaPtrArray {
	XbArena *arena;
	void **pointers;
	size_t len;
	size_t cap;
} XbArenaPtrArray;

XbArenaPtrArray *
xb_arena_ptr_array_new(XbArena *arena);
void
xb_arena_ptr_array_add(XbArenaPtrArray *self, void *data);
void
xb_arena_ptr_array_remove(XbArenaPtrArray *self, void *data);
void
xb_arena_ptr_array_remove_index(XbArenaPtrArray *self, size_t index);