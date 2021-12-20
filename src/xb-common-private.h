/*
 * Copyright (C) 2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib.h>

gchar *
xb_content_type_guess(const gchar *filename, const guchar *buf, gsize bufsz);
gboolean
xb_file_set_contents(GFile *file,
		     const guint8 *buf,
		     gsize bufsz,
		     GCancellable *cancellable,
		     GError **error);
