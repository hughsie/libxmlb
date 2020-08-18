/*
 * Copyright (C) 2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib.h>

gchar		*xb_content_type_guess			(const gchar	*filename,
							 const guchar	*buf,
							 gsize		 bufsz);
