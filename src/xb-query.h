/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_QUERY_H
#define __XB_QUERY_H

G_BEGIN_DECLS

#include <glib-object.h>

#include "xb-silo.h"

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

XbQuery		*xb_query_new			(XbSilo		*silo,
						 const gchar	*xpath,
						 GError		**error);
const gchar	*xb_query_get_xpath		(XbQuery	*self);

G_END_DECLS

#endif /* __XB_QUERY_H */
