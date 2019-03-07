/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbNode"

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>

#include "xb-node-private.h"
#include "xb-silo-export-private.h"

typedef struct {
	GObject			 parent_instance;
	XbSilo			*silo;
	XbSiloNode		*sn;
} XbNodePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XbNode, xb_node, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (xb_node_get_instance_private (o))

/**
 * xb_node_get_data:
 * @self: a #XbNode
 * @key: a string key, e.g. `fwupd::RemoteId`
 *
 * Gets any data that has been set on the node using xb_node_set_data().
 *
 * Returns: (transfer none): a #GBytes, or %NULL if not found
 *
 * Since: 0.1.0
 **/
GBytes *
xb_node_get_data (XbNode *self, const gchar *key)
{
	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	g_return_val_if_fail (key != NULL, NULL);
	return g_object_get_data (G_OBJECT (self), key);
}

/**
 * xb_node_set_data:
 * @self: a #XbNode
 * @key: a string key, e.g. `fwupd::RemoteId`
 * @data: a #GBytes
 *
 * Sets some data on the node which can be retrieved using xb_node_get_data().
 *
 * Since: 0.1.0
 **/
void
xb_node_set_data (XbNode *self, const gchar *key, GBytes *data)
{
	g_return_if_fail (XB_IS_NODE (self));
	g_return_if_fail (key != NULL);
	g_return_if_fail (data != NULL);
	g_object_set_data_full (G_OBJECT (self), key,
				g_bytes_ref (data),
				(GDestroyNotify) g_bytes_unref);
}

/**
 * xb_node_get_sn: (skip)
 * @self: a #XbNode
 *
 * Gets the #XbSiloNode for the node.
 *
 * Returns: (transfer none): a #XbSiloNode
 *
 * Since: 0.1.0
 **/
XbSiloNode *
xb_node_get_sn (XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	return priv->sn;
}

/**
 * xb_node_get_silo: (skip)
 * @self: a #XbNode
 *
 * Gets the #XbSilo for the node.
 *
 * Returns: (transfer none): a #XbSilo
 *
 * Since: 0.1.0
 **/
XbSilo *
xb_node_get_silo (XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	return priv->silo;
}

/**
 * xb_node_get_root:
 * @self: a #XbNode
 *
 * Gets the root node for the node.
 *
 * Returns: (transfer full): a #XbNode, or %NULL
 *
 * Since: 0.1.0
 **/
XbNode *
xb_node_get_root (XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	XbSiloNode *sn;

	g_return_val_if_fail (XB_IS_NODE (self), NULL);

	sn = xb_silo_get_sroot (priv->silo);
	if (sn == NULL)
		return NULL;
	return xb_silo_node_create (priv->silo, sn);
}

/**
 * xb_node_get_parent:
 * @self: a #XbNode
 *
 * Gets the parent node for the current node.
 *
 * Returns: (transfer full): a #XbNode, or %NULL
 *
 * Since: 0.1.0
 **/
XbNode *
xb_node_get_parent (XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	XbSiloNode *sn;

	g_return_val_if_fail (XB_IS_NODE (self), NULL);

	sn = xb_silo_node_get_parent (priv->silo, priv->sn);
	if (sn == NULL)
		return NULL;
	return xb_silo_node_create (priv->silo, sn);
}

/**
 * xb_node_get_next:
 * @self: a #XbNode
 *
 * Gets the next sibling node for the current node.
 *
 * Returns: (transfer none): a #XbNode, or %NULL
 *
 * Since: 0.1.0
 **/
XbNode *
xb_node_get_next (XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	XbSiloNode *sn;

	g_return_val_if_fail (XB_IS_NODE (self), NULL);

	sn = xb_silo_node_get_next (priv->silo, priv->sn);
	if (sn == NULL)
		return NULL;
	return xb_silo_node_create (priv->silo, sn);
}

/**
 * xb_node_get_child:
 * @self: a #XbNode
 *
 * Gets the first child node for the current node.
 *
 * Returns: (transfer none): a #XbNode, or %NULL
 *
 * Since: 0.1.0
 **/
XbNode *
xb_node_get_child (XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	XbSiloNode *sn;

	g_return_val_if_fail (XB_IS_NODE (self), NULL);

	sn = xb_silo_node_get_child (priv->silo, priv->sn);
	if (sn == NULL)
		return NULL;
	return xb_silo_node_create (priv->silo, sn);
}

/**
 * xb_node_get_children:
 * @self: a #XbNode
 *
 * Gets all the children for the current node.
 *
 * Returns: (transfer container) (element-type XbNode): an array of children
 *
 * Since: 0.1.0
 **/
GPtrArray *
xb_node_get_children (XbNode *self)
{
	XbNode *n;
	GPtrArray *array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* add all children */
	n = xb_node_get_child (self);
	while (n != NULL) {
		g_ptr_array_add (array, n);
		n = xb_node_get_next (n);
	}
	return array;
}

/**
 * xb_node_get_text:
 * @self: a #XbNode
 *
 * Gets the text data for a specific node.
 *
 * Returns: a string, or %NULL for unset
 *
 * Since: 0.1.0
 **/
const gchar *
xb_node_get_text (XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	return xb_silo_node_get_text (priv->silo, priv->sn);
}

/**
 * xb_node_get_text_as_uint:
 * @self: a #XbNode
 *
 * Gets some attribute text data for a specific node.
 *
 * Returns: a guint64, or %G_MAXUINT64 if unfound
 *
 * Since: 0.1.0
 **/
guint64
xb_node_get_text_as_uint (XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	const gchar *tmp;

	g_return_val_if_fail (XB_IS_NODE (self), G_MAXUINT64);

	tmp = xb_silo_node_get_text (priv->silo, priv->sn);;
	if (tmp == NULL)
		return G_MAXUINT64;
	if (g_str_has_prefix (tmp, "0x"))
		return g_ascii_strtoull (tmp + 2, NULL, 16);
	return g_ascii_strtoull (tmp, NULL, 10);
}

/**
 * xb_node_get_element:
 * @self: a #XbNode
 *
 * Gets the element name for a specific node.
 *
 * Returns: a string, or %NULL for unset
 *
 * Since: 0.1.0
 **/
const gchar *
xb_node_get_element (XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	return xb_silo_node_get_element (priv->silo, priv->sn);
}

/**
 * xb_node_get_attr:
 * @self: a #XbNode
 * @name: an attribute name, e.g. "type"
 *
 * Gets some attribute text data for a specific node.
 *
 * Returns: a string, or %NULL for unset
 *
 * Since: 0.1.0
 **/
const gchar *
xb_node_get_attr (XbNode *self, const gchar *name)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	XbSiloAttr *a;

	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	a = xb_silo_node_get_attr_by_str (priv->silo, priv->sn, name);
	if (a == NULL)
		return NULL;
	return xb_silo_from_strtab (priv->silo, a->attr_value);
}

/**
 * xb_node_get_attr_as_uint:
 * @self: a #XbNode
 * @name: an attribute name, e.g. `type`
 *
 * Gets some attribute text data for a specific node.
 *
 * Returns: a guint64, or %G_MAXUINT64 if unfound
 *
 * Since: 0.1.0
 **/
guint64
xb_node_get_attr_as_uint (XbNode *self, const gchar *name)
{
	const gchar *tmp;

	g_return_val_if_fail (XB_IS_NODE (self), G_MAXUINT64);
	g_return_val_if_fail (name != NULL, G_MAXUINT64);

	tmp = xb_node_get_attr (self, name);
	if (tmp == NULL)
		return G_MAXUINT64;
	if (g_str_has_prefix (tmp, "0x"))
		return g_ascii_strtoull (tmp + 2, NULL, 16);
	return g_ascii_strtoull (tmp, NULL, 10);
}

/**
 * xb_node_get_depth:
 * @self: a #XbNode
 *
 * Gets the depth of the node to a root.
 *
 * Returns: a integer, where 0 is the root node iself.
 *
 * Since: 0.1.0
 **/
guint
xb_node_get_depth (XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_NODE (self), 0);
	return xb_silo_node_get_depth (priv->silo, priv->sn);
}

/**
 * xb_node_export:
 * @self: a #XbNode
 * @flags: some #XbNodeExportFlags, e.g. #XB_NODE_EXPORT_FLAG_NONE
 * @error: the #GError, or %NULL
 *
 * Exports the node back to XML.
 *
 * Returns: XML data, or %NULL for an error
 *
 * Since: 0.1.0
 **/
gchar *
xb_node_export (XbNode *self, XbNodeExportFlags flags, GError **error)
{
	GString *xml;
	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	xml = xb_silo_export_with_root (xb_node_get_silo (self), self, flags, error);
	if (xml == NULL)
		return NULL;
	return g_string_free (xml, FALSE);
}

static void
xb_node_init (XbNode *self)
{
}

static void
xb_node_finalize (GObject *obj)
{
	G_OBJECT_CLASS (xb_node_parent_class)->finalize (obj);
}

static void
xb_node_class_init (XbNodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = xb_node_finalize;
}

/**
 * xb_node_new: (skip)
 * @silo: A #XbSilo
 * @sn: A #XbSiloNode
 *
 * Creates a new node.
 *
 * Returns: a new #XbNode
 *
 * Since: 0.1.0
 **/
XbNode *
xb_node_new (XbSilo *silo, XbSiloNode *sn)
{
	XbNode *self = g_object_new (XB_TYPE_NODE, NULL);
	XbNodePrivate *priv = GET_PRIVATE (self);
	priv->silo = silo;
	priv->sn = sn;
	return self;
}
