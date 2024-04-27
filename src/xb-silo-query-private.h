/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-query-context.h"
#include "xb-query.h"
#include "xb-silo-query.h"

G_BEGIN_DECLS

GPtrArray *
xb_silo_query_sn_with_root(XbSilo *self, XbNode *n, const gchar *xpath, guint limit, GError **error)
    G_GNUC_NON_NULL(1, 3);
GPtrArray *
xb_silo_query_with_root(XbSilo *self, XbNode *n, const gchar *xpath, guint limit, GError **error)
    G_GNUC_NON_NULL(1, 3);
GPtrArray *
xb_silo_query_with_root_full(XbSilo *self,
			     XbNode *n,
			     XbQuery *query,
			     XbQueryContext *context,
			     gboolean first_result_only,
			     GError **error) G_GNUC_NON_NULL(1, 3);

G_END_DECLS
