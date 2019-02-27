/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <gio/gio.h>

#include "xb-builder-fixup.h"
#include "xb-builder-node.h"
#include "xb-builder-source-ctx.h"

G_BEGIN_DECLS

#define XB_TYPE_BUILDER_SOURCE (xb_builder_source_get_type ())

G_DECLARE_DERIVABLE_TYPE (XbBuilderSource, xb_builder_source, XB, BUILDER_SOURCE, GObject)

struct _XbBuilderSourceClass {
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
 * XbBuilderSourceFlags:
 * @XB_BUILDER_SOURCE_FLAG_NONE:		No extra flags to use
 * @XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT:	Do not attempt to repair XML whitespace
 * @XB_BUILDER_SOURCE_FLAG_WATCH_FILE:		Watch the source file for changes
 *
 * The flags for converting to XML.
 **/
typedef enum {
	XB_BUILDER_SOURCE_FLAG_NONE		= 0,		/* Since: 0.1.0 */
	XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT	= 1 << 0,	/* Since: 0.1.0 */
	XB_BUILDER_SOURCE_FLAG_WATCH_FILE	= 1 << 1,	/* Since: 0.1.0 */
	/*< private >*/
	XB_BUILDER_SOURCE_FLAG_LAST
} XbBuilderSourceFlags;

typedef gboolean (*XbBuilderSourceNodeFunc)	(XbBuilderSource	*self,
						 XbBuilderNode		*bn,
						 gpointer		 user_data,
						 GError			**error);
typedef GInputStream *(*XbBuilderSourceConverterFunc) (XbBuilderSource	*self,
						 GFile			*file,
						 gpointer		 user_data,
						 GCancellable		*cancellable,
						 GError			**error);
typedef GInputStream *(*XbBuilderSourceAdapterFunc) (XbBuilderSource	*self,
						 XbBuilderSourceCtx	*ctx,
						 gpointer		 user_data,
						 GCancellable		*cancellable,
						 GError			**error);

XbBuilderSource	*xb_builder_source_new		(void);
gboolean	 xb_builder_source_load_file	(XbBuilderSource	*self,
						 GFile			*file,
						 XbBuilderSourceFlags	 flags,
						 GCancellable		*cancellable,
						 GError			**error);
gboolean	 xb_builder_source_load_xml	(XbBuilderSource	*self,
						 const gchar		*xml,
						 XbBuilderSourceFlags	 flags,
						 GError			**error);
gboolean	 xb_builder_source_load_bytes	(XbBuilderSource	*self,
						 GBytes			*bytes,
						 XbBuilderSourceFlags	 flags,
						 GError			**error);
void		 xb_builder_source_set_info	(XbBuilderSource	*self,
						 XbBuilderNode		*info);
void		 xb_builder_source_set_prefix	(XbBuilderSource	*self,
						 const gchar		*prefix);
void		 xb_builder_source_add_node_func (XbBuilderSource	*self,
						 const gchar		*id,
						 XbBuilderSourceNodeFunc func,
						 gpointer		 user_data,
						 GDestroyNotify		 user_data_free)
G_DEPRECATED_FOR(xb_builder_source_add_fixup);
void		 xb_builder_source_add_fixup	(XbBuilderSource	*self,
						 XbBuilderFixup		*fixup);
void		 xb_builder_source_add_converter (XbBuilderSource	*self,
						 const gchar		*content_types,
						 XbBuilderSourceConverterFunc func,
						 gpointer		 user_data,
						 GDestroyNotify		 user_data_free)
G_DEPRECATED_FOR(xb_builder_source_add_adapter);
void		 xb_builder_source_add_adapter	(XbBuilderSource	*self,
						 const gchar		*content_types,
						 XbBuilderSourceAdapterFunc func,
						 gpointer		 user_data,
						 GDestroyNotify		 user_data_free);

G_END_DECLS
