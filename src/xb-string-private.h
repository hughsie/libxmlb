/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>
#include "xb-string.h"

G_BEGIN_DECLS

guint		 xb_string_replace			(GString	*str,
							 const gchar	*search,
							 const gchar	*replace);
gboolean	 xb_string_contains			(const gchar	*text,
							 const gchar	*search);
gboolean	 xb_string_search			(const gchar	*text,
							 const gchar	*search);
gchar		*xb_string_xml_escape			(const gchar	*str);

G_END_DECLS
