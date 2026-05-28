/*
 * Copyright 2026 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib.h>

typedef void GRWLockReaderLocker;

static inline GRWLockReaderLocker *
g_rw_lock_reader_locker_new(GRWLock *rw_lock)
{
	g_rw_lock_reader_lock(rw_lock);
	return (GRWLockReaderLocker *)rw_lock;
}

static inline void
g_rw_lock_reader_locker_free(GRWLockReaderLocker *locker)
{
	g_rw_lock_reader_unlock((GRWLock *)locker);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GRWLockReaderLocker, g_rw_lock_reader_locker_free)

typedef void GRWLockWriterLocker;

static inline GRWLockWriterLocker *
g_rw_lock_writer_locker_new(GRWLock *rw_lock)
{
	g_rw_lock_writer_lock(rw_lock);
	return (GRWLockWriterLocker *)rw_lock;
}

static inline void
g_rw_lock_writer_locker_free(GRWLockWriterLocker *locker)
{
	g_rw_lock_writer_unlock((GRWLock *)locker);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GRWLockWriterLocker, g_rw_lock_writer_locker_free)
