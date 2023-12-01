/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-node.h"
#include "xb-query-context.h"
#include "xb-query.h"

G_BEGIN_DECLS

GPtrArray *
xb_node_query(XbNode *self, const gchar *xpath, guint limit, GError **error) G_GNUC_NON_NULL(1, 2);

GPtrArray *
xb_node_query_full(XbNode *self, XbQuery *query, GError **error) G_GNUC_NON_NULL(1, 2);
GPtrArray *
xb_node_query_with_context(XbNode *self, XbQuery *query, XbQueryContext *context, GError **error)
    G_GNUC_NON_NULL(1, 2, 3);

XbNode *
xb_node_query_first(XbNode *self, const gchar *xpath, GError **error) G_GNUC_NON_NULL(1, 2);

XbNode *
xb_node_query_first_full(XbNode *self, XbQuery *query, GError **error) G_GNUC_NON_NULL(1, 2);
XbNode *
xb_node_query_first_with_context(XbNode *self,
				 XbQuery *query,
				 XbQueryContext *context,
				 GError **error) G_GNUC_NON_NULL(1, 2);

const gchar *
xb_node_query_text(XbNode *self, const gchar *xpath, GError **error) G_GNUC_NON_NULL(1, 2);
guint64
xb_node_query_text_as_uint(XbNode *self, const gchar *xpath, GError **error) G_GNUC_NON_NULL(1, 2);
const gchar *
xb_node_query_attr(XbNode *self, const gchar *xpath, const gchar *name, GError **error)
    G_GNUC_NON_NULL(1, 2, 3);
guint64
xb_node_query_attr_as_uint(XbNode *self, const gchar *xpath, const gchar *name, GError **error)
    G_GNUC_NON_NULL(1, 2, 3);
gchar *
xb_node_query_export(XbNode *self, const gchar *xpath, GError **error) G_GNUC_NON_NULL(1);

G_END_DECLS
