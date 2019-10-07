/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-query.h"

G_BEGIN_DECLS

typedef enum {
	XB_SILO_QUERY_KIND_UNKNOWN,
	XB_SILO_QUERY_KIND_WILDCARD,
	XB_SILO_QUERY_KIND_PARENT,
	XB_SILO_QUERY_KIND_LAST
} XbSiloQueryKind;

typedef struct {
	gchar		*element;
	guint32		 element_idx;
	GPtrArray	*predicates;	/* of XbStack */
	XbSiloQueryKind	 kind;
} XbQuerySection;

GPtrArray	*xb_query_get_sections		(XbQuery	*self);
gchar		*xb_query_to_string		(XbQuery	*self);

G_END_DECLS
