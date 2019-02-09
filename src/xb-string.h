/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

void		 xb_string_append_union			(GString	*xpath,
							 const gchar	*fmt,
							 ...) G_GNUC_PRINTF (2, 3);
gchar		*xb_string_escape			(const gchar	*str);

G_END_DECLS
