/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_COMMON_H
#define __XB_COMMON_H

#include <glib-object.h>

G_BEGIN_DECLS

guint		 xb_string_replace			(GString	*str,
							 const gchar	*search,
							 const gchar	*replace);
gboolean	 xb_string_contains_fuzzy		(const gchar	*text,
							 const gchar	*search);

G_END_DECLS

#endif /* __XB_COMMON_H */
