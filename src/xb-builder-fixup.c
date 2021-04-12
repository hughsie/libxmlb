/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <gio/gio.h>

#include "xb-builder-fixup-private.h"

typedef struct {
	gchar			*id;
	XbBuilderFixupFunc	 func;
	gpointer		 user_data;
	GDestroyNotify		 user_data_free;
	gint			 max_depth;
} XbBuilderFixupPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XbBuilderFixup, xb_builder_fixup, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (xb_builder_fixup_get_instance_private (o))

typedef struct {
	XbBuilderFixup	*self;
	gboolean	 ret;
	GError		*error;
} XbBuilderFixupHelper;

static gboolean
xb_builder_fixup_cb (XbBuilderNode *bn, gpointer data)
{
	XbBuilderFixupHelper *helper = (XbBuilderFixupHelper *) data;
	XbBuilderFixup *self = XB_BUILDER_FIXUP (helper->self);
	XbBuilderFixupPrivate *priv = GET_PRIVATE (self);

	/* run all node funcs on the source */
	if (!priv->func (self, bn, priv->user_data, &helper->error)) {
		helper->ret = FALSE;
		return TRUE;
	}

	/* keep going */
	return FALSE;
}

/* private */
gboolean
xb_builder_fixup_node (XbBuilderFixup *self, XbBuilderNode *bn, GError **error)
{
	XbBuilderFixupPrivate *priv = GET_PRIVATE (self);
	XbBuilderFixupHelper helper = {
		.self = self,
		.ret = TRUE,
		.error = NULL,
	};

	/* visit each node */
	xb_builder_node_traverse (bn, G_PRE_ORDER, G_TRAVERSE_ALL, priv->max_depth,
				  xb_builder_fixup_cb, &helper);
	if (!helper.ret) {
		g_propagate_error (error, helper.error);
		return FALSE;
	}
	return TRUE;
}

/**
 * xb_builder_fixup_get_id:
 * @self: a #XbBuilderFixup
 *
 * Gets the fixup ID.
 *
 * Returns: string, e.g. `AppStreamUpgrade`
 *
 * Since: 0.1.3
 **/
const gchar *
xb_builder_fixup_get_id (XbBuilderFixup *self)
{
	XbBuilderFixupPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_FIXUP (self), NULL);
	return priv->id;
}

/* private */
gchar *
xb_builder_fixup_get_guid (XbBuilderFixup *self)
{
	g_autoptr(GString) str = g_string_new ("func-id=");
	XbBuilderFixupPrivate *priv = GET_PRIVATE (self);

	g_return_val_if_fail (XB_IS_BUILDER_FIXUP (self), NULL);

	/* build GUID using ID and max-depth, if set */
	g_string_append (str, priv->id);
	if (priv->max_depth != -1)
		g_string_append_printf (str, "@%i", priv->max_depth);
	return g_string_free (g_steal_pointer (&str), FALSE);
}

/**
 * xb_builder_fixup_get_max_depth:
 * @self: a #XbBuilderFixup
 *
 * Gets the maximum depth used for this fixup, if each node is being visited.
 *
 * Returns: integer, or -1 if unset
 *
 * Since: 0.1.3
 **/
gint
xb_builder_fixup_get_max_depth (XbBuilderFixup *self)
{
	XbBuilderFixupPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_FIXUP (self), 0);
	return priv->max_depth;
}

/**
 * xb_builder_fixup_set_max_depth:
 * @self: a #XbBuilderFixup
 * @max_depth: integer, -1 for "all"
 *
 * Sets the maximum depth used for this fixup. Use a @max_depth of 0 to only
 * visit the root node.
 *
 * Setting a maximum depth may increase performance considerably if using
 * fixup functions on large and deeply nested XML files.
 *
 * Since: 0.1.3
 **/
void
xb_builder_fixup_set_max_depth (XbBuilderFixup *self, gint max_depth)
{
	XbBuilderFixupPrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER_FIXUP (self));
	priv->max_depth = max_depth;
}

static void
xb_builder_fixup_finalize (GObject *obj)
{
	XbBuilderFixup *self = XB_BUILDER_FIXUP (obj);
	XbBuilderFixupPrivate *priv = GET_PRIVATE (self);

	if (priv->user_data_free != NULL)
		priv->user_data_free (priv->user_data);
	g_free (priv->id);

	G_OBJECT_CLASS (xb_builder_fixup_parent_class)->finalize (obj);
}

static void
xb_builder_fixup_class_init (XbBuilderFixupClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = xb_builder_fixup_finalize;
}

static void
xb_builder_fixup_init (XbBuilderFixup *self)
{
	XbBuilderFixupPrivate *priv = GET_PRIVATE (self);
	priv->max_depth = -1;
}

/**
 * xb_builder_fixup_new:
 * @id: a text ID value, e.g. `AppStreamUpgrade`
 * @func: a callback
 * @user_data: user pointer to pass to @func, or %NULL
 * @user_data_free: a function which gets called to free @user_data, or %NULL
 *
 * Creates a function that will get run on every #XbBuilderNode compile creates.
 *
 * Returns: a new #XbBuilderFixup
 *
 * Since: 0.1.3
 **/
XbBuilderFixup *
xb_builder_fixup_new (const gchar *id,
		      XbBuilderFixupFunc func,
		      gpointer user_data,
		      GDestroyNotify user_data_free)
{
	XbBuilderFixup *self = g_object_new (XB_TYPE_BUILDER_FIXUP, NULL);
	XbBuilderFixupPrivate *priv = GET_PRIVATE (self);

	g_return_val_if_fail (XB_IS_BUILDER_FIXUP (self), NULL);
	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (func != NULL, NULL);

	priv->id = g_strdup (id);
	priv->func = func;
	priv->user_data = user_data;
	priv->user_data_free = user_data_free;
	return self;
}
