/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 * vi:set noexpandtab tabstop=8 shiftwidth=8:
 *
 * Copyright (C) 2020 Endless OS Foundation LLC
 *
 * Author: Philip Withnall <withnall@endlessm.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib.h>

#include "xb-query.h"
#include "xb-value-bindings.h"

G_BEGIN_DECLS

/**
 * XbQueryContext:
 *
 * An opaque struct which contains context for executing a query in, such as the
 * number of results to return, or values to bind to query placeholders.
 *
 * Since: 0.3.0
 */
typedef struct {
	/*< private >*/
	gint dummy0;
	guint dummy1;
	XbValueBindings dummy2;
	gpointer dummy3[5];
} XbQueryContext;

GType
xb_query_context_get_type(void);

/**
 * XB_QUERY_CONTEXT_INIT:
 *
 * Static initialiser for #XbQueryContext so it can be used on the stack.
 *
 * Use it in association with g_auto(), to ensure the bindings are freed once
 * finished with:
 * |[
 * g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT ();
 *
 * xb_query_context_set_limit (&context, 0);
 * ]|
 *
 * Since: 0.3.0
 */
#define XB_QUERY_CONTEXT_INIT()                                                                    \
	{                                                                                          \
		0, 0, XB_VALUE_BINDINGS_INIT(), { NULL, NULL, NULL, NULL, NULL }                   \
	}

void
xb_query_context_init(XbQueryContext *self) G_GNUC_NON_NULL(1);
void
xb_query_context_clear(XbQueryContext *self) G_GNUC_NON_NULL(1);

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC(XbQueryContext, xb_query_context_clear)

XbQueryContext *
xb_query_context_copy(XbQueryContext *self) G_GNUC_NON_NULL(1);
void
xb_query_context_free(XbQueryContext *self) G_GNUC_NON_NULL(1);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XbQueryContext, xb_query_context_free)

XbValueBindings *
xb_query_context_get_bindings(XbQueryContext *self) G_GNUC_NON_NULL(1);

guint
xb_query_context_get_limit(XbQueryContext *self) G_GNUC_NON_NULL(1);
void
xb_query_context_set_limit(XbQueryContext *self, guint limit) G_GNUC_NON_NULL(1);

XbQueryFlags
xb_query_context_get_flags(XbQueryContext *self) G_GNUC_NON_NULL(1);
void
xb_query_context_set_flags(XbQueryContext *self, XbQueryFlags flags) G_GNUC_NON_NULL(1);

G_END_DECLS
