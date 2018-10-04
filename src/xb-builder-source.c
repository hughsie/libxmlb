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

struct _XbBuilderSource {
	GObject			 parent_instance;
	GInputStream		*istream;
	GPtrArray		*node_items;	/* of XbBuilderSourceNodeFuncItem */
	XbBuilderNode		*info;
	gchar			*guid;
	gchar			*prefix;
	XbBuilderSourceFlags	 flags;
};

G_DEFINE_TYPE (XbBuilderSource, xb_builder_source, G_TYPE_OBJECT)

typedef struct {
	XbBuilderSourceNodeFunc		 func;
	gpointer			 user_data;
	GDestroyNotify			 user_data_free;
} XbBuilderSourceNodeFuncItem;

/**
 * xb_builder_source_new_file:
 * @file: a #GFile
 * @flags: some #XbBuilderSourceFlags, e.g. %XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT
 * @cancellable: a #GCancellable, or %NULL
 * @error: the #GError, or %NULL
 *
 * Adds an optionally compressed XML file to build a #XbSilo.
 *
 * Returns: (transfer full): a #XbBuilderSource, or NULL for error.
 *
 * Since: 0.1.0
 **/
XbBuilderSource *
xb_builder_source_new_file (GFile *file,
			    XbBuilderSourceFlags flags,
			    GCancellable *cancellable,
			    GError **error)
{
	const gchar *content_type = NULL;
	guint64 mtime;
	g_autofree gchar *fn = NULL;
	g_autoptr(GConverter) conv = NULL;
	g_autoptr(GFileInfo) fileinfo = NULL;
	g_autoptr(GInputStream) istream = NULL;
	g_autoptr(XbBuilderSource) self = g_object_new (XB_TYPE_BUILDER_SOURCE, NULL);

	/* create input stream */
	istream = G_INPUT_STREAM (g_file_read (file, cancellable, error));
	if (istream == NULL)
		return NULL;

	/* what kind of file is this */
	fileinfo = g_file_query_info (file,
				      G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				      G_FILE_ATTRIBUTE_TIME_MODIFIED,
				      G_FILE_QUERY_INFO_NONE,
				      cancellable,
				      error);
	if (fileinfo == NULL)
		return NULL;

	/* add data to GUID */
	fn = g_file_get_path (file);
	mtime = g_file_info_get_attribute_uint64 (fileinfo, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	self->guid = g_strdup_printf ("%s:%" G_GUINT64_FORMAT, fn, mtime);

	/* decompress if required */
	content_type = g_file_info_get_attribute_string (fileinfo, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	if (g_strcmp0 (content_type, "application/gzip") == 0 ||
	    g_strcmp0 (content_type, "application/x-gzip") == 0) {
		conv = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
		self->istream = g_converter_input_stream_new (istream, conv);
	} else if (g_strcmp0 (content_type, "application/xml") == 0) {
		self->istream = g_object_ref (istream);
	} else {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "cannot process file of type %s",
			     content_type);
		return NULL;
	}

	/* success */
	self->flags = flags;
	return g_steal_pointer (&self);
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
	g_return_if_fail (XB_IS_BUILDER_SOURCE (self));
	g_set_object (&self->info, info);
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
	g_return_if_fail (XB_IS_BUILDER_SOURCE (self));
	g_free (self->prefix);
	self->prefix = g_strdup (prefix);
}

/**
 * xb_builder_source_new_xml:
 * @xml: XML data
 * @flags: some #XbBuilderSourceFlags, e.g. %XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT
 * @error: the #GError, or %NULL
 *
 * Parses XML data and begins to build a #XbSilo.
 *
 * Returns: (transfer full): a #XbBuilderSource, or NULL for error.
 *
 * Since: 0.1.0
 **/
XbBuilderSource *
xb_builder_source_new_xml (const gchar *xml, XbBuilderSourceFlags flags, GError **error)
{
	g_autoptr(XbBuilderSource) self = g_object_new (XB_TYPE_BUILDER_SOURCE, NULL);
	g_autoptr(GBytes) blob = NULL;
	g_autoptr(GChecksum) csum = g_checksum_new (G_CHECKSUM_SHA1);

	/* add a GUID of the SHA1 hash of the entire string */
	g_checksum_update (csum, (const guchar *) xml, -1);
	self->guid = g_strdup (g_checksum_get_string (csum));

	/* create input stream */
	blob = g_bytes_new (xml, strlen (xml));
	self->istream = g_memory_input_stream_new_from_bytes (blob);
	if (self->istream == NULL)
		return NULL;

	/* success */
	self->flags = flags;
	return g_steal_pointer (&self);
}

/**
 * xb_builder_source_add_node_func:
 * @self: a #XbBuilderSource
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
				 XbBuilderSourceNodeFunc func,
				 gpointer user_data,
				 GDestroyNotify user_data_free)
{
	XbBuilderSourceNodeFuncItem *item;

	g_return_if_fail (XB_IS_BUILDER_SOURCE (self));
	g_return_if_fail (func != NULL);

	item = g_slice_new0 (XbBuilderSourceNodeFuncItem);
	item->func = func;
	item->user_data = user_data;
	item->user_data_free = user_data_free;
	g_ptr_array_add (self->node_items, item);
}

gboolean
xb_builder_source_funcs_node (XbBuilderSource *self, XbBuilderNode *bn, GError **error)
{
	for (guint i = 0; i < self->node_items->len; i++) {
		XbBuilderSourceNodeFuncItem *item = g_ptr_array_index (self->node_items, i);
		if (!item->func (self, bn, item->user_data, error))
			return FALSE;
	}
	return TRUE;
}

const gchar *
xb_builder_source_get_guid (XbBuilderSource *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), NULL);
	return self->guid;
}

const gchar *
xb_builder_source_get_prefix (XbBuilderSource *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), NULL);
	return self->prefix;
}

XbBuilderNode *
xb_builder_source_get_info (XbBuilderSource *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), NULL);
	return self->info;
}

GInputStream *
xb_builder_source_get_istream (XbBuilderSource *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), NULL);
	return self->istream;
}

XbBuilderSourceFlags
xb_builder_source_get_flags (XbBuilderSource *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_SOURCE (self), 0);
	return self->flags;
}

static void
xb_builder_import_node_func_free (XbBuilderSourceNodeFuncItem *item)
{
	if (item->user_data_free != NULL)
		item->user_data_free (item->user_data);
	g_slice_free (XbBuilderSourceNodeFuncItem, item);
}

static void
xb_builder_source_finalize (GObject *obj)
{
	XbBuilderSource *self = XB_BUILDER_SOURCE (obj);

	if (self->istream != NULL)
		g_object_unref (self->istream);
	if (self->info != NULL)
		g_object_unref (self->info);
	g_ptr_array_unref (self->node_items);
	g_free (self->guid);
	g_free (self->prefix);

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
	self->node_items = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_builder_import_node_func_free);
}
