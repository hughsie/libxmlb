/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_BUILDER_SOURCE_H
#define __XB_BUILDER_SOURCE_H

#include <gio/gio.h>

#include "xb-builder-node.h"

G_BEGIN_DECLS

#define XB_TYPE_BUILDER_SOURCE (xb_builder_source_get_type ())

G_DECLARE_FINAL_TYPE (XbBuilderSource, xb_builder_source, XB, BUILDER_SOURCE, GObject)

typedef gboolean (*XbBuilderSourceNodeFunc)	(XbBuilderSource	*self,
						 XbBuilderNode		*bn,
						 gpointer		 user_data,
						 GError			**error);

XbBuilderSource	*xb_builder_source_new_file	(GFile			*file,
						 GCancellable		*cancellable,
						 GError			**error);
XbBuilderSource	*xb_builder_source_new_xml	(const gchar		*xml,
						 GError			**error);
void		 xb_builder_source_set_info	(XbBuilderSource	*self,
						 XbBuilderNode		*info);
void		 xb_builder_source_set_prefix	(XbBuilderSource	*self,
						 const gchar		*prefix);
void		 xb_builder_source_add_node_func (XbBuilderSource	*self,
						 XbBuilderSourceNodeFunc func,
						 gpointer		 user_data,
						 GDestroyNotify		 user_data_free);

G_END_DECLS

#endif /* __XB_BUILDER_SOURCE_H */
