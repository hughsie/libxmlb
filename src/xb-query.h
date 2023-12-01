/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define XB_TYPE_QUERY (xb_query_get_type())
G_DECLARE_DERIVABLE_TYPE(XbQuery, xb_query, XB, QUERY, GObject)

struct _XbQueryClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_xb_reserved1)(void);
	void (*_xb_reserved2)(void);
	void (*_xb_reserved3)(void);
	void (*_xb_reserved4)(void);
	void (*_xb_reserved5)(void);
	void (*_xb_reserved6)(void);
	void (*_xb_reserved7)(void);
};

/**
 * XbQueryFlags:
 * @XB_QUERY_FLAG_NONE:			No extra flags to use
 * @XB_QUERY_FLAG_OPTIMIZE:		Optimize the query when possible
 * @XB_QUERY_FLAG_USE_INDEXES:		Use the indexed parameters
 * @XB_QUERY_FLAG_REVERSE:		Reverse the results order
 * @XB_QUERY_FLAG_FORCE_NODE_CACHE:	Always cache the #XbNode objects
 *
 * The flags used for queries.
 **/
typedef enum {
	XB_QUERY_FLAG_NONE = 0,			 /* Since: 0.1.6 */
	XB_QUERY_FLAG_OPTIMIZE = 1 << 0,	 /* Since: 0.1.6 */
	XB_QUERY_FLAG_USE_INDEXES = 1 << 1,	 /* Since: 0.1.6 */
	XB_QUERY_FLAG_REVERSE = 1 << 2,		 /* Since: 0.1.15 */
	XB_QUERY_FLAG_FORCE_NODE_CACHE = 1 << 3, /* Since: 0.2.0 */
	/*< private >*/
	XB_QUERY_FLAG_LAST
} XbQueryFlags;

#include "xb-silo.h"

XbQuery *
xb_query_new(XbSilo *silo, const gchar *xpath, GError **error) G_GNUC_NON_NULL(1, 2);
XbQuery *
xb_query_new_full(XbSilo *silo, const gchar *xpath, XbQueryFlags flags, GError **error)
    G_GNUC_NON_NULL(1, 2);
const gchar *
xb_query_get_xpath(XbQuery *self) G_GNUC_NON_NULL(1);

G_DEPRECATED_FOR(xb_query_context_get_limit)
guint
xb_query_get_limit(XbQuery *self) G_GNUC_NON_NULL(1);
G_DEPRECATED_FOR(xb_query_context_set_limit)
void
xb_query_set_limit(XbQuery *self, guint limit) G_GNUC_NON_NULL(1);

G_DEPRECATED_FOR(xb_query_context_get_flags)
XbQueryFlags
xb_query_get_flags(XbQuery *self) G_GNUC_NON_NULL(1);
G_DEPRECATED_FOR(xb_query_context_set_flags)
void
xb_query_set_flags(XbQuery *self, XbQueryFlags flags) G_GNUC_NON_NULL(1);

G_DEPRECATED_FOR(xb_value_bindings_bind_str)
gboolean
xb_query_bind_str(XbQuery *self, guint idx, const gchar *str, GError **error) G_GNUC_NON_NULL(1);
G_DEPRECATED_FOR(xb_value_bindings_bind_val)
gboolean
xb_query_bind_val(XbQuery *self, guint idx, guint32 val, GError **error) G_GNUC_NON_NULL(1);

G_END_DECLS
