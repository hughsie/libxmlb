/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <gio/gio.h>

#include "xb-builder-source-ctx-private.h"

typedef struct {
	GObject			 parent_instance;
	GInputStream		*istream;
	gchar			*filename;
	gchar			*content_type;
} XbBuilderSourceCtxPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XbBuilderSourceCtx, xb_builder_source_ctx, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (xb_builder_source_ctx_get_instance_private (o))

/**
 * xb_builder_source_ctx_get_stream:
 * @self: a #XbBuilderSourceCtx
 *
 * Returns the input stream currently being processed.
 *
 * Returns: (transfer none): a #GInputStream
 *
 * Since: 0.1.7
 **/
GInputStream *
xb_builder_source_ctx_get_stream (XbBuilderSourceCtx *self)
{
	XbBuilderSourceCtxPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE_CTX (self), NULL);
	return priv->istream;
}

/**
 * xb_builder_source_ctx_get_bytes:
 * @self: a #XbBuilderSourceCtx
 * @cancellable: a #GCancellable, or %NULL
 * @error: the #GError, or %NULL
 *
 * Returns the data currently being processed.
 *
 * Returns: (transfer none): a #GInputStream
 *
 * Since: 0.1.7
 **/
GBytes *
xb_builder_source_ctx_get_bytes (XbBuilderSourceCtx *self,
				 GCancellable *cancellable,
				 GError **error)
{
	XbBuilderSourceCtxPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE_CTX (self), NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	return g_input_stream_read_bytes (priv->istream,
					  128 * 1024 * 1024, /* 128Mb */
					  cancellable, error);
}

/**
 * xb_builder_source_ctx_get_filename:
 * @self: a #XbBuilderSourceCtx
 *
 * Returns the basename of the file currently being processed.
 *
 * Returns: a filename, or %NULL if unset
 *
 * Since: 0.1.7
 **/
const gchar *
xb_builder_source_ctx_get_filename (XbBuilderSourceCtx *self)
{
	XbBuilderSourceCtxPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE_CTX (self), NULL);
	return priv->filename;
}

/**
 * xb_builder_source_ctx_get_content_type:
 * @self: a #XbBuilderSourceCtx
 *
 * Returns the content type of the input stream currently being
 * processed.
 *
 * Returns: (transfer full): a content type (e.g. `application/x-desktop`), or %NULL
 *
 * Since: 0.1.7
 **/
gchar *
xb_builder_source_ctx_get_content_type (XbBuilderSourceCtx *self,
					GCancellable *cancellable,
					GError **error)
{
	XbBuilderSourceCtxPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE_CTX (self), NULL);
	if (G_IS_SEEKABLE (priv->istream)) {
		gsize bufsz = 0;
		guchar buf[4096] = { 0x00 };
		if (!g_input_stream_read_all (priv->istream, buf, sizeof(buf),
					      &bufsz, cancellable, error))
			return NULL;
		if (!g_seekable_seek (G_SEEKABLE (priv->istream), 0, G_SEEK_SET,
				      cancellable, error))
			return NULL;
		if (bufsz > 0)
			return g_content_type_guess (priv->filename, buf, bufsz, NULL);
	}
	return g_content_type_guess (priv->filename, NULL, 0, NULL);
}

/* private */
void
xb_builder_source_ctx_set_filename (XbBuilderSourceCtx *self, const gchar *filename)
{
	XbBuilderSourceCtxPrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER_SOURCE_CTX (self));
	g_return_if_fail (filename != NULL);
	g_free (priv->filename);
	priv->filename = g_strdup (filename);
}

static void
xb_builder_source_ctx_init (XbBuilderSourceCtx *self)
{
}

static void
xb_builder_source_ctx_finalize (GObject *obj)
{
	XbBuilderSourceCtx *self = XB_BUILDER_SOURCE_CTX (obj);
	XbBuilderSourceCtxPrivate *priv = GET_PRIVATE (self);
	g_free (priv->filename);
	g_object_unref (priv->istream);
	G_OBJECT_CLASS (xb_builder_source_ctx_parent_class)->finalize (obj);
}

static void
xb_builder_source_ctx_class_init (XbBuilderSourceCtxClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = xb_builder_source_ctx_finalize;
}

/**
 * xb_builder_source_ctx_new:
 * @element: An element name, e.g. "component"
 *
 * Creates a new builder source_ctx.
 *
 * Returns: (transfer full): a new #XbBuilderSourceCtx
 *
 * Since: 0.1.7
 **/
XbBuilderSourceCtx *
xb_builder_source_ctx_new (GInputStream *istream)
{
	XbBuilderSourceCtx *self = g_object_new (XB_TYPE_BUILDER_SOURCE_CTX, NULL);
	XbBuilderSourceCtxPrivate *priv = GET_PRIVATE (self);
	priv->istream = g_object_ref (istream);
	return self;
}
