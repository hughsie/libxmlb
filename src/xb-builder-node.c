/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <gio/gio.h>

#include "xb-builder-node.h"
#include "xb-silo-private.h"

static void
xb_builder_node_attr_free (XbBuilderNodeAttr *attr)
{
	g_free (attr->name);
	g_free (attr->value);
	g_slice_free (XbBuilderNodeAttr, attr);
}

XbBuilderNode *
xb_builder_node_new (const gchar *element_name)
{
	XbBuilderNodeData *data;
	data = g_slice_new0 (XbBuilderNodeData);
	data->element_name = g_strdup (element_name);
	data->element_name_idx = XB_SILO_UNSET;
	data->attrs = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_builder_node_attr_free);
	data->text_idx = XB_SILO_UNSET;
	return g_node_new (data);
}

/* private */
void
xb_builder_node_add_attribute (XbBuilderNode *n, const gchar *name, const gchar *value)
{
	XbBuilderNodeData *data = n->data;
	XbBuilderNodeAttr *a = g_slice_new0 (XbBuilderNodeAttr);
	a->name = g_strdup (name);
	a->name_idx = XB_SILO_UNSET;
	a->value = g_strdup (value);
	a->value_idx = XB_SILO_UNSET;
	g_ptr_array_add (data->attrs, a);
}

guint32
xb_builder_node_size (XbBuilderNode *n)
{
	XbBuilderNodeData *data = n->data;
	guint32 sz = sizeof(XbSiloNode);
	return sz + data->attrs->len * sizeof(XbSiloAttr);
}

void
xb_builder_node_free (XbBuilderNode *n)
{
	XbBuilderNodeData *data = n->data;
	g_free (data->element_name);
	g_free (data->text);
	g_ptr_array_unref (data->attrs);
	g_slice_free (XbBuilderNodeData, data);
}
