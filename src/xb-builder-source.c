/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <gio/gio.h>
#include <string.h>

#include "xb-builder-fixup-private.h"
#include "xb-builder-source-ctx-private.h"
#include "xb-builder-source-private.h"

typedef struct {
	GObject			 parent_instance;
	GInputStream		*istream;
	GFile			*file;
	GPtrArray		*fixups;	/* of XbBuilderFixup */
	GPtrArray		*adapters;	/* of XbBuilderSourceAdapter */
	XbBuilderNode		*info;
	gchar			*guid;
	gchar			*prefix;
	gchar			*content_type;
	XbBuilderSourceFlags	 flags;
} XbBuilderSourcePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XbBuilderSource, xb_builder_source, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (xb_builder_source_get_instance_private (o))

typedef struct {
	gchar				*content_type;
	XbBuilderSourceConverterFunc	 func_file;	/* deprecated */
	XbBuilderSourceAdapterFunc	 func_adapter;
	gpointer			 user_data;
	GDestroyNotify			 user_data_free;
} XbBuilderSourceAdapter;

static XbBuilderSourceAdapter *
xb_builder_source_get_converter_by_mime (XbBuilderSource *self,
					 const gchar *content_type)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	for (guint i = 0; i < priv->adapters->len; i++) {
		XbBuilderSourceAdapter *item = g_ptr_array_index (priv->adapters, i);
		if (item->func_file == NULL)
			continue;
		if (g_strcmp0 (item->content_type, content_type) == 0)
			return item;
	}
	return NULL;
}

static XbBuilderSourceAdapter *
xb_builder_source_get_adapter_by_mime (XbBuilderSource *self,
				       const gchar *content_type)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	for (guint i = 0; i < priv->adapters->len; i++) {
		XbBuilderSourceAdapter *item = g_ptr_array_index (priv->adapters, i);
		if (item->func_adapter == NULL)
			continue;
		if (g_strcmp0 (item->content_type, content_type) == 0)
			return item;
	}
	return NULL;
}

/**
 * xb_builder_source_load_file:
 * @self: a #XbBuilderSource
 * @file: a #GFile
 * @flags: some #XbBuilderSourceFlags, e.g. %XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT
 * @cancellable: a #GCancellable, or %NULL
 * @error: the #GError, or %NULL
 *
 * Loads an optionally compressed XML file to build a #XbSilo.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.1
 **/
gboolean
xb_builder_source_load_file (XbBuilderSource *self,
			     GFile *file,
			     XbBuilderSourceFlags flags,
			     GCancellable *cancellable,
			     GError **error)
{
	const gchar *content_type = NULL;
	guint32 ctime_usec;
	guint64 ctime;
	g_autofree gchar *fn = NULL;
	g_autoptr(GFileInfo) fileinfo = NULL;
	g_autoptr(GString) guid = NULL;
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);

	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* what kind of file is this */
	fileinfo = g_file_query_info (file,
				      G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				      G_FILE_ATTRIBUTE_TIME_CHANGED ","
				      G_FILE_ATTRIBUTE_TIME_CHANGED_USEC,
				      G_FILE_QUERY_INFO_NONE,
				      cancellable,
				      error);
	if (fileinfo == NULL)
		return FALSE;

	/* add data to GUID */
	fn = g_file_get_path (file);
	guid = g_string_new (fn);
	ctime = g_file_info_get_attribute_uint64 (fileinfo, G_FILE_ATTRIBUTE_TIME_CHANGED);
	if (ctime != 0)
		g_string_append_printf (guid, ":ctime=%" G_GUINT64_FORMAT, ctime);
	ctime_usec = g_file_info_get_attribute_uint32 (fileinfo, G_FILE_ATTRIBUTE_TIME_CHANGED_USEC);
	if (ctime_usec != 0)
		g_string_append_printf (guid, ".%" G_GUINT32_FORMAT, ctime_usec);
	priv->guid = g_string_free (g_steal_pointer (&guid), FALSE);

	/* check content type of file */
	content_type = g_file_info_get_attribute_string (fileinfo, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	if (content_type == NULL) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "cannot get content type for file");
		return FALSE;
	}

	/* success */
	priv->flags = flags;
	priv->content_type = g_strdup (content_type);
	priv->file = g_object_ref (file);
	return TRUE;
}

/**
 * xb_builder_source_set_info:
 * @self: a #XbBuilderSource
 * @info: (allow-none): a #XbBuilderNode
 *
 * Sets an optional information metadata node on the root node.
 *
 * Since: 0.1.0
 **/
void
xb_builder_source_set_info (XbBuilderSource *self, XbBuilderNode *info)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER_SOURCE (self));
	g_set_object (&priv->info, info);
}

/**
 * xb_builder_source_set_prefix:
 * @self: a #XbBuilderSource
 * @prefix: (allow-none): an XPath prefix, e.g. `installed`
 *
 * Sets an optional prefix on the root node. This makes any nodes added
 * using this source reside under a common shared parent node.
 *
 * Since: 0.1.0
 **/
void
xb_builder_source_set_prefix (XbBuilderSource *self, const gchar *prefix)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER_SOURCE (self));
	g_free (priv->prefix);
	priv->prefix = g_strdup (prefix);
}

/**
 * xb_builder_source_load_xml:
 * @self: a #XbBuilderSource
 * @xml: XML data
 * @flags: some #XbBuilderSourceFlags, e.g. %XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT
 * @error: the #GError, or %NULL
 *
 * Loads XML data and begins to build a #XbSilo.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.1
 **/
gboolean
xb_builder_source_load_xml (XbBuilderSource *self,
			    const gchar *xml,
			    XbBuilderSourceFlags flags,
			    GError **error)
{
	g_autoptr(GBytes) blob = NULL;
	g_autoptr(GChecksum) csum = g_checksum_new (G_CHECKSUM_SHA1);
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);

	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), FALSE);
	g_return_val_if_fail (xml != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* add a GUID of the SHA1 hash of the entire string */
	g_checksum_update (csum, (const guchar *) xml, -1);
	priv->guid = g_strdup (g_checksum_get_string (csum));

	/* create input stream */
	blob = g_bytes_new (xml, strlen (xml));
	priv->istream = g_memory_input_stream_new_from_bytes (blob);
	if (priv->istream == NULL)
		return FALSE;

	/* success */
	priv->flags = flags;
	return TRUE;
}

/**
 * xb_builder_source_load_bytes:
 * @self: a #XbBuilderSource
 * @bytes: a #GBytes
 * @flags: some #XbBuilderSourceFlags, e.g. %XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT
 * @error: the #GError, or %NULL
 *
 * Loads XML data and begins to build a #XbSilo.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.2
 **/
gboolean
xb_builder_source_load_bytes (XbBuilderSource *self,
			      GBytes *bytes,
			      XbBuilderSourceFlags flags,
			      GError **error)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	g_autoptr(GChecksum) csum = g_checksum_new (G_CHECKSUM_SHA1);

	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), FALSE);
	g_return_val_if_fail (bytes != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* add a GUID of the SHA1 hash of the entire blob */
	g_checksum_update (csum,
			   (const guchar *) g_bytes_get_data (bytes, NULL),
			   (gssize) g_bytes_get_size (bytes));
	priv->guid = g_strdup (g_checksum_get_string (csum));

	/* create input stream */
	priv->istream = g_memory_input_stream_new_from_bytes (bytes);
	if (priv->istream == NULL)
		return FALSE;

	/* success */
	priv->flags = flags;
	return TRUE;
}

/**
 * xb_builder_source_add_fixup:
 * @self: a #XbBuilderSource
 * @fixup: a #XbBuilderFixup
 *
 * Adds a function that will get run on every #XbBuilderNode compile creates
 * with this source.
 *
 * Since: 0.1.3
 **/
void
xb_builder_source_add_fixup (XbBuilderSource *self, XbBuilderFixup *fixup)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER_SOURCE (self));
	g_return_if_fail (XB_IS_BUILDER_FIXUP (fixup));
	g_ptr_array_add (priv->fixups, g_object_ref (fixup));
}

/**
 * xb_builder_source_add_node_func:
 * @self: a #XbBuilderSource
 * @id: a text ID value, e.g. `AppStreamUpgrade`
 * @func: a callback
 * @user_data: user pointer to pass to @func, or %NULL
 * @user_data_free: a function which gets called to free @user_data, or %NULL
 *
 * Adds a function that will get run on every #XbBuilderNode compile creates.
 *
 * Since: 0.1.0
 **/
void
xb_builder_source_add_node_func (XbBuilderSource *self,
				 const gchar *id,
				 XbBuilderSourceNodeFunc func,
				 gpointer user_data,
				 GDestroyNotify user_data_free)
{
	g_autoptr(XbBuilderFixup) fixup = NULL;
	/* close enough... */
	fixup = xb_builder_fixup_new (id, (XbBuilderFixupFunc) func,
				      user_data, user_data_free);
	xb_builder_source_add_fixup (self, fixup);
}

/**
 * xb_builder_source_add_converter:
 * @self: a #XbBuilderSource
 * @content_types: mimetypes, e.g. `application/x-desktop,application/gzip`
 * @func: a callback
 * @user_data: user pointer to pass to @func, or %NULL
 * @user_data_free: a function which gets called to free @user_data, or %NULL
 *
 * Adds a function that can be used to convert files loaded with
 * xb_builder_source_load_xml().
 *
 * This function is now deprecated, and xb_builder_source_add_adapter() can be
 * used instead. The latter allows chaining streams rather than just one #GFile
 * to #GInputStream conversion.
 *
 * Since: 0.1.1
 **/
void
xb_builder_source_add_converter (XbBuilderSource *self,
				 const gchar *content_types,
				 XbBuilderSourceConverterFunc func,
				 gpointer user_data,
				 GDestroyNotify user_data_free)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	g_auto(GStrv) split = NULL;

	g_return_if_fail (XB_IS_BUILDER_SOURCE (self));
	g_return_if_fail (content_types != NULL);

	/* add each */
	split = g_strsplit (content_types, ",", -1);
	for (guint i = 0; split[i] != NULL; i++) {
		XbBuilderSourceAdapter *item;
		item = g_slice_new0 (XbBuilderSourceAdapter);
		item->content_type = g_strdup (split[i]);
		item->func_file = func;
		item->user_data = user_data;
		item->user_data_free = user_data_free;
		g_ptr_array_add (priv->adapters, item);
	}
}

/**
 * xb_builder_source_add_adapter:
 * @self: a #XbBuilderSource
 * @content_types: mimetypes, e.g. `application/x-desktop,application/gzip`
 * @func: a callback, or %NULL
 * @user_data: user pointer to pass to @func, or %NULL
 * @user_data_free: a function which gets called to free @user_data, or %NULL
 *
 * Adds a function that can be used to convert streams loaded with
 * xb_builder_source_load_xml().
 *
 * Since: 0.1.7
 **/
void
xb_builder_source_add_adapter (XbBuilderSource *self,
			       const gchar *content_types,
			       XbBuilderSourceAdapterFunc func,
			       gpointer user_data,
			       GDestroyNotify user_data_free)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	g_auto(GStrv) split = NULL;

	g_return_if_fail (XB_IS_BUILDER_SOURCE (self));
	g_return_if_fail (content_types != NULL);
	g_return_if_fail (func != NULL);

	/* add each */
	split = g_strsplit (content_types, ",", -1);
	for (guint i = 0; split[i] != NULL; i++) {
		XbBuilderSourceAdapter *item;
		item = g_slice_new0 (XbBuilderSourceAdapter);
		item->content_type = g_strdup (split[i]);
		item->func_adapter = func;
		item->user_data = user_data;
		item->user_data_free = user_data_free;
		g_ptr_array_add (priv->adapters, item);
	}
}

gboolean
xb_builder_source_fixup (XbBuilderSource *self, XbBuilderNode *bn, GError **error)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	for (guint i = 0; i < priv->fixups->len; i++) {
		XbBuilderFixup *fixup = g_ptr_array_index (priv->fixups, i);
		if (!xb_builder_fixup_node (fixup, bn, error))
			return FALSE;
	}
	return TRUE;
}

static gboolean
xb_builder_source_info_guid_cb (XbBuilderNode *bn, gpointer data)
{
	GString *str = (GString *) data;
	if (xb_builder_node_get_text (bn) != NULL) {
		g_string_append_printf (str, ":%s=%s",
					xb_builder_node_get_element (bn),
					xb_builder_node_get_text (bn));
	}
	return FALSE;
}

gchar *
xb_builder_source_get_guid (XbBuilderSource *self)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	GString *str = g_string_new (priv->guid);

	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), NULL);

	/* append function IDs */
	for (guint i = 0; i < priv->fixups->len; i++) {
		XbBuilderFixup *fixup = g_ptr_array_index (priv->fixups, i);
		g_autofree gchar *tmp = xb_builder_fixup_get_guid (fixup);
		g_string_append_printf (str, ":%s", tmp);
	}

	/* append any info */
	if (priv->info != NULL) {
		xb_builder_node_traverse (priv->info, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
					  xb_builder_source_info_guid_cb, str);
	}

	/* append prefix */
	if (priv->prefix != NULL)
		g_string_append_printf (str, ":prefix=%s", priv->prefix);
	return g_string_free (str, FALSE);
}

const gchar *
xb_builder_source_get_prefix (XbBuilderSource *self)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), NULL);
	return priv->prefix;
}

XbBuilderNode *
xb_builder_source_get_info (XbBuilderSource *self)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), NULL);
	return priv->info;
}

/* converts foo.xml.gz to foo.xml */
static void
xb_builder_source_remove_last_extension (gchar *basename)
{
	gchar *tmp = g_strrstr (basename, ".");
	if (tmp != NULL)
		*tmp = '\0';
}

GInputStream *
xb_builder_source_get_istream (XbBuilderSource *self,
			       GCancellable *cancellable,
			       GError **error)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	XbBuilderSourceAdapter *item;
	g_autofree gchar *basename = NULL;

	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), NULL);

	/* nothing required */
	if (priv->istream != NULL)
		return g_object_ref (priv->istream);

	/* decompress if required using the deprecated vfunc */
	item = xb_builder_source_get_converter_by_mime (self, priv->content_type);
	if (item != NULL && item->func_file != NULL) {
		return item->func_file (self, priv->file, item->user_data,
					cancellable, error);
	}

	/* convert the file to a GFileInputStream */
	priv->istream = G_INPUT_STREAM (g_file_read (priv->file, cancellable, error));
	if (priv->istream == NULL)
		return NULL;

	/* run the content type handlers until we get application/xml */
	basename = g_file_get_basename (priv->file);
	do {
		g_autofree gchar *content_type = NULL;
		g_autoptr(GInputStream) istream_tmp = NULL;
		g_autoptr(XbBuilderSourceCtx) ctx = xb_builder_source_ctx_new (priv->istream);

		/* get the content type of the stream */
		xb_builder_source_ctx_set_filename (ctx, basename);
		content_type = xb_builder_source_ctx_get_content_type (ctx,
								       cancellable,
								       error);
		if (content_type == NULL)
			return NULL;
		if (g_strcmp0 (content_type, "application/xml") == 0)
			break;

		/* convert the stream */
		item = xb_builder_source_get_adapter_by_mime (self, content_type);
		if (item == NULL || item->func_adapter == NULL) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "cannot process content type %s",
				     content_type);
			return NULL;
		}
		istream_tmp = item->func_adapter (self, ctx, item->user_data,
						  cancellable, error);
		if (istream_tmp == NULL)
			return NULL;
		xb_builder_source_remove_last_extension (basename);
		g_set_object (&priv->istream, istream_tmp);
	} while (TRUE);
	return g_object_ref (priv->istream);
}

GFile *
xb_builder_source_get_file (XbBuilderSource *self)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), NULL);
	return priv->file;
}

XbBuilderSourceFlags
xb_builder_source_get_flags (XbBuilderSource *self)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), 0);
	return priv->flags;
}

static GInputStream *
xb_builder_source_load_gzip_cb (XbBuilderSource *self,
				XbBuilderSourceCtx *ctx,
				gpointer user_data,
				GCancellable *cancellable,
				GError **error)
{
	GInputStream *istream = xb_builder_source_ctx_get_stream (ctx);
	g_autoptr(GConverter) conv = NULL;
	conv = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
	return g_converter_input_stream_new (istream, conv);
}

static void
xb_builder_source_adapter_free (XbBuilderSourceAdapter *item)
{
	if (item->user_data_free != NULL)
		item->user_data_free (item->user_data);
	g_free (item->content_type);
	g_slice_free (XbBuilderSourceAdapter, item);
}

static void
xb_builder_source_finalize (GObject *obj)
{
	XbBuilderSource *self = XB_BUILDER_SOURCE (obj);
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);

	if (priv->istream != NULL)
		g_object_unref (priv->istream);
	if (priv->info != NULL)
		g_object_unref (priv->info);
	if (priv->file != NULL)
		g_object_unref (priv->file);
	g_ptr_array_unref (priv->fixups);
	g_ptr_array_unref (priv->adapters);
	g_free (priv->guid);
	g_free (priv->prefix);
	g_free (priv->content_type);

	G_OBJECT_CLASS (xb_builder_source_parent_class)->finalize (obj);
}

static void
xb_builder_source_class_init (XbBuilderSourceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = xb_builder_source_finalize;
}

static void
xb_builder_source_init (XbBuilderSource *self)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	priv->fixups = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->adapters = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_builder_source_adapter_free);
	xb_builder_source_add_adapter (self, "application/gzip,application/x-gzip",
				       xb_builder_source_load_gzip_cb, NULL, NULL);
}

/**
 * xb_builder_source_new:
 *
 * Creates a new builder source.
 *
 * Returns: a new #XbBuilderSource
 *
 * Since: 0.1.1
 **/
XbBuilderSource *
xb_builder_source_new (void)
{
	return g_object_new (XB_TYPE_BUILDER_SOURCE, NULL);
}
