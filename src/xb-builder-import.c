/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <gio/gio.h>

#include "xb-builder-import.h"

struct _XbBuilderImport {
	GObject			 parent_instance;
	GInputStream		*istream;
	gchar			*guid;
	gchar			*key;
};

G_DEFINE_TYPE (XbBuilderImport, xb_builder_import, G_TYPE_OBJECT)

XbBuilderImport *
xb_builder_import_new_file (GFile *file, GCancellable *cancellable, GError **error)
{
	const gchar *content_type = NULL;
	guint64 mtime;
	g_autoptr(GConverter) conv = NULL;
	g_autoptr(GFileInfo) info = NULL;
	g_autoptr(GInputStream) istream = NULL;
	g_autoptr(XbBuilderImport) self = g_object_new (XB_TYPE_BUILDER_IMPORT, NULL);

	/* create input stream */
	istream = G_INPUT_STREAM (g_file_read (file, cancellable, error));
	if (istream == NULL)
		return FALSE;

	/* what kind of file is this */
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				  G_FILE_ATTRIBUTE_TIME_MODIFIED,
				  G_FILE_QUERY_INFO_NONE,
				  cancellable,
				  error);
	if (info == NULL)
		return FALSE;

	/* add data to GUID */
	self->key = g_file_get_path (file);
	mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	self->guid = g_strdup_printf ("%s:%" G_GUINT64_FORMAT, self->key, mtime);

	/* decompress if required */
	content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
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

const gchar *
xb_builder_import_get_guid (XbBuilderImport *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_IMPORT (self), NULL);
	return self->guid;
}

const gchar *
xb_builder_import_get_key (XbBuilderImport *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_IMPORT (self), NULL);
	return self->key;
}

GInputStream *
xb_builder_import_get_istream (XbBuilderImport *self)
{
	g_return_val_if_fail (XB_IS_BUILDER_IMPORT (self), NULL);
	return self->istream;
}

static void
xb_builder_import_finalize (GObject *obj)
{
	XbBuilderImport *self = XB_BUILDER_IMPORT (obj);

	if (self->istream != NULL)
		g_object_unref (self->istream);
	g_free (self->guid);
	g_free (self->key);

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
}
