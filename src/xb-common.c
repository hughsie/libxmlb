/*
 * Copyright (C) 2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbCommon"

#include "config.h"

#include <string.h>
#include <gio/gio.h>

#include "xb-common-private.h"

static const gchar *
xb_content_type_guess_from_fn (const gchar *filename)
{
	gchar *ext; /* no ownership */

	g_return_val_if_fail (filename != NULL, NULL);

	/* get file extension with dot */
	ext = g_strrstr (filename, ".");
	if (ext == NULL)
		return NULL;

	/* map Windows "mime-type" to a content type */
	if (g_strcmp0 (ext, ".gz") == 0)
		return "application/gzip";
	if (g_strcmp0 (ext, ".txt") == 0 ||
	    g_strcmp0 (ext, ".xml") == 0)
		return "application/xml";
	if (g_strcmp0 (ext, ".desktop") == 0)
		return "application/x-desktop";
	return NULL;
}

static gboolean
xb_content_type_match (const guchar *buf, gsize bufsz, gsize offset,
		       const gchar *magic, gsize magic_size)
{
	/* document too small */
	if (offset + magic_size > bufsz)
		return FALSE;
	return memcmp (buf + offset, magic, magic_size) == 0;
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
xb_content_type_guess (const gchar *filename, const guchar *buf, gsize bufsz)
{
	g_autofree gchar *content_type = NULL;

	/* check for bad results, e.g. from Chrome OS */
	content_type = g_content_type_guess (filename, buf, bufsz, NULL);
	if (g_strcmp0 (content_type, "application/octet-stream") == 0 ||
	    g_strcmp0 (content_type, "text/plain") == 0) {

		/* magic */
		if (bufsz > 0) {
			if (xb_content_type_match (buf, bufsz, 0x0, "\x1f\x8b", 2))
				return g_strdup ("application/gzip");
			if (xb_content_type_match (buf, bufsz, 0x0, "<?xml", 5))
				return g_strdup ("application/xml");
			if (xb_content_type_match (buf, bufsz, 0x0, "[Desktop Entry]", 15))
				return g_strdup ("application/x-desktop");
		}

		/* file extensions */
		if (filename != NULL) {
			const gchar *tmp = xb_content_type_guess_from_fn (filename);
			if (tmp != NULL)
				return g_strdup (tmp);
		}
	}

#ifdef _WIN32
	/* fall back harder as there is no mime data at all */
	if (filename != NULL) {
		const gchar *tmp = xb_content_type_guess_from_fn (filename);
		if (tmp != NULL)
			return g_strdup (tmp);
	}
#endif

	return g_steal_pointer (&content_type);
}
