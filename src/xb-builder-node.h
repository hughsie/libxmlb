/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_BUILDER_NODE_H
#define __XB_BUILDER_NODE_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef GNode XbBuilderNode;

typedef struct {
	gchar			*name;
	guint32			 name_idx;
	gchar			*value;
	guint32			 value_idx;
} XbBuilderNodeAttr;

typedef struct {
	guint32			 off;
	gboolean		 is_cdata_ignore;
	gchar			*element_name;
	guint32			 element_name_idx;
	gchar			*text;
	guint32			 text_idx;
	GPtrArray		*attrs;
} XbBuilderNodeData;

XbBuilderNode	*xb_builder_node_new		(const gchar	*element_name);
void		 xb_builder_node_free		(XbBuilderNode	*n);
guint32		 xb_builder_node_size		(XbBuilderNode	*n);
void		 xb_builder_node_add_attribute	(XbBuilderNode	*n,
						 const gchar	*name,
						 const gchar	*value);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XbBuilderNode, xb_builder_node_free)

G_END_DECLS

#endif /* __XB_BUILDER_NODE_H */
