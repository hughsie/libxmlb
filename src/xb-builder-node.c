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

struct _XbBuilderNode
{
	GObject			 parent_instance;
	guint32			 offset;
	XbBuilderNodeFlags	 flags;
	gchar			*element;
	guint32			 element_idx;
	gchar			*text;
	guint32			 text_idx;
	GPtrArray		*children;	/* of XbBuilderNode */
	GPtrArray		*attrs;		/* of XbBuilderNodeAttr */
};

G_DEFINE_TYPE (XbBuilderNode, xb_builder_node, G_TYPE_OBJECT)

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
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), FALSE);
	return (self->flags & flag) > 0;
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
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	self->flags |= flag;
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
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), NULL);
	return self->element;
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
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), NULL);
	return self->text;
}

/* private */
GPtrArray *
xb_builder_get_attrs (XbBuilderNode *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), NULL);
	return self->attrs;
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
	guint newline_count = 0;
	g_auto(GStrv) split = NULL;

	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	g_return_if_fail (text != NULL);

	/* old data */
	g_free (self->text);

	/* we know this has been pre-fixed */
	if (xb_builder_node_has_flag (self, XB_BUILDER_NODE_FLAG_LITERAL_TEXT)) {
		self->text = g_strndup (text, text_len);
		return;
	}

	/* all on one line, no trailing or leading whitespace */
	if (g_strstr_len (text, text_len, "\n") == NULL &&
	    !g_str_has_prefix (text, " ") &&
	    !g_str_has_suffix (text, " ")) {
		gsize len;
		len = text_len >= 0 ? (gsize) text_len : strlen (text);
		self->text = g_strndup (text, len);
		return;
	}

	/* split the text into lines */
	tmp = g_string_sized_new ((gsize) text_len + 1);
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
	self->text = g_string_free (tmp, FALSE);
}

/**
 * xb_builder_node_add_attribute:
 * @self: a #XbBuilderNode
 * @name: attribute name, e.g. `type`
 * @value: attribute value, e.g. `desktop`
 *
 * Adds an attribute to the builder node.
 *
 * Since: 0.1.0
 **/
void
xb_builder_node_add_attribute (XbBuilderNode *self, const gchar *name, const gchar *value)
{
	XbBuilderNodeAttr *a = g_slice_new0 (XbBuilderNodeAttr);
	a->name = g_strdup (name);
	a->name_idx = XB_SILO_UNSET;
	a->value = g_strdup (value);
	a->value_idx = XB_SILO_UNSET;
	g_ptr_array_add (self->attrs, a);
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
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	g_return_if_fail (XB_IS_BUILDER_NODE (child));
	g_ptr_array_add (self->children, g_object_ref (child));
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
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), NULL);
	return self->children;
}

/* private */
guint32
xb_builder_node_get_offset (XbBuilderNode *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), 0);
	return self->offset;
}

/* private */
void
xb_builder_node_set_offset (XbBuilderNode *self, guint32 offset)
{
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	self->offset = offset;
}

/* private */
guint32
xb_builder_node_get_element_idx (XbBuilderNode *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), 0);
	return self->element_idx;
}

/* private */
void
xb_builder_node_set_element_idx (XbBuilderNode *self, guint32 element_idx)
{
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	self->element_idx = element_idx;
}

/* private */
guint32
xb_builder_node_get_text_idx (XbBuilderNode *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_NODE (self), 0);
	return self->text_idx;
}

/* private */
void
xb_builder_node_set_text_idx (XbBuilderNode *self, guint32 text_idx)
{
	g_return_if_fail (XB_IS_BUILDER_NODE (self));
	self->text_idx = text_idx;
}

/* private */
guint32
xb_builder_node_size (XbBuilderNode *self)
{
	guint32 sz = sizeof(XbSiloNode);
	return sz + self->attrs->len * sizeof(XbSiloAttr);
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
	self->element_idx = XB_SILO_UNSET;
	self->text_idx = XB_SILO_UNSET;
	self->attrs = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_builder_node_attr_free);
	self->children = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

static void
xb_builder_node_finalize (GObject *obj)
{
	XbBuilderNode *self = XB_BUILDER_NODE (obj);
	g_free (self->element);
	g_free (self->text);
	g_ptr_array_unref (self->attrs);
	g_ptr_array_unref (self->children);
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
	self->element = g_strdup (element);
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
		xb_builder_node_add_attribute (self, key, value);
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
		xb_builder_node_add_attribute (self, key, value);
	}
	va_end (args);
}
