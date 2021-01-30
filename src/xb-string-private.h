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
gboolean	 xb_string_searchv			(const gchar	**text,
							 const gchar	**search);
gboolean	 xb_string_token_valid			(const gchar	*text);
gchar		*xb_string_xml_escape			(const gchar	*str);
gboolean	 xb_string_isspace			(const gchar	*str,
							 gssize		 strsz);

typedef struct __attribute__ ((packed)) {
	guint32	tlo;
	guint16	tmi;
	guint16	thi;
	guint16	clo;
	guint8	nde[6];
} XbGuid;

gchar		*xb_guid_to_string			(XbGuid		*guid);
void		 xb_guid_compute_for_data		(XbGuid		*out,
							 const guint8	*buf,
							 gsize		 bufsz);

G_END_DECLS
