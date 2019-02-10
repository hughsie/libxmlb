/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-silo.h"

G_BEGIN_DECLS

#define XB_TYPE_QUERY (xb_query_get_type ())
G_DECLARE_DERIVABLE_TYPE (XbQuery, xb_query, XB, QUERY, GObject)

struct _XbQueryClass {
	GObjectClass			 parent_class;
	/*< private >*/
	void (*_xb_reserved1)		(void);
	void (*_xb_reserved2)		(void);
	void (*_xb_reserved3)		(void);
	void (*_xb_reserved4)		(void);
	void (*_xb_reserved5)		(void);
	void (*_xb_reserved6)		(void);
	void (*_xb_reserved7)		(void);
};

/**
 * XbQueryFlags:
 * @XB_QUERY_FLAG_NONE:			No extra flags to use
 * @XB_QUERY_FLAG_OPTIMIZE:		Optimize the query when possible
 * @XB_QUERY_FLAG_USE_INDEXES:		Use the indexed parameters
 *
 * The flags used fo query.
 **/
typedef enum {
	XB_QUERY_FLAG_NONE		= 0,			/* Since: 0.1.6 */
	XB_QUERY_FLAG_OPTIMIZE		= 1 << 0,		/* Since: 0.1.6 */
	XB_QUERY_FLAG_USE_INDEXES	= 1 << 1,		/* Since: 0.1.6 */
	/*< private >*/
	XB_QUERY_FLAG_LAST
} XbQueryFlags;

XbQuery		*xb_query_new			(XbSilo		*silo,
						 const gchar	*xpath,
						 GError		**error);
XbQuery		*xb_query_new_full		(XbSilo		*silo,
						 const gchar	*xpath,
						 XbQueryFlags	 flags,
						 GError		**error);
const gchar	*xb_query_get_xpath		(XbQuery	*self);
guint		 xb_query_get_limit		(XbQuery	*self);
void		 xb_query_set_limit		(XbQuery	*self,
						 guint		 limit);
gboolean	 xb_query_bind_str		(XbQuery	*self,
						 guint		 idx,
						 const gchar	*str,
						 GError		**error);
gboolean	 xb_query_bind_val		(XbQuery	*self,
						 guint		 idx,
						 guint32	 val,
						 GError		**error);

G_END_DECLS
