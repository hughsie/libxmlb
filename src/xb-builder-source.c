/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <gio/gio.h>
#include <string.h>

#include "xb-builder-source-private.h"

typedef struct {
	GObject			 parent_instance;
	GInputStream		*istream;
	GFile			*file;
	GPtrArray		*node_items;	/* of XbBuilderSourceNodeFuncItem */
	GPtrArray		*converters;	/* of XbBuilderSourceConverterItem */
	XbBuilderNode		*info;
	gchar			*guid;
	gchar			*prefix;
	XbBuilderSourceFlags	 flags;
} XbBuilderSourcePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XbBuilderSource, xb_builder_source, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (xb_builder_source_get_instance_private (o))

typedef struct {
	gchar				*id;
	XbBuilderSourceNodeFunc		 func;
	gpointer			 user_data;
	GDestroyNotify			 user_data_free;
} XbBuilderSourceNodeFuncItem;

typedef struct {
	gchar				*content_type;
	XbBuilderSourceConverterFunc	 func;
	gpointer			 user_data;
	GDestroyNotify			 user_data_free;
} XbBuilderSourceConverterItem;

static GInputStream *
xb_builder_source_convert (XbBuilderSource *self,
			   GFile *file,
			   const gchar *content_type,
			   GCancellable *cancellable,
			   GError **error)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);

	/* uncompressed XML */
	if (g_strcmp0 (content_type, "application/xml") == 0)
		return G_INPUT_STREAM (g_file_read (file, cancellable, error));

	/* gzip */
	if (g_strcmp0 (content_type, "application/gzip") == 0 ||
	    g_strcmp0 (content_type, "application/x-gzip") == 0) {
		g_autoptr(GConverter) conv = NULL;
		g_autoptr(GInputStream) istream = NULL;
		istream = G_INPUT_STREAM (g_file_read (file, cancellable, error));
		if (istream == NULL)
			return NULL;
		conv = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
		return g_converter_input_stream_new (istream, conv);
	}

	/* registered converter */
	for (guint i = 0; i < priv->converters->len; i++) {
		XbBuilderSourceConverterItem *item = g_ptr_array_index (priv->converters, i);
		if (g_strcmp0 (item->content_type, content_type) == 0) {
			return item->func (self, file,
					   item->user_data,
					   cancellable,
					   error);
		}
	}

	/* unsupported */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "cannot process file of type %s",
		     content_type);
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

	/* decompress if required */
	content_type = g_file_info_get_attribute_string (fileinfo, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	priv->istream = xb_builder_source_convert (self, file,
						   content_type,
						   cancellable,
						   error);
	if (priv->istream == NULL)
		return FALSE;

	/* success */
	priv->flags = flags;
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
	XbBuilderSourceNodeFuncItem *item;
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);

	g_return_if_fail (XB_IS_BUILDER_SOURCE (self));
	g_return_if_fail (id != NULL);
	g_return_if_fail (func != NULL);

	item = g_slice_new0 (XbBuilderSourceNodeFuncItem);
	item->id = g_strdup (id);
	item->func = func;
	item->user_data = user_data;
	item->user_data_free = user_data_free;
	g_ptr_array_add (priv->node_items, item);
}

/**
 * xb_builder_source_add_converter:
 * @self: a #XbBuilderSource
 * @content_type: a mimetype, e.g. `application/x-desktop`
 * @func: a callback
 * @user_data: user pointer to pass to @func, or %NULL
 * @user_data_free: a function which gets called to free @user_data, or %NULL
 *
 * Adds a function that can be used to convert files loaded with
 * xb_builder_source_load_xml().
 *
 * Since: 0.1.1
 **/
void
xb_builder_source_add_converter (XbBuilderSource *self,
				 const gchar *content_type,
				 XbBuilderSourceConverterFunc func,
				 gpointer user_data,
				 GDestroyNotify user_data_free)
{
	XbBuilderSourceConverterItem *item;
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);

	g_return_if_fail (XB_IS_BUILDER_SOURCE (self));
	g_return_if_fail (content_type != NULL);
	g_return_if_fail (func != NULL);

	item = g_slice_new0 (XbBuilderSourceConverterItem);
	item->content_type = g_strdup (content_type);
	item->func = func;
	item->user_data = user_data;
	item->user_data_free = user_data_free;
	g_ptr_array_add (priv->converters, item);
}

gboolean
xb_builder_source_funcs_node (XbBuilderSource *self, XbBuilderNode *bn, GError **error)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	for (guint i = 0; i < priv->node_items->len; i++) {
		XbBuilderSourceNodeFuncItem *item = g_ptr_array_index (priv->node_items, i);
		if (!item->func (self, bn, item->user_data, error))
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
	for (guint i = 0; i < priv->node_items->len; i++) {
		XbBuilderSourceNodeFuncItem *item = g_ptr_array_index (priv->node_items, i);
		g_string_append_printf (str, ":func-id=%s", item->id);
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

GInputStream *
xb_builder_source_get_istream (XbBuilderSource *self)
{
	XbBuilderSourcePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), NULL);
	return priv->istream;
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

static void
xb_builder_source_node_func_free (XbBuilderSourceNodeFuncItem *item)
{
	if (item->user_data_free != NULL)
		item->user_data_free (item->user_data);
	g_free (item->id);
	g_slice_free (XbBuilderSourceNodeFuncItem, item);
}

static void
xb_builder_source_converter_free (XbBuilderSourceConverterItem *item)
{
	if (item->user_data_free != NULL)
		item->user_data_free (item->user_data);
	g_free (item->content_type);
	g_slice_free (XbBuilderSourceConverterItem, item);
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
	g_ptr_array_unref (priv->node_items);
	g_ptr_array_unref (priv->converters);
	g_free (priv->guid);
	g_free (priv->prefix);

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
	priv->node_items = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_builder_source_node_func_free);
	priv->converters = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_builder_source_converter_free);
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
