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
#include "xb-node-silo.h"
#include "xb-silo-export-private.h"

typedef struct {
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
 * This will only work across queries to the associated silo if the silo has
 * its #XbSilo:enable-node-cache property set to %TRUE. Otherwise a new #XbNode
 * may be constructed for future queries which return the same element as a
 * result.
 *
 * Returns: (transfer none): a #GBytes, or %NULL if not found
 *
 * Since: 0.1.0
 **/
GBytes *
xb_node_get_data (XbNode *self, const gchar *key)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (priv->silo, NULL);
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
 * This will only work across queries to the associated silo if the silo has
 * its #XbSilo:enable-node-cache property set to %TRUE. Otherwise a new #XbNode
 * may be constructed for future queries which return the same element as a
 * result.
 *
 * Since: 0.1.0
 **/
void
xb_node_set_data (XbNode *self, const gchar *key, GBytes *data)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_NODE (self));
	g_return_if_fail (key != NULL);
	g_return_if_fail (data != NULL);
	g_return_if_fail (priv->silo);
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
 * xb_node_get_silo:
 * @self: a #XbNode
 *
 * Gets the #XbSilo for the node.
 *
 * Returns: (transfer none): a #XbSilo
 *
 * Since: 0.2.0
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

	sn = xb_silo_get_root_node (priv->silo);
	if (sn == NULL)
		return NULL;
	return xb_silo_create_node (priv->silo, sn, FALSE);
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

	sn = xb_silo_get_parent_node (priv->silo, priv->sn);
	if (sn == NULL)
		return NULL;
	return xb_silo_create_node (priv->silo, sn, FALSE);
}

/**
 * xb_node_get_next:
 * @self: a #XbNode
 *
 * Gets the next sibling node for the current node.
 *
 * Returns: (transfer full): a #XbNode, or %NULL
 *
 * Since: 0.1.0
 **/
XbNode *
xb_node_get_next (XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	XbSiloNode *sn;

	g_return_val_if_fail (XB_IS_NODE (self), NULL);

	sn = xb_silo_get_next_node (priv->silo, priv->sn);
	if (sn == NULL)
		return NULL;
	return xb_silo_create_node (priv->silo, sn, FALSE);
}

/**
 * xb_node_get_child:
 * @self: a #XbNode
 *
 * Gets the first child node for the current node.
 *
 * Returns: (transfer full): a #XbNode, or %NULL
 *
 * Since: 0.1.0
 **/
XbNode *
xb_node_get_child (XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	XbSiloNode *sn;

	g_return_val_if_fail (XB_IS_NODE (self), NULL);

	sn = xb_silo_get_child_node (priv->silo, priv->sn);
	if (sn == NULL)
		return NULL;
	return xb_silo_create_node (priv->silo, sn, FALSE);
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
	return xb_silo_get_node_text (priv->silo, priv->sn);
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

	tmp = xb_silo_get_node_text (priv->silo, priv->sn);;
	if (tmp == NULL)
		return G_MAXUINT64;
	if (g_str_has_prefix (tmp, "0x"))
		return g_ascii_strtoull (tmp + 2, NULL, 16);
	return g_ascii_strtoull (tmp, NULL, 10);
}

/**
 * xb_node_get_tail:
 * @self: a #XbNode
 *
 * Gets the tail data for a specific node.
 *
 * Returns: a string, or %NULL for unset
 *
 * Since: 0.1.12
 **/
const gchar *
xb_node_get_tail (XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	return xb_silo_get_node_tail (priv->silo, priv->sn);
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
	return xb_silo_get_node_element (priv->silo, priv->sn);
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
	XbSiloNodeAttr *a;

	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	a = xb_silo_get_node_attr_by_str (priv->silo, priv->sn, name);
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
	return xb_silo_get_node_depth (priv->silo, priv->sn);
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
	xml = xb_silo_export_with_root (xb_node_get_silo (self), xb_node_get_sn (self), flags, error);
	if (xml == NULL)
		return NULL;
	return g_string_free (xml, FALSE);
}

/**
 * xb_node_transmogrify:
 * @self: a #XbNode
 * @func_text: (scope call): (allow-none): a #XbBuilderNodeTraverseFunc
 * @func_tail: (scope call): (allow-none): a #XbBuilderNodeTraverseFunc
 * @user_data: user pointer to pass to @func, or %NULL
 *
 * Traverses a tree starting from @self. It calls the given functions for each
 * node visited. This allows transmogrification of the source, for instance
 * converting the XML description to PangoMarkup or even something completely
 * different like markdown.
 *
 * The traversal can be halted at any point by returning TRUE from @func.
 *
 * Returns: %TRUE if all nodes were visited
 *
 * Since: 0.1.12
 **/
gboolean
xb_node_transmogrify (XbNode *self,
		      XbNodeTransmogrifyFunc func_text,
		      XbNodeTransmogrifyFunc func_tail,
		      gpointer user_data)
{
	g_autoptr(XbNode) n = NULL;

	g_return_val_if_fail (XB_IS_NODE (self), FALSE);

	/* all siblings */
	n = g_object_ref (self);
	while (n != NULL) {
		g_autoptr(XbNode) c = NULL;
		g_autoptr(XbNode) tmp = NULL;

		/* head */
		if (func_text != NULL) {
			if (func_text (n, user_data))
				return FALSE;
		}

		/* all children */
		c = xb_node_get_child (n);
		if (c != NULL) {
			if (!xb_node_transmogrify (c, func_text, func_tail, user_data))
				return FALSE;
		}

		/* tail */
		if (func_tail != NULL) {
			if (func_tail (n, user_data))
				return FALSE;
		}

		/* next sibling */
		tmp = xb_node_get_next (n);
		g_set_object (&n, tmp);
	}
	return TRUE;
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
