/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN "XbNode"

#include "config.h"

#include <gio/gio.h>
#include <glib-object.h>

#include "xb-node-private.h"
#include "xb-node-silo.h"
#include "xb-silo-export-private.h"

typedef struct {
	XbSilo *silo;
	XbSiloNode *sn;
} XbNodePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(XbNode, xb_node, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (xb_node_get_instance_private(o))

/**
 * XbNodeAttrIter:
 *
 * A #XbNodeAttrIter structure represents an iterator that can be used
 * to iterate over the attributes of a #XbNode. #XbNodeAttrIter
 * structures are typically allocated on the stack and then initialized
 * with xb_node_attr_iter_init().
 *
 * The iteration order of a #XbNodeAttrIter is not defined.
 *
 * Since: 0.3.4
 */

typedef struct {
	XbNode *node;
	guint8 position;
	gpointer dummy3;
	gpointer dummy4;
	gpointer dummy5;
	gpointer dummy6;
} RealAttrIter;

G_STATIC_ASSERT(sizeof(XbNodeAttrIter) == sizeof(RealAttrIter));

/**
 * XbNodeChildIter:
 *
 * A #XbNodeChildIter structure represents an iterator that can be used
 * to iterate over the children of a #XbNode. #XbNodeChildIter
 * structures are typically allocated on the stack and then initialized
 * with xb_node_child_iter_init().
 *
 * Since: 0.3.4
 */

typedef struct {
	XbNode *node;
	XbSiloNode *position;
	gboolean first_iter;
	gpointer dummy4;
	gpointer dummy5;
	gpointer dummy6;
} RealChildIter;

G_STATIC_ASSERT(sizeof(XbNodeChildIter) == sizeof(RealChildIter));

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
xb_node_get_data(XbNode *self, const gchar *key)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(XB_IS_NODE(self), NULL);
	g_return_val_if_fail(key != NULL, NULL);
	g_return_val_if_fail(priv->silo, NULL);
	return g_object_get_data(G_OBJECT(self), key);
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
xb_node_set_data(XbNode *self, const gchar *key, GBytes *data)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	g_return_if_fail(XB_IS_NODE(self));
	g_return_if_fail(key != NULL);
	g_return_if_fail(data != NULL);
	g_return_if_fail(priv->silo);
	g_object_set_data_full(G_OBJECT(self),
			       key,
			       g_bytes_ref(data),
			       (GDestroyNotify)g_bytes_unref);
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
xb_node_get_sn(XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
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
xb_node_get_silo(XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
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
xb_node_get_root(XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	XbSiloNode *sn;

	g_return_val_if_fail(XB_IS_NODE(self), NULL);

	sn = xb_silo_get_root_node(priv->silo);
	if (sn == NULL)
		return NULL;
	return xb_silo_create_node(priv->silo, sn, FALSE);
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
xb_node_get_parent(XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	XbSiloNode *sn;

	g_return_val_if_fail(XB_IS_NODE(self), NULL);

	sn = xb_silo_get_parent_node(priv->silo, priv->sn);
	if (sn == NULL)
		return NULL;
	return xb_silo_create_node(priv->silo, sn, FALSE);
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
xb_node_get_next(XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	XbSiloNode *sn;

	g_return_val_if_fail(XB_IS_NODE(self), NULL);

	sn = xb_silo_get_next_node(priv->silo, priv->sn);
	if (sn == NULL)
		return NULL;
	return xb_silo_create_node(priv->silo, sn, FALSE);
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
xb_node_get_child(XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	XbSiloNode *sn;

	g_return_val_if_fail(XB_IS_NODE(self), NULL);

	sn = xb_silo_get_child_node(priv->silo, priv->sn);
	if (sn == NULL)
		return NULL;
	return xb_silo_create_node(priv->silo, sn, FALSE);
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
xb_node_get_children(XbNode *self)
{
	XbNode *n;
	GPtrArray *array = g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);

	/* add all children */
	n = xb_node_get_child(self);
	while (n != NULL) {
		g_ptr_array_add(array, n);
		n = xb_node_get_next(n);
	}
	return array;
}

/**
 * xb_node_child_iter_init:
 * @iter: an uninitialized #XbNodeChildIter
 * @self: a #XbNode
 *
 * Initializes a child iterator for the node's children and associates
 * it with @self.
 * The #XbNodeChildIter structure is typically allocated on the stack
 * and does not need to be freed explicitly.
 *
 * Since: 0.3.4
 */
void
xb_node_child_iter_init(XbNodeChildIter *iter, XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	RealChildIter *ri = (RealChildIter *)iter;

	g_return_if_fail(iter != NULL);
	g_return_if_fail(XB_IS_NODE(self));

	ri->node = self;
	ri->position = xb_silo_get_child_node(priv->silo, priv->sn);
	ri->first_iter = TRUE;
}

/**
 * xb_node_child_iter_next:
 * @iter: an initialized #XbNodeAttrIter
 * @child: (out) (optional) (not nullable): Destination of the returned child
 *
 * Returns the current child and advances the iterator.
 * The retrieved #XbNode child needs to be dereferenced with g_object_unref().
 * Example:
 * |[<!-- language="C" -->
 * XbNodeChildIter iter;
 * g_autoptr(XbNode) child = NULL;
 *
 * xb_node_child_iter_init (&iter, node);
 * while (xb_node_child_iter_next (&iter, &child)) {
 *     // do something with the node child
 *     g_clear_pointer (&child, g_object_unref);
 * }
 * ]|
 *
 * Returns: %FALSE if the last child has been reached.
 *
 * Since: 0.3.4
 */
gboolean
xb_node_child_iter_next(XbNodeChildIter *iter, XbNode **child)
{
	XbNodePrivate *priv;
	RealChildIter *ri = (RealChildIter *)iter;

	g_return_val_if_fail(iter != NULL, FALSE);
	g_return_val_if_fail(child != NULL, FALSE);
	priv = GET_PRIVATE(ri->node);

	/* check if the iteration was finished */
	if (ri->position == NULL) {
		*child = NULL;
		return FALSE;
	}

	*child = xb_silo_create_node(priv->silo, ri->position, FALSE);
	ri->position = xb_silo_get_next_node(priv->silo, ri->position);

	return TRUE;
}

/**
 * xb_node_child_iter_loop: (skip)
 * @iter: an initialized #XbNodeAttrIter
 * @child: (out) (optional) (nullable): Destination of the returned child
 *
 * Returns the current child and advances the iterator.
 * On the first call to this function, the @child pointer is assumed to point
 * at uninitialised memory.
 * On any later calls, it is assumed that the same pointers
 * will be given and that they will point to the memory as set by the
 * previous call to this function. This allows the previous values to
 * be freed, as appropriate.
 *
 * Example:
 * |[<!-- language="C" -->
 * XbNodeChildIter iter;
 * XbNode *child;
 *
 * xb_node_child_iter_init (&iter, node);
 * while (xb_node_child_iter_loop (&iter, &child)) {
 *     // do something with the node child
 *     // no need to free 'child' unless breaking out of this loop
 * }
 * ]|
 *
 * Returns: %FALSE if the last child has been reached.
 *
 * Since: 0.3.4
 */
gboolean
xb_node_child_iter_loop(XbNodeChildIter *iter, XbNode **child)
{
	XbNodePrivate *priv;
	RealChildIter *ri = (RealChildIter *)iter;

	g_return_val_if_fail(iter != NULL, FALSE);
	g_return_val_if_fail(child != NULL, FALSE);
	priv = GET_PRIVATE(ri->node);

	/* unref child from previous iterations, if there were any */
	if (ri->first_iter)
		ri->first_iter = FALSE;
	else
		g_object_unref(*child);

	/* check if the iteration was finished */
	if (ri->position == NULL) {
		*child = NULL;
		return FALSE;
	}

	*child = xb_silo_create_node(priv->silo, ri->position, FALSE);
	ri->position = xb_silo_get_next_node(priv->silo, ri->position);

	return TRUE;
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
xb_node_get_text(XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(XB_IS_NODE(self), NULL);
	return xb_silo_get_node_text(priv->silo, priv->sn);
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
xb_node_get_text_as_uint(XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	const gchar *tmp;

	g_return_val_if_fail(XB_IS_NODE(self), G_MAXUINT64);

	tmp = xb_silo_get_node_text(priv->silo, priv->sn);
	;
	if (tmp == NULL)
		return G_MAXUINT64;
	if (g_str_has_prefix(tmp, "0x"))
		return g_ascii_strtoull(tmp + 2, NULL, 16);
	return g_ascii_strtoull(tmp, NULL, 10);
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
xb_node_get_tail(XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(XB_IS_NODE(self), NULL);
	return xb_silo_get_node_tail(priv->silo, priv->sn);
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
xb_node_get_element(XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(XB_IS_NODE(self), NULL);
	return xb_silo_get_node_element(priv->silo, priv->sn);
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
xb_node_get_attr(XbNode *self, const gchar *name)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	XbSiloNodeAttr *a;

	g_return_val_if_fail(XB_IS_NODE(self), NULL);
	g_return_val_if_fail(name != NULL, NULL);

	a = xb_silo_get_node_attr_by_str(priv->silo, priv->sn, name);
	if (a == NULL)
		return NULL;
	return xb_silo_from_strtab(priv->silo, a->attr_value);
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
xb_node_get_attr_as_uint(XbNode *self, const gchar *name)
{
	const gchar *tmp;

	g_return_val_if_fail(XB_IS_NODE(self), G_MAXUINT64);
	g_return_val_if_fail(name != NULL, G_MAXUINT64);

	tmp = xb_node_get_attr(self, name);
	if (tmp == NULL)
		return G_MAXUINT64;
	if (g_str_has_prefix(tmp, "0x"))
		return g_ascii_strtoull(tmp + 2, NULL, 16);
	return g_ascii_strtoull(tmp, NULL, 10);
}

/**
 * xb_node_attr_iter_init:
 * @iter: an uninitialized #XbNodeAttrIter
 * @self: a #XbNode
 *
 * Initializes a name/value pair iterator for the node attributes
 * and associates it with @self.
 * The #XbNodeAttrIter structure is typically allocated on the stack
 * and does not need to be freed explicitly.
 *
 * Since: 0.3.4
 */
void
xb_node_attr_iter_init(XbNodeAttrIter *iter, XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	RealAttrIter *ri = (RealAttrIter *)iter;

	g_return_if_fail(iter != NULL);
	g_return_if_fail(XB_IS_NODE(self));

	ri->node = self;
	ri->position = xb_silo_node_get_attr_count(priv->sn);
}

/**
 * xb_node_attr_iter_next:
 * @iter: an initialized #XbNodeAttrIter
 * @name: (out) (optional) (not nullable): Destination of the returned attribute name
 * @value: (out) (optional) (not nullable): Destination of the returned attribute value
 *
 * Returns the current attribute name and value and advances the iterator.
 * Example:
 * |[<!-- language="C" -->
 * XbNodeAttrIter iter;
 * const gchar *attr_name, *attr_value;
 *
 * xb_node_attr_iter_init (&iter, node);
 * while (xb_node_attr_iter_next (&iter, &attr_name, &attr_value)) {
 *     // use attr_name and attr_value; no need to free them
 * }
 * ]|
 *
 * Returns: %TRUE if there are more attributes.
 *
 * Since: 0.3.4
 */
gboolean
xb_node_attr_iter_next(XbNodeAttrIter *iter, const gchar **name, const gchar **value)
{
	XbSiloNodeAttr *a;
	XbNodePrivate *priv;
	RealAttrIter *ri = (RealAttrIter *)iter;

	g_return_val_if_fail(iter != NULL, FALSE);
	priv = GET_PRIVATE(ri->node);

	/* check if the iteration was finished */
	if (ri->position == 0) {
		if (name != NULL)
			*name = NULL;
		if (value != NULL)
			*value = NULL;
		return FALSE;
	}

	ri->position--;
	a = xb_silo_node_get_attr(priv->sn, ri->position);
	if (name != NULL)
		*name = xb_silo_from_strtab(priv->silo, a->attr_name);
	if (value != NULL)
		*value = xb_silo_from_strtab(priv->silo, a->attr_value);

	return TRUE;
}

/**
 * xb_node_get_depth:
 * @self: a #XbNode
 *
 * Gets the depth of the node to a root.
 *
 * Returns: a integer, where 0 is the root node itself.
 *
 * Since: 0.1.0
 **/
guint
xb_node_get_depth(XbNode *self)
{
	XbNodePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(XB_IS_NODE(self), 0);
	return xb_silo_get_node_depth(priv->silo, priv->sn);
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
xb_node_export(XbNode *self, XbNodeExportFlags flags, GError **error)
{
	GString *xml;
	g_return_val_if_fail(XB_IS_NODE(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);
	xml = xb_silo_export_with_root(xb_node_get_silo(self), xb_node_get_sn(self), flags, error);
	if (xml == NULL)
		return NULL;
	return g_string_free(xml, FALSE);
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
xb_node_transmogrify(XbNode *self,
		     XbNodeTransmogrifyFunc func_text,
		     XbNodeTransmogrifyFunc func_tail,
		     gpointer user_data)
{
	g_autoptr(XbNode) n = NULL;

	g_return_val_if_fail(XB_IS_NODE(self), FALSE);

	/* all siblings */
	n = g_object_ref(self);
	while (n != NULL) {
		g_autoptr(XbNode) c = NULL;
		g_autoptr(XbNode) tmp = NULL;

		/* head */
		if (func_text != NULL) {
			if (func_text(n, user_data))
				return FALSE;
		}

		/* all children */
		c = xb_node_get_child(n);
		if (c != NULL) {
			if (!xb_node_transmogrify(c, func_text, func_tail, user_data))
				return FALSE;
		}

		/* tail */
		if (func_tail != NULL) {
			if (func_tail(n, user_data))
				return FALSE;
		}

		/* next sibling */
		tmp = xb_node_get_next(n);
		g_set_object(&n, tmp);
	}
	return TRUE;
}

static void
xb_node_init(XbNode *self)
{
}

static void
xb_node_finalize(GObject *obj)
{
	G_OBJECT_CLASS(xb_node_parent_class)->finalize(obj);
}

static void
xb_node_class_init(XbNodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
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
xb_node_new(XbSilo *silo, XbSiloNode *sn)
{
	XbNode *self = g_object_new(XB_TYPE_NODE, NULL);
	XbNodePrivate *priv = GET_PRIVATE(self);
	priv->silo = silo;
	priv->sn = sn;
	return self;
}
