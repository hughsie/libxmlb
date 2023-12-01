/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "xb-builder-fixup.h"
#include "xb-builder-node.h"
#include "xb-builder-source-ctx.h"
#include "xb-compile.h"

G_BEGIN_DECLS

#define XB_TYPE_BUILDER_SOURCE (xb_builder_source_get_type())

G_DECLARE_DERIVABLE_TYPE(XbBuilderSource, xb_builder_source, XB, BUILDER_SOURCE, GObject)

struct _XbBuilderSourceClass {
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
 * XbBuilderSourceFlags:
 * @XB_BUILDER_SOURCE_FLAG_NONE:		No extra flags to use
 * @XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT:	Do not attempt to repair XML whitespace
 * @XB_BUILDER_SOURCE_FLAG_WATCH_FILE:		Watch the source file for changes
 * @XB_BUILDER_SOURCE_FLAG_WATCH_DIRECTORY:	Watch the directory containing the source file for
 *changes (for example, if watching all the sources in a directory — this allows the file monitors
 *to be shared)
 *
 * The flags for converting to XML.
 **/
typedef enum {
	XB_BUILDER_SOURCE_FLAG_NONE = 0,		 /* Since: 0.1.0 */
	XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT = 1 << 0,	 /* Since: 0.1.0 */
	XB_BUILDER_SOURCE_FLAG_WATCH_FILE = 1 << 1,	 /* Since: 0.1.0 */
	XB_BUILDER_SOURCE_FLAG_WATCH_DIRECTORY = 1 << 2, /* Since: 0.2.0 */
	/*< private >*/
	XB_BUILDER_SOURCE_FLAG_LAST
} XbBuilderSourceFlags;

typedef gboolean (*XbBuilderSourceNodeFunc)(XbBuilderSource *self,
					    XbBuilderNode *bn,
					    gpointer user_data,
					    GError **error);
typedef GInputStream *(*XbBuilderSourceAdapterFunc)(XbBuilderSource *self,
						    XbBuilderSourceCtx *ctx,
						    gpointer user_data,
						    GCancellable *cancellable,
						    GError **error);

XbBuilderSource *
xb_builder_source_new(void);
gboolean
xb_builder_source_load_file(XbBuilderSource *self,
			    GFile *file,
			    XbBuilderSourceFlags flags,
			    GCancellable *cancellable,
			    GError **error) G_GNUC_NON_NULL(1, 2);
gboolean
xb_builder_source_load_xml(XbBuilderSource *self,
			   const gchar *xml,
			   XbBuilderSourceFlags flags,
			   GError **error) G_GNUC_NON_NULL(1, 2);
gboolean
xb_builder_source_load_bytes(XbBuilderSource *self,
			     GBytes *bytes,
			     XbBuilderSourceFlags flags,
			     GError **error) G_GNUC_NON_NULL(1, 2);
void
xb_builder_source_set_info(XbBuilderSource *self, XbBuilderNode *info) G_GNUC_NON_NULL(1);
void
xb_builder_source_set_prefix(XbBuilderSource *self, const gchar *prefix) G_GNUC_NON_NULL(1);
void
xb_builder_source_add_fixup(XbBuilderSource *self, XbBuilderFixup *fixup) G_GNUC_NON_NULL(1, 2);
void
xb_builder_source_add_adapter(XbBuilderSource *self,
			      const gchar *content_types,
			      XbBuilderSourceAdapterFunc func,
			      gpointer user_data,
			      GDestroyNotify user_data_free) G_GNUC_NON_NULL(1, 2, 3);
void
xb_builder_source_add_simple_adapter(XbBuilderSource *self,
				     const gchar *content_types,
				     XbBuilderSourceAdapterFunc func,
				     gpointer user_data,
				     GDestroyNotify user_data_free) G_GNUC_NON_NULL(1, 2, 3);

G_END_DECLS
