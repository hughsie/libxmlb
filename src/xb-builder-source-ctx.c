/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <gio/gio.h>

#include "xb-builder-source-ctx-private.h"
#include "xb-common-private.h"

typedef struct {
	GFile			*file;
	GInputStream		*istream;
	gchar			*basename;
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

static GBytes *
_g_input_stream_read_bytes_in_chunks (GInputStream *stream,
				      gsize count,
				      gsize chunk_sz,
				      GCancellable *cancellable,
				      GError **error)
{
	g_autofree guint8 *tmp = NULL;
	g_autoptr(GByteArray) buf = g_byte_array_new ();

	g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);
	g_return_val_if_fail (count > 0, NULL);
	g_return_val_if_fail (chunk_sz > 0, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* read from stream in chunks */
	tmp = g_malloc (chunk_sz);
	while (TRUE) {
		gssize sz;
		sz = g_input_stream_read (stream, tmp, chunk_sz, NULL, error);
		if (sz == 0)
			break;
		if (sz < 0)
			return NULL;
		g_byte_array_append (buf, tmp, sz);
		if (buf->len > count) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "cannot read from fd: 0x%x > 0x%x",
				     buf->len, (guint) count);
			return NULL;
		}
	}
	return g_byte_array_free_to_bytes (g_steal_pointer (&buf));
}

/**
 * xb_builder_source_ctx_get_bytes:
 * @self: a #XbBuilderSourceCtx
 * @cancellable: a #GCancellable, or %NULL
 * @error: the #GError, or %NULL
 *
 * Returns the data currently being processed.
 *
 * If the #XbBuilderSourceCtx is backed by a file, the returned #GBytes may be
 * memory-mapped, and the backing file must not be modified until the #GBytes is
 * destroyed.
 *
 * Returns: (transfer full): a #GBytes
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

	/* Try mmap()ing the file first, as that avoids buffer allocation.
	 * Note that this imposes the restriction that the backing file must not
	 * be modified during the lifetime of the returned #GBytes. */
	if (priv->file != NULL) {
		g_autoptr(GMappedFile) mapped_file = NULL;
		g_autofree gchar *filename = g_file_get_path (priv->file);

		mapped_file = g_mapped_file_new (filename, FALSE, NULL);
		if (mapped_file != NULL)
			return g_mapped_file_get_bytes (mapped_file);
	}

	return _g_input_stream_read_bytes_in_chunks (priv->istream,
						     128 * 1024 * 1024, /* 128Mb */
						     32 * 1024, /* 32Kb */
						     cancellable, error);
}

/**
 * xb_builder_source_ctx_get_filename:
 * @self: a #XbBuilderSourceCtx
 *
 * Returns the basename of the file currently being processed.
 *
 * Returns: (transfer none) (nullable): a basename, or %NULL if unset
 *
 * Since: 0.1.7
 **/
const gchar *
xb_builder_source_ctx_get_filename (XbBuilderSourceCtx *self)
{
	XbBuilderSourceCtxPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE_CTX (self), NULL);
	return priv->basename;
}

/**
 * xb_builder_source_ctx_get_content_type:
 * @self: a #XbBuilderSourceCtx
 * @cancellable: a #GCancellable, or %NULL
 * @error: the #GError, or %NULL
 *
 * Returns the content type of the input stream currently being
 * processed.
 *
 * Returns: (transfer full): a content type (e.g. `application/x-desktop`), or %NULL on error
 *
 * Since: 0.1.7
 **/
gchar *
xb_builder_source_ctx_get_content_type (XbBuilderSourceCtx *self,
					GCancellable *cancellable,
					GError **error)
{
	XbBuilderSourceCtxPrivate *priv = GET_PRIVATE (self);
	gsize bufsz = 0;
	guchar buf[4096] = { 0x00 };

	g_return_val_if_fail (XB_IS_BUILDER_SOURCE_CTX (self), NULL);

	if (G_IS_SEEKABLE (priv->istream)) {
		if (!g_input_stream_read_all (priv->istream, buf, sizeof(buf),
					      &bufsz, cancellable, error))
			return NULL;
		if (!g_seekable_seek (G_SEEKABLE (priv->istream), 0, G_SEEK_SET,
				      cancellable, error))
			return NULL;
	}
	if (bufsz > 0)
		return xb_content_type_guess (priv->basename, buf, bufsz);
	return xb_content_type_guess (priv->basename, NULL, 0);
}

/* private */
void
xb_builder_source_ctx_set_filename (XbBuilderSourceCtx *self, const gchar *basename)
{
	XbBuilderSourceCtxPrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER_SOURCE_CTX (self));
	g_return_if_fail (basename != NULL);
	g_free (priv->basename);
	priv->basename = g_strdup (basename);
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
	g_free (priv->basename);
	g_object_unref (priv->istream);
	g_clear_object (&priv->file);
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
 * @file: (transfer none) (nullable): Path to the file which contains the source,
 *    or %NULL if the source is not directly loadable from disk
 * @istream: (transfer none): Input stream to load the source from
 *
 * Creates a new builder source_ctx.
 *
 * Returns: (transfer full): a new #XbBuilderSourceCtx
 *
 * Since: 0.1.7
 **/
XbBuilderSourceCtx *
xb_builder_source_ctx_new (GFile *file, GInputStream *istream)
{
	XbBuilderSourceCtx *self = g_object_new (XB_TYPE_BUILDER_SOURCE_CTX, NULL);
	XbBuilderSourceCtxPrivate *priv = GET_PRIVATE (self);

	g_return_val_if_fail (file == NULL || G_IS_FILE (file), NULL);
	g_return_val_if_fail (G_IS_INPUT_STREAM (istream), NULL);

	priv->file = (file != NULL) ? g_object_ref (file) : NULL;
	priv->istream = g_object_ref (istream);
	return self;
}
