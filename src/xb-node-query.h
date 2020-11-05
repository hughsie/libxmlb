/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-query.h"
#include "xb-query-context.h"
#include "xb-node.h"

G_BEGIN_DECLS

GPtrArray	*xb_node_query			(XbNode		*self,
						 const gchar	*xpath,
						 guint		 limit,
						 GError		**error);

GPtrArray	*xb_node_query_full		(XbNode		*self,
						 XbQuery	*query,
						 GError		**error);
GPtrArray	*xb_node_query_with_context	(XbNode		*self,
						 XbQuery	*query,
						 XbQueryContext	*context,
						 GError		**error);

XbNode		*xb_node_query_first		(XbNode		*self,
						 const gchar	*xpath,
						 GError		**error);

XbNode		*xb_node_query_first_full	(XbNode		*self,
						 XbQuery	*query,
						 GError		**error);
XbNode		*xb_node_query_first_with_context(XbNode	*self,
						 XbQuery	*query,
						 XbQueryContext	*context,
						 GError		**error);

const gchar	*xb_node_query_text		(XbNode		*self,
						 const gchar	*xpath,
						 GError		**error);
guint64		 xb_node_query_text_as_uint	(XbNode		*self,
						 const gchar	*xpath,
						 GError		**error);
const gchar	*xb_node_query_attr		(XbNode		*self,
						 const gchar	*xpath,
						 const gchar	*name,
						 GError		**error);
guint64		 xb_node_query_attr_as_uint	(XbNode		*self,
						 const gchar	*xpath,
						 const gchar	*name,
						 GError		**error);
gchar		*xb_node_query_export		(XbNode		*self,
						 const gchar	*xpath,
						 GError		**error);

G_END_DECLS
