/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <gio/gio.h>
#include <string.h>

#include "xb-builder-import-private.h"

struct _XbBuilderImport {
	GObject			 parent_instance;
	GInputStream		*istream;
	GPtrArray		*node_items;	/* of XbBuilderImportNodeFuncItem */
	XbBuilderNode		*info;
	gchar			*guid;
	gchar			*prefix;
};

G_DEFINE_TYPE (XbBuilderImport, xb_builder_import, G_TYPE_OBJECT)

typedef struct {
	XbBuilderImportNodeFunc		 func;
	gpointer			 user_data;
	GDestroyNotify			 user_data_free;
} XbBuilderImportNodeFuncItem;

/**
 * xb_builder_import_new_file:
 * @file: a #GFile
 * @cancellable: a #GCancellable, or %NULL
 * @error: the #GError, or %NULL
 *
 * Adds an optionally compressed XML file to build a #XbSilo.
 *
 * Returns: (transfer full): a #XbBuilderImport, or NULL for error.
 *
 * Since: 0.1.0
 **/
XbBuilderImport *
xb_builder_import_new_file (GFile *file, GCancellable *cancellable, GError **error)
{
	const gchar *content_type = NULL;
	guint64 mtime;
	g_autofree gchar *fn = NULL;
	g_autoptr(GConverter) conv = NULL;
	g_autoptr(GFileInfo) fileinfo = NULL;
	g_autoptr(GInputStream) istream = NULL;
	g_autoptr(XbBuilderImport) self = g_object_new (XB_TYPE_BUILDER_IMPORT, NULL);

	/* create input stream */
	istream = G_INPUT_STREAM (g_file_read (file, cancellable, error));
	if (istream == NULL)
		return FALSE;

	/* what kind of file is this */
	fileinfo = g_file_query_info (file,
				      G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				      G_FILE_ATTRIBUTE_TIME_MODIFIED,
				      G_FILE_QUERY_INFO_NONE,
				      cancellable,
				      error);
	if (fileinfo == NULL)
		return FALSE;

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
		return FALSE;
	}

	/* success */
	return g_steal_pointer (&self);
}

/**
 * xb_builder_import_set_info:
 * @self: a #XbBuilderImport
 * @info: (allow-none): a #XbBuilderNode
 *
 * Sets an optional information metadata node on the root import node.
 *
 * Since: 0.1.0
 **/
void
xb_builder_import_set_info (XbBuilderImport *self, XbBuilderNode *info)
{
	g_return_if_fail (XB_IS_BUILDER_IMPORT (self));
	g_set_object (&self->info, info);
}

/**
 * xb_builder_import_set_prefix:
 * @self: a #XbBuilderImport
 * @prefix: (allow-none): an XPath prefix, e.g. `installed`
 *
 * Sets an optional prefix on the root import node. This makes any nodes added
 * using this import reside under a common shared parent node.
 *
 * Since: 0.1.0
 **/
void
xb_builder_import_set_prefix (XbBuilderImport *self, const gchar *prefix)
{
	g_return_if_fail (XB_IS_BUILDER_IMPORT (self));
	g_free (self->prefix);
	self->prefix = g_strdup (prefix);
}

/**
 * xb_builder_import_new_xml:
 * @xml: XML data
 * @error: the #GError, or %NULL
 *
 * Parses XML data and begins to build a #XbSilo.
 *
 * Returns: (transfer full): a #XbBuilderImport, or NULL for error.
 *
 * Since: 0.1.0
 **/
XbBuilderImport *
xb_builder_import_new_xml (const gchar *xml, GError **error)
{
	g_autoptr(XbBuilderImport) self = g_object_new (XB_TYPE_BUILDER_IMPORT, NULL);
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
	return g_steal_pointer (&self);
}

/**
 * xb_builder_import_add_node_func:
 * @self: a #XbBuilderImport
 * @func: a callback
 * @user_data: user pointer to pass to @func, or %NULL
 * @user_data_free: a function which gets called to free @user_data, or %NULL
 *
 * Adds a function that will get run on every #XbBuilderNode compile creates.
 *
 * Since: 0.1.0
 **/
void
xb_builder_import_add_node_func (XbBuilderImport *self,
				 XbBuilderImportNodeFunc func,
				 gpointer user_data,
				 GDestroyNotify user_data_free)
{
	XbBuilderImportNodeFuncItem *item;

	g_return_if_fail (XB_IS_BUILDER_IMPORT (self));
	g_return_if_fail (func != NULL);

	item = g_slice_new0 (XbBuilderImportNodeFuncItem);
	item->func = func;
	item->user_data = user_data;
	item->user_data_free = user_data_free;
	g_ptr_array_add (self->node_items, item);
}

gboolean
xb_builder_import_node_func_run (XbBuilderImport *self, XbBuilderNode *bn, GError **error)
{
	for (guint i = 0; i < self->node_items->len; i++) {
		XbBuilderImportNodeFuncItem *item = g_ptr_array_index (self->node_items, i);
		if (!item->func (self, bn, item->user_data, error))
			return FALSE;
	}
	return TRUE;
}

const gchar *
xb_builder_import_get_guid (XbBuilderImport *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_IMPORT (self), NULL);
	return self->guid;
}

const gchar *
xb_builder_import_get_prefix (XbBuilderImport *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_IMPORT (self), NULL);
	return self->prefix;
}

XbBuilderNode *
xb_builder_import_get_info (XbBuilderImport *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_IMPORT (self), NULL);
	return self->info;
}

GInputStream *
xb_builder_import_get_istream (XbBuilderImport *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_IMPORT (self), NULL);
	return self->istream;
}

static void
xb_builder_import_node_func_free (XbBuilderImportNodeFuncItem *item)
{
	if (item->user_data_free != NULL)
		item->user_data_free (item->user_data);
	g_slice_free (XbBuilderImportNodeFuncItem, item);
}

static void
xb_builder_import_finalize (GObject *obj)
{
	XbBuilderImport *self = XB_BUILDER_IMPORT (obj);

	if (self->istream != NULL)
		g_object_unref (self->istream);
	if (self->info != NULL)
		g_object_unref (self->info);
	g_ptr_array_unref (self->node_items);
	g_free (self->guid);
	g_free (self->prefix);

	G_OBJECT_CLASS (xb_builder_import_parent_class)->finalize (obj);
}

static void
xb_builder_import_class_init (XbBuilderImportClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = xb_builder_import_finalize;
}

static void
xb_builder_import_init (XbBuilderImport *self)
{
	self->node_items = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_builder_import_node_func_free);
}
