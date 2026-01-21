/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "xb-builder-node.h"
#include "xb-compile.h"

G_BEGIN_DECLS

typedef struct {
	/*< private >*/
	gchar *name;
	guint32 name_idx;
	gchar *value;
	guint32 value_idx;
} XbBuilderNodeAttr;

XbArenaPtrArray *
xb_builder_node_get_attrs(XbBuilderNode *self) G_GNUC_NON_NULL(1);
guint32
xb_builder_node_size(XbBuilderNode *self) G_GNUC_NON_NULL(1);
guint32
xb_builder_node_get_offset(XbBuilderNode *self) G_GNUC_NON_NULL(1);
void
xb_builder_node_set_offset(XbBuilderNode *self, guint32 offset) G_GNUC_NON_NULL(1);
gint
xb_builder_node_get_priority(XbBuilderNode *self) G_GNUC_NON_NULL(1);
void
xb_builder_node_set_priority(XbBuilderNode *self, gint priority) G_GNUC_NON_NULL(1);
guint32
xb_builder_node_get_element_idx(XbBuilderNode *self) G_GNUC_NON_NULL(1);
void
xb_builder_node_set_element_idx(XbBuilderNode *self, guint32 element_idx) G_GNUC_NON_NULL(1);
guint32
xb_builder_node_get_text_idx(XbBuilderNode *self) G_GNUC_NON_NULL(1);
void
xb_builder_node_set_text_idx(XbBuilderNode *self, guint32 text_idx) G_GNUC_NON_NULL(1);
guint32
xb_builder_node_get_tail_idx(XbBuilderNode *self) G_GNUC_NON_NULL(1);
void
xb_builder_node_set_tail_idx(XbBuilderNode *self, guint32 tail_idx) G_GNUC_NON_NULL(1);
void
xb_builder_node_add_token_idx(XbBuilderNode *self, guint32 tail_idx) G_GNUC_NON_NULL(1);
GArray *
xb_builder_node_get_token_idxs(XbBuilderNode *self) G_GNUC_NON_NULL(1);

G_END_DECLS
