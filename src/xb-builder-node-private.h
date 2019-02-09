/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

G_BEGIN_DECLS

#include <glib-object.h>

#include "xb-builder-node.h"

typedef struct {
	/*< private >*/
	gchar			*name;
	guint32			 name_idx;
	gchar			*value;
	guint32			 value_idx;
} XbBuilderNodeAttr;

GPtrArray	*xb_builder_node_get_attrs	(XbBuilderNode		*self);
guint32		 xb_builder_node_size		(XbBuilderNode		*self);
guint32		 xb_builder_node_get_offset	(XbBuilderNode		*self);
void		 xb_builder_node_set_offset	(XbBuilderNode		*self,
						 guint32		 offset);
gint		 xb_builder_node_get_priority	(XbBuilderNode		*self);
void		 xb_builder_node_set_priority	(XbBuilderNode		*self,
						 gint			 priority);
guint32		 xb_builder_node_get_element_idx(XbBuilderNode		*self);
void		 xb_builder_node_set_element_idx(XbBuilderNode		*self,
						 guint32		 element_idx);
guint32		 xb_builder_node_get_text_idx	(XbBuilderNode		*self);
void		 xb_builder_node_set_text_idx	(XbBuilderNode		*self,
						 guint32		 text_idx);

G_END_DECLS
