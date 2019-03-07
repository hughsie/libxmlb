/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <gio/gio.h>
#include <string.h>

#include "xb-builder-node-private.h"
#include "xb-silo-private.h"
#include "xb-string-private.h"

typedef struct {
	GObject			 parent_instance;
	guint32			 offset;
	gint			 priority;
	XbBuilderNodeFlags	 flags;
	gchar			*element;
	guint32			 element_idx;
	gchar			*text;
	guint32			 text_idx;
	XbBuilderNode		*parent;	/* noref */
	GPtrArray		*children;	/* of XbBuilderNode */
	GPtrArray		*attrs;		/* of XbBuilderNodeAttr */
} XbBuilderNodePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XbBuilderNode, xb_builder_node, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (xb_builder_node_get_instance_private (o))

/**
 * xb_builder_node_has_flag:
 * @self: a #XbBuilderNode
 * @flag: a #XbBuilderNodeFlags
 *
 * Checks a flag on the builder node.
 *
 * Returns: %TRUE if @flag is set
 *
 * Since: 0.1.0
 **/
gboolean
xb_builder_node_has_flag (XbBuilderNode *self, XbBuilderNodeFlags flag)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), FALSE);
	return (priv->flags & flag) > 0;
}

/**
 * xb_builder_node_add_flag:
 * @self: a #XbBuilderNode
 * @flag: a #XbBuilderNodeFlags
 *
 * Adds a flag to the builder node.
 *
 * Since: 0.1.0
 **/
void
xb_builder_node_add_flag (XbBuilderNode *self, XbBuilderNodeFlags flag)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	priv->flags |= flag;
	for (guint i = 0; i < priv->children->len; i++) {
		XbBuilderNode *c = g_ptr_array_index (priv->children, i);
		xb_builder_node_add_flag (c, flag);
	}
}

/**
 * xb_builder_node_get_element:
 * @self: a #XbBuilderNode
 *
 * Gets the element from the builder node.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.0
 **/
const gchar *
xb_builder_node_get_element (XbBuilderNode *self)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), NULL);
	return priv->element;
}

/**
 * xb_builder_node_set_element:
 * @self: a #XbBuilderNode
 * @element: a string element
 *
 * Sets the element name on the builder node.
 *
 * Since: 0.1.0
 **/
void
xb_builder_node_set_element (XbBuilderNode *self, const gchar *element)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	g_free (priv->element);
	priv->element = g_strdup (element);
}

/**
 * xb_builder_node_get_attr:
 * @self: a #XbBuilderNode
 * @name: attribute name, e.g. `type`
 *
 * Gets an attribute from the builder node.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.0
 **/
const gchar *
xb_builder_node_get_attr (XbBuilderNode *self, const gchar *name)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), NULL);
	g_return_val_if_fail (name != NULL, NULL);
	for (guint i = 0; i < priv->attrs->len; i++) {
		XbBuilderNodeAttr *a = g_ptr_array_index (priv->attrs, i);
		if (g_strcmp0 (a->name, name) == 0)
			return a->value;
	}
	return NULL;
}

/**
 * xb_builder_node_get_attr_as_uint:
 * @self: a #XbBuilderNode
 * @name: attribute name, e.g. `priority`
 *
 * Gets an attribute from the builder node.
 *
 * Returns: integer, or 0 if unset
 *
 * Since: 0.1.3
 **/
guint64
xb_builder_node_get_attr_as_uint (XbBuilderNode *self, const gchar *name)
{
	const gchar *tmp = xb_builder_node_get_attr (self, name);
	if (tmp == NULL)
		return 0;
	if (g_str_has_prefix (tmp, "0x"))
		return g_ascii_strtoull (tmp + 2, NULL, 16);
	return g_ascii_strtoll (tmp, NULL, 10);
}

/**
 * xb_builder_node_get_text:
 * @self: a #XbBuilderNode
 *
 * Gets the text from the builder node.
 *
 * Returns: string, or %NULL if unset
 *
 * Since: 0.1.0
 **/
const gchar *
xb_builder_node_get_text (XbBuilderNode *self)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), NULL);
	return priv->text;
}

/**
 * xb_builder_node_get_text_as_uint:
 * @self: a #XbBuilderNode
 *
 * Gets the text from the builder node.
 *
 * Returns: integer, or 0 if unset
 *
 * Since: 0.1.3
 **/
guint64
xb_builder_node_get_text_as_uint (XbBuilderNode *self)
{
	const gchar *tmp = xb_builder_node_get_text (self);
	if (tmp == NULL)
		return 0;
	if (g_str_has_prefix (tmp, "0x"))
		return g_ascii_strtoull (tmp + 2, NULL, 16);
	return g_ascii_strtoll (tmp, NULL, 10);
}

/* private */
GPtrArray *
xb_builder_node_get_attrs (XbBuilderNode *self)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), NULL);
	return priv->attrs;
}

/**
 * xb_builder_node_set_text:
 * @self: a #XbBuilderNode
 * @text: a string
 * @text_len: length of @text, or -1 if @text is NUL terminated
 *
 * Sets the text on the builder node.
 *
 * Since: 0.1.0
 **/
void
xb_builder_node_set_text (XbBuilderNode *self, const gchar *text, gssize text_len)
{
	GString *tmp;
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	guint newline_count = 0;
	g_auto(GStrv) split = NULL;
	gsize text_len_safe;

	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	g_return_if_fail (text != NULL);

	/* old data */
	g_free (priv->text);

	/* we know this has been pre-fixed */
	text_len_safe = text_len >= 0 ? (gsize) text_len : strlen (text);
	if (xb_builder_node_has_flag (self, XB_BUILDER_NODE_FLAG_LITERAL_TEXT)) {
		priv->text = g_strndup (text, text_len_safe);
		return;
	}

	/* all on one line, no trailing or leading whitespace */
	if (g_strstr_len (text, text_len, "\n") == NULL &&
	    !g_str_has_prefix (text, " ") &&
	    !g_str_has_suffix (text, " ")) {
		priv->text = g_strndup (text, text_len_safe);
		return;
	}

	/* split the text into lines */
	tmp = g_string_sized_new ((gsize) text_len_safe + 1);
	split = g_strsplit (text, "\n", -1);
	for (guint i = 0; split[i] != NULL; i++) {

		/* remove leading and trailing whitespace */
		g_strstrip (split[i]);

		/* if this is a blank line we end the paragraph mode
		 * and swallow the newline. If we see exactly two
		 * newlines in sequence then do a paragraph break */
		if (split[i][0] == '\0') {
			newline_count++;
			continue;
		}

		/* if the line just before this one was not a newline
		 * then seporate the words with a space */
		if (newline_count == 1 && tmp->len > 0)
			g_string_append (tmp, " ");

		/* if we had more than one newline in sequence add a paragraph
		 * break */
		if (newline_count > 1)
			g_string_append (tmp, "\n\n");

		/* add the actual stripped text */
		g_string_append (tmp, split[i]);

		/* this last section was paragraph */
		newline_count = 1;
	}

	/* success */
	priv->text = g_string_free (tmp, FALSE);
}

/**
 * xb_builder_node_set_attr:
 * @self: a #XbBuilderNode
 * @name: attribute name, e.g. `type`
 * @value: attribute value, e.g. `desktop`
 *
 * Adds an attribute to the builder node.
 *
 * Since: 0.1.0
 **/
void
xb_builder_node_set_attr (XbBuilderNode *self, const gchar *name, const gchar *value)
{
	XbBuilderNodeAttr *a;
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);

	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	g_return_if_fail (name != NULL);

	/* check for existing name */
	for (guint i = 0; i < priv->attrs->len; i++) {
		a = g_ptr_array_index (priv->attrs, i);
		if (g_strcmp0 (a->name, name) == 0) {
			g_free (a->value);
			a->value = g_strdup (value);
			return;
		}
	}

	/* create new */
	a = g_slice_new0 (XbBuilderNodeAttr);
	a->name = g_strdup (name);
	a->name_idx = XB_SILO_UNSET;
	a->value = g_strdup (value);
	a->value_idx = XB_SILO_UNSET;
	g_ptr_array_add (priv->attrs, a);
}

/**
 * xb_builder_node_remove_attr:
 * @self: a #XbBuilderNode
 * @name: attribute name, e.g. `type`
 *
 * Removes an attribute from the builder node.
 *
 * Since: 0.1.0
 **/
void
xb_builder_node_remove_attr (XbBuilderNode *self, const gchar *name)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);

	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	g_return_if_fail (name != NULL);

	for (guint i = 0; i < priv->attrs->len; i++) {
		XbBuilderNodeAttr *a = g_ptr_array_index (priv->attrs, i);
		if (g_strcmp0 (a->name, name) == 0) {
			g_ptr_array_remove_index (priv->attrs, i);
			break;
		}
	}
}

/**
 * xb_builder_node_depth:
 * @self: a #XbBuilderNode
 *
 * Gets the depth of the node tree, where 0 is the root node.
 *
 * Since: 0.1.1
 **/
guint
xb_builder_node_depth (XbBuilderNode *self)
{
	for (guint i = 0; ; i++) {
		XbBuilderNodePrivate *priv = GET_PRIVATE (self);
		if (priv->parent == NULL)
			return i;
		self = priv->parent;
	}
	return 0;
}

/**
 * xb_builder_node_add_child:
 * @self: A XbBuilderNode
 * @child: A XbBuilderNode
 *
 * Adds a child builder node.
 *
 * Since: 0.1.0
 **/
void
xb_builder_node_add_child (XbBuilderNode *self, XbBuilderNode *child)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	XbBuilderNodePrivate *priv_child = GET_PRIVATE (child);
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	g_return_if_fail (XB_IS_BUILDER_NODE (child));
	g_return_if_fail (priv_child->parent == NULL);

	/* no refcount */
	priv_child->parent = self;
	g_object_add_weak_pointer (G_OBJECT (self), (gpointer *) &priv_child->parent);

	g_ptr_array_add (priv->children, g_object_ref (child));
}

/**
 * xb_builder_node_remove_child:
 * @self: A XbBuilderNode
 * @child: A XbBuilderNode
 *
 * Removes a child builder node.
 *
 * Since: 0.1.1
 **/
void
xb_builder_node_remove_child (XbBuilderNode *self, XbBuilderNode *child)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	XbBuilderNodePrivate *priv_child = GET_PRIVATE (child);

	/* no refcount */
	g_object_remove_weak_pointer (G_OBJECT (self), (gpointer *) &priv_child->parent);
	priv_child->parent = NULL;

	g_ptr_array_remove (priv->children, child);
}

/**
 * xb_builder_node_unlink:
 * @self: a #XbBuilderNode
 *
 * Unlinks a #XbBuilderNode from a tree, resulting in two separate trees.
 *
 * This should not be used from the function called by xb_builder_node_traverse()
 * otherwise the entire tree will not be traversed.
 *
 * Instead use xb_builder_node_add_flag(bn,XB_BUILDER_NODE_FLAG_IGNORE);
 *
 * Since: 0.1.1
 **/
void
xb_builder_node_unlink (XbBuilderNode *self)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	if (priv->parent == NULL)
		return;
	xb_builder_node_remove_child (priv->parent, self);
}

/**
 * xb_builder_node_get_parent:
 * @self: a #XbBuilderNode
 *
 * Gets the parent node for the current node.
 *
 * Returns: (transfer full): a new #XbBuilderNode, or %NULL no parent exists.
 *
 * Since: 0.1.1
 **/
XbBuilderNode *
xb_builder_node_get_parent (XbBuilderNode *self)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), NULL);
	if (priv->parent == NULL)
		return NULL;
	return g_object_ref (priv->parent);
}

/**
 * xb_builder_node_get_children:
 * @self: a #XbBuilderNode
 *
 * Gets the children of the builder node.
 *
 * Returns: (transfer none) (element-type XbBuilderNode): children
 *
 * Since: 0.1.0
 **/
GPtrArray *
xb_builder_node_get_children (XbBuilderNode *self)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), NULL);
	return priv->children;
}

/**
 * xb_builder_node_get_child:
 * @self: a #XbBuilderNode
 * @element: An element name, e.g. "url"
 * @text: (allow-none): node text, e.g. "gimp.desktop"
 *
 * Finds a child builder node by the element name, and optionally text value.
 *
 * Returns: (transfer full): a new #XbBuilderNode, or %NULL if not found
 *
 * Since: 0.1.1
 **/
XbBuilderNode *
xb_builder_node_get_child (XbBuilderNode *self, const gchar *element, const gchar *text)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);

	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), NULL);
	g_return_val_if_fail (element != NULL, NULL);

	for (guint i = 0; i < priv->children->len; i++) {
		XbBuilderNode *child = g_ptr_array_index (priv->children, i);
		if (g_strcmp0 (xb_builder_node_get_element (child), element) != 0)
			continue;
		if (text != NULL && g_strcmp0 (xb_builder_node_get_text (child), text) != 0)
			continue;
		return g_object_ref (child);
	}
	return NULL;
}

typedef struct {
	gint				 max_depth;
	XbBuilderNodeTraverseFunc	 func;
	gpointer			 user_data;
	GTraverseFlags			 flags;
	GTraverseType			 order;
} XbBuilderNodeTraverseHelper;

static void
xb_builder_node_traverse_cb (XbBuilderNodeTraverseHelper *helper,
			     XbBuilderNode *bn,
			     gint depth)
{
	GPtrArray *children = xb_builder_node_get_children (bn);

	/* only leaves */
	if (helper->flags == G_TRAVERSE_LEAVES &&
	    children->len > 0)
		return;

	/* only non-leaves */
	if (helper->flags == G_TRAVERSE_NON_LEAVES &&
	    children->len == 0)
		return;

	/* recurse */
	if (helper->order == G_PRE_ORDER) {
		if (helper->func (bn, helper->user_data))
			return;
	}
	if (helper->max_depth < 0 || depth < helper->max_depth) {
		for (guint i = 0; i < children->len; i++) {
			XbBuilderNode *bc = g_ptr_array_index (children, i);
			xb_builder_node_traverse_cb (helper, bc, depth + 1);
		}
	}
	if (helper->order == G_POST_ORDER) {
		if (helper->func (bn, helper->user_data))
			return;
	}
}

/**
 * xb_builder_node_traverse:
 * @self: a #XbBuilderNode
 * @order: a #GTraverseType, e.g. %G_PRE_ORDER
 * @flags: a #GTraverseFlags, e.g. %G_TRAVERSE_ALL
 * @max_depth: the maximum depth of the traversal, or -1 for no limit
 * @func: (scope call): a #XbBuilderNodeTraverseFunc
 * @user_data: user pointer to pass to @func, or %NULL
 *
 * Traverses a tree starting from @self. It calls the given function for each
 * node visited.
 *
 * The traversal can be halted at any point by returning TRUE from @func.
 *
 * Since: 0.1.1
 **/
void
xb_builder_node_traverse (XbBuilderNode *self,
			  GTraverseType order,
			  GTraverseFlags flags,
			  gint max_depth,
			  XbBuilderNodeTraverseFunc func,
			  gpointer user_data)
{
	XbBuilderNodeTraverseHelper helper = {
		.max_depth = max_depth,
		.order = order,
		.flags = flags,
		.func = func,
		.user_data = user_data,
	};
	if (order == G_PRE_ORDER || order == G_POST_ORDER) {
		xb_builder_node_traverse_cb (&helper, self, 0);
		return;
	}
	g_critical ("order %u not supported", order);
}

typedef struct {
	XbBuilderNodeSortFunc func;
	gpointer user_data;
} XbBuilderNodeSortHelper;

static gint
xb_builder_node_sort_children_cb (gconstpointer a, gconstpointer b, gpointer user_data)
{
	XbBuilderNodeSortHelper *helper = (XbBuilderNodeSortHelper *) user_data;
	XbBuilderNode *bn1 = *((XbBuilderNode **) a);
	XbBuilderNode *bn2 = *((XbBuilderNode **) b);
	return helper->func (bn1, bn2, helper->user_data);
}

/**
 * xb_builder_node_sort_children:
 * @self: a #XbBuilderNode
 * @func: (scope call): a #XbBuilderNodeSortFunc
 * @user_data: user pointer to pass to @func, or %NULL
 *
 * Sorts the node children using a custom sort function.
 *
 * Since: 0.1.3
 **/
void
xb_builder_node_sort_children (XbBuilderNode *self,
			       XbBuilderNodeSortFunc func,
			       gpointer user_data)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	XbBuilderNodeSortHelper helper = {
		.func = func,
		.user_data = user_data,
	};
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	g_return_if_fail (func != NULL);
	g_ptr_array_sort_with_data (priv->children,
				    xb_builder_node_sort_children_cb,
				    &helper);
}

/* private */
guint32
xb_builder_node_get_offset (XbBuilderNode *self)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), 0);
	return priv->offset;
}

/* private */
void
xb_builder_node_set_offset (XbBuilderNode *self, guint32 offset)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	priv->offset = offset;
}

/* private */
gint
xb_builder_node_get_priority (XbBuilderNode *self)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), 0);
	return priv->priority;
}

/* private */
void
xb_builder_node_set_priority (XbBuilderNode *self, gint priority)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	priv->priority = priority;
}

/* private */
guint32
xb_builder_node_get_element_idx (XbBuilderNode *self)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), 0);
	return priv->element_idx;
}

/* private */
void
xb_builder_node_set_element_idx (XbBuilderNode *self, guint32 element_idx)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	priv->element_idx = element_idx;
}

/* private */
guint32
xb_builder_node_get_text_idx (XbBuilderNode *self)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), 0);
	return priv->text_idx;
}

/* private */
void
xb_builder_node_set_text_idx (XbBuilderNode *self, guint32 text_idx)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	priv->text_idx = text_idx;
}

/* private */
guint32
xb_builder_node_size (XbBuilderNode *self)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	guint32 sz = sizeof(XbSiloNode);
	return sz + priv->attrs->len * sizeof(XbSiloAttr);
}

static void
xb_builder_node_attr_free (XbBuilderNodeAttr *attr)
{
	g_free (attr->name);
	g_free (attr->value);
	g_slice_free (XbBuilderNodeAttr, attr);
}

static void
xb_builder_node_init (XbBuilderNode *self)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	priv->element_idx = XB_SILO_UNSET;
	priv->text_idx = XB_SILO_UNSET;
	priv->attrs = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_builder_node_attr_free);
	priv->children = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

static void
xb_builder_node_finalize (GObject *obj)
{
	XbBuilderNode *self = XB_BUILDER_NODE (obj);
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	g_free (priv->element);
	g_free (priv->text);
	g_ptr_array_unref (priv->attrs);
	g_ptr_array_unref (priv->children);
	G_OBJECT_CLASS (xb_builder_node_parent_class)->finalize (obj);
}

static void
xb_builder_node_class_init (XbBuilderNodeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = xb_builder_node_finalize;
}

/**
 * xb_builder_node_new:
 * @element: An element name, e.g. "component"
 *
 * Creates a new builder node.
 *
 * Returns: (transfer full): a new #XbBuilderNode
 *
 * Since: 0.1.0
 **/
XbBuilderNode *
xb_builder_node_new (const gchar *element)
{
	XbBuilderNode *self = g_object_new (XB_TYPE_BUILDER_NODE, NULL);
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);
	priv->element = g_strdup (element);
	return self;
}

/**
 * xb_builder_node_insert: (skip)
 * @parent: A XbBuilderNode, or %NULL
 * @element: An element name, e.g. "component"
 * @...: any attributes to add to the node, terminated by %NULL
 *
 * Creates a new builder node.
 *
 * Returns: (transfer full): a new #XbBuilderNode
 *
 * Since: 0.1.0
 **/
XbBuilderNode *
xb_builder_node_insert (XbBuilderNode *parent, const gchar *element, ...)
{
	XbBuilderNode *self = xb_builder_node_new (element);
	va_list args;
	const gchar *key;
	const gchar *value;

	/* add this node to the parent */
	if (parent != NULL)
		xb_builder_node_add_child (parent, self);

	/* process the attrs valist */
	va_start (args, element);
	for (guint i = 0;; i++) {
		key = va_arg (args, const gchar *);
		if (key == NULL)
			break;
		value = va_arg (args, const gchar *);
		if (value == NULL)
			break;
		xb_builder_node_set_attr (self, key, value);
	}
	va_end (args);

	return self;
}

/**
 * xb_builder_node_insert_text: (skip)
 * @parent: A XbBuilderNode, or %NULL
 * @element: An element name, e.g. "id"
 * @text: (allow-none): node text, e.g. "gimp.desktop"
 * @...: any attributes to add to the node, terminated by %NULL
 *
 * Creates a new builder node with optional node text.
 *
 * Since: 0.1.0
 **/
void
xb_builder_node_insert_text (XbBuilderNode *parent,
			     const gchar *element,
			     const gchar *text,
			     ...)
{
	g_autoptr(XbBuilderNode) self = xb_builder_node_new (element);
	va_list args;
	const gchar *key;
	const gchar *value;

	g_return_if_fail (parent != NULL);

	/* add this node to the parent */
	xb_builder_node_add_child (parent, self);
	if (text != NULL)
		xb_builder_node_set_text (self, text, -1);

	/* process the attrs valist */
	va_start (args, text);
	for (guint i = 0;; i++) {
		key = va_arg (args, const gchar *);
		if (key == NULL)
			break;
		value = va_arg (args, const gchar *);
		if (value == NULL)
			break;
		xb_builder_node_set_attr (self, key, value);
	}
	va_end (args);
}

typedef struct {
	GString			*xml;
	XbNodeExportFlags	 flags;
	guint			 level;
} XbBuilderNodeExportHelper;

static gboolean
xb_builder_node_export_helper (XbBuilderNode *self,
			       XbBuilderNodeExportHelper *helper,
			       GError **error)
{
	XbBuilderNodePrivate *priv = GET_PRIVATE (self);

	/* add start of opening tag */
	if (helper->flags & XB_NODE_EXPORT_FLAG_FORMAT_INDENT) {
		for (guint i = 0; i < helper->level; i++)
			g_string_append (helper->xml, "  ");
	}
	g_string_append_printf (helper->xml, "<%s", priv->element);

	/* add any attributes */
	for (guint i = 0; i < priv->attrs->len; i++) {
		XbBuilderNodeAttr *a = g_ptr_array_index (priv->attrs, i);
		g_autofree gchar *key = xb_string_xml_escape (a->name);
		g_autofree gchar *val = xb_string_xml_escape (a->value);
		g_string_append_printf (helper->xml, " %s=\"%s\"", key, val);
	}

	/* finish the opening tag and add any text if it exists */
	if (priv->text != NULL) {
		g_autofree gchar *text = xb_string_xml_escape (priv->text);
		g_string_append (helper->xml, ">");
		g_string_append (helper->xml, text);
	} else {
		g_string_append (helper->xml, ">");
		if (helper->flags & XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE)
			g_string_append (helper->xml, "\n");
	}

	/* recurse deeper */
	for (guint i = 0; i < priv->children->len; i++) {
		XbBuilderNode *child = g_ptr_array_index (priv->children, i);
		helper->level++;
		if (!xb_builder_node_export_helper (child, helper, error))
			return FALSE;
		helper->level--;
	}

	/* add closing tag */
	if ((helper->flags & XB_NODE_EXPORT_FLAG_FORMAT_INDENT) > 0 &&
	    priv->text == NULL) {
		for (guint i = 0; i < helper->level; i++)
			g_string_append (helper->xml, "  ");
	}
	g_string_append_printf (helper->xml, "</%s>", priv->element);
	if (helper->flags & XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE)
		g_string_append (helper->xml, "\n");
	return TRUE;
}

/**
 * xb_builder_node_export:
 * @self: a #XbBuilderNode
 * @flags: some #XbNodeExportFlags, e.g. #XB_NODE_EXPORT_FLAG_NONE
 * @error: the #GError, or %NULL
 *
 * Exports the node to XML.
 *
 * Returns: XML data, or %NULL for an error
 *
 * Since: 0.1.5
 **/
gchar *
xb_builder_node_export (XbBuilderNode *self, XbNodeExportFlags flags, GError **error)
{
	g_autoptr(GString) xml = g_string_new (NULL);
	XbBuilderNodeExportHelper helper = {
		.flags		= flags,
		.level		= 0,
		.xml		= xml,
	};
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	if ((flags & XB_NODE_EXPORT_FLAG_ADD_HEADER) > 0)
		g_string_append (xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	if (!xb_builder_node_export_helper (self, &helper, error))
		return NULL;
	return g_string_free (g_steal_pointer (&xml), FALSE);
}
