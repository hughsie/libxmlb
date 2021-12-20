/*
 * Copyright (C) 2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN "XbCommon"

#include "config.h"

#include <gio/gio.h>
#include <string.h>

#include "xb-common-private.h"

static const gchar *
xb_content_type_guess_from_fn(const gchar *filename)
{
	gchar *ext; /* no ownership */

	g_return_val_if_fail(filename != NULL, NULL);

	/* get file extension with dot */
	ext = g_strrstr(filename, ".");
	if (ext == NULL)
		return NULL;

	/* map Windows "mime-type" to a content type */
	if (g_strcmp0(ext, ".gz") == 0)
		return "application/gzip";
	if (g_strcmp0(ext, ".xz") == 0)
		return "application/x-xz";
	if (g_strcmp0(ext, ".txt") == 0 || g_strcmp0(ext, ".xml") == 0)
		return "application/xml";
	if (g_strcmp0(ext, ".desktop") == 0)
		return "application/x-desktop";
	return NULL;
}

static gboolean
xb_content_type_match(const guchar *buf,
		      gsize bufsz,
		      gsize offset,
		      const gchar *magic,
		      gsize magic_size)
{
	/* document too small */
	if (offset + magic_size > bufsz)
		return FALSE;
	return memcmp(buf + offset, magic, magic_size) == 0;
}

/**
 * xb_content_type_guess: (skip)
 * @filename: (nullable): filename
 * @buf: (nullable): file data buffer
 * @bufsz: size of file data buffer
 *
 * Guesses the content type based on example data. Either @filename or @buf may
 * be %NULL, in which case the guess will be based solely on the other argument.
 *
 * Returns: a string indicating a guessed content type
 **/
gchar *
xb_content_type_guess(const gchar *filename, const guchar *buf, gsize bufsz)
{
	g_autofree gchar *content_type = NULL;

	/* check for bad results, e.g. from Chrome OS */
	content_type = g_content_type_guess(filename, buf, bufsz, NULL);
	if (g_strstr_len(content_type, -1, "/") == NULL ||
	    g_strcmp0(content_type, "application/octet-stream") == 0 ||
	    g_strcmp0(content_type, "text/plain") == 0) {
		/* magic */
		if (bufsz > 0) {
			if (xb_content_type_match(buf, bufsz, 0x0, "\x1f\x8b", 2))
				return g_strdup("application/gzip");
			if (xb_content_type_match(buf, bufsz, 0x0, "\xfd\x37\x7a\x58\x5a\x00", 6))
				return g_strdup("application/x-xz");
			if (xb_content_type_match(buf, bufsz, 0x0, "<?xml", 5))
				return g_strdup("application/xml");
			if (xb_content_type_match(buf, bufsz, 0x0, "[Desktop Entry]", 15))
				return g_strdup("application/x-desktop");
		}

		/* file extensions */
		if (filename != NULL) {
			const gchar *tmp = xb_content_type_guess_from_fn(filename);
			if (tmp != NULL)
				return g_strdup(tmp);
		}
	}

#ifdef _WIN32
	/* fall back harder as there is no mime data at all */
	if (filename != NULL) {
		const gchar *tmp = xb_content_type_guess_from_fn(filename);
		if (tmp != NULL)
			return g_strdup(tmp);
	}
#endif

	return g_steal_pointer(&content_type);
}

/**
 * xb_file_set_contents: (skip)
 * @file: (nullable): file to write
 * @buf: (nullable): data buffer
 * @bufsz: size of @buf
 * @cancellable: (nullable): optional #GCancellable
 * @error: (nullable): optional return location for an error
 *
 * Writes data to a file. This is done safely using a temporary file to do this
 * atomically on Linux but a direct write is used on Windows.
 *
 * Returns: %TRUE for success
 **/
gboolean
xb_file_set_contents(GFile *file,
		     const guint8 *buf,
		     gsize bufsz,
		     GCancellable *cancellable,
		     GError **error)
{
#ifdef _WIN32
	g_autofree gchar *fn = g_file_get_path(file);
#endif

	g_return_val_if_fail(G_IS_FILE(file), FALSE);
	g_return_val_if_fail(buf != NULL, FALSE);
	g_return_val_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

#ifdef _WIN32
#if GLIB_CHECK_VERSION(2, 66, 0)
	return g_file_set_contents_full(fn,
					(const gchar *)buf,
					(gssize)bufsz,
					G_FILE_SET_CONTENTS_NONE,
					0666,
					error);

#else
	g_set_error_literal(error,
			    G_IO_ERROR,
			    G_IO_ERROR_NOT_SUPPORTED,
			    "not supported as GLib version is too old");
	return FALSE;
#endif
#else
	return g_file_replace_contents(file,
				       (const gchar *)buf,
				       (gsize)bufsz,
				       NULL,
				       FALSE,
				       G_FILE_CREATE_NONE,
				       NULL,
				       cancellable,
				       error);
#endif
}
