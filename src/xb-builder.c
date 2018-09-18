/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <gio/gio.h>
#include <string.h>

#include "xb-silo-private.h"
#include "xb-builder.h"
#include "xb-builder-node.h"

struct _XbBuilder {
	GObject			 parent_instance;
	GString			*strtab;
	GString			*guid;
	GHashTable		*strtab_hash;
	gboolean		 used;
	XbBuilderNode		*current;
};

G_DEFINE_TYPE (XbBuilder, xb_builder, G_TYPE_OBJECT)

#define XB_SILO_APPENDBUF(str,data,sz)	g_string_append_len(str,(const gchar *)data, sz);

static guint32
xb_builder_import_add_to_strtab (XbBuilder *self, const gchar *str)
{
	gpointer val;
	guint32 idx;

	/* already exists */
	if (g_hash_table_lookup_extended (self->strtab_hash, str, NULL, &val))
		return GPOINTER_TO_UINT (val);

	/* new */
	idx = self->strtab->len;
	XB_SILO_APPENDBUF (self->strtab, str, strlen (str) + 1);
	g_hash_table_insert (self->strtab_hash, g_strdup (str), GUINT_TO_POINTER (idx));
	return idx;
}

static gchar *
xb_builder_reflow_text (const gchar *text, gssize text_len)
{
	GString *tmp;
	guint newline_count = 0;
	g_auto(GStrv) split = NULL;

	/* all on one line, no trailing or leading whitespace */
	if (g_strstr_len (text, text_len, "\n") == NULL &&
	    !g_str_has_prefix (text, " ") &&
	    !g_str_has_suffix (text, " ")) {
		gsize len;
		len = text_len >= 0 ? (gsize) text_len : strlen (text);
		return g_strndup (text, len);
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
	return g_string_free (tmp, FALSE);
}

typedef struct {
	XbBuilder		*self;
	XbBuilderNode		*current;
	XbBuilderImportFlags	 flags;
	const gchar * const	*locales;
} XbBuilderImportHelper;

static void
xb_builder_import_start_element_cb (GMarkupParseContext *context,
				    const gchar         *element_name,
				    const gchar        **attr_names,
				    const gchar        **attr_values,
				    gpointer             user_data,
				    GError             **error)
{
	XbBuilderImportHelper *helper = (XbBuilderImportHelper *) user_data;
	XbBuilderNode *bn = xb_builder_node_new (element_name);
	XbBuilderNodeData *data = bn->data;
	XbBuilderNodeData *data_parent = helper->current->data;

	/* parent node is being ignored */
	if (data_parent != NULL && data_parent->is_cdata_ignore)
		data->is_cdata_ignore = TRUE;

	/* check if we should ignore the locale */
	if (!data->is_cdata_ignore &&
	    helper->flags & XB_BUILDER_IMPORT_FLAG_NATIVE_LANGS) {
		for (guint i = 0; attr_names[i] != NULL; i++) {
			if (g_strcmp0 (attr_names[i], "xml:lang") == 0) {
				const gchar *lang = attr_values[i];
				if (lang != NULL && !g_strv_contains (helper->locales, lang))
					data->is_cdata_ignore = TRUE;
			}
		}
	}

	/* add attributes */
	if (!data->is_cdata_ignore) {
		for (guint i = 0; attr_names[i] != NULL; i++)
			xb_builder_node_add_attribute (bn, attr_names[i], attr_values[i]);
	}
	helper->current = g_node_append (helper->current, bn);
}

static void
xb_builder_import_end_element_cb (GMarkupParseContext *context,
				  const gchar         *element_name,
				  gpointer             user_data,
				  GError             **error)
{
	XbBuilderImportHelper *helper = (XbBuilderImportHelper *) user_data;
	helper->current = helper->current->parent;
}

static void
xb_builder_import_text_cb (GMarkupParseContext *context,
			   const gchar         *text,
			   gsize                text_len,
			   gpointer             user_data,
			   GError             **error)
{
	XbBuilderImportHelper *helper = (XbBuilderImportHelper *) user_data;
	XbBuilderNodeData *data = helper->current->data;
	guint i;

	/* no data */
	if (text_len == 0)
		return;

	/* unimportant */
	if (data->is_cdata_ignore)
		return;

	/* all whitespace? */
	for (i = 0; i < text_len; i++) {
		if (!g_ascii_isspace (text[i]))
			break;
	}
	if (i >= text_len)
		return;

	/* repair text unless we know it's valid */
	if (helper->flags & XB_BUILDER_IMPORT_FLAG_LITERAL_TEXT) {
		data->text = g_strndup (text, text_len);
	} else {
		data->text = xb_builder_reflow_text (text, text_len);
	}
}

/**
 * xb_builder_import:
 * @self: a #XbSilo
 * @xml: XML data
 * @flags: some #XbBuilderImportFlags, e.g. %XB_BUILDER_IMPORT_FLAG_LITERAL_TEXT
 * @error: the #GError, or %NULL
 *
 * Parses XML data and begins to build a #XbSilo.
 *
 * Returns: %TRUE for success, otherwise @error is set.
 *
 * Since: 0.1.0
 **/
gboolean
xb_builder_import (XbBuilder *self, const gchar *xml, XbBuilderImportFlags flags, GError **error)
{
	g_autoptr(GChecksum) csum = g_checksum_new (G_CHECKSUM_SHA1);
	g_autoptr(GMarkupParseContext) ctx = NULL;
	XbBuilderImportHelper helper = {
		.self = self,
		.flags = flags,
		.current = self->current,
		.locales = g_get_language_names (),
	};
	const GMarkupParser parser = {
		xb_builder_import_start_element_cb,
		xb_builder_import_end_element_cb,
		xb_builder_import_text_cb,
		NULL, NULL };

	g_return_val_if_fail (XB_IS_BUILDER (self), FALSE);
	g_return_val_if_fail (!self->used, FALSE);

	/* parse the XML */
	ctx = g_markup_parse_context_new (&parser, G_MARKUP_PREFIX_ERROR_POSITION, &helper, NULL);
	if (!g_markup_parse_context_parse (ctx, xml, -1, error))
		return FALSE;

	/* add a GUID of the SHA1 hash of the entire string */
	g_checksum_update (csum, (const guchar *) xml, -1);
	xb_builder_append_guid (self, g_checksum_get_string (csum));

	/* more opening than closing */
	if (self->current != helper.current) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "Mismatched XML");
		return FALSE;
	}

	/* success */
	return TRUE;
}

/**
 * xb_builder_import_dir:
 * @self: a #XbSilo
 * @path: a directory path
 * @flags: some #XbBuilderImportFlags, e.g. %XB_BUILDER_IMPORT_FLAG_LITERAL_TEXT
 * @error: the #GError, or %NULL
 * @cancellable: a #GCancellable, or %NULL
 *
 * Parses a directory, parsing any .xml or xml.gz paths into a #XbSilo.
 *
 * Returns: %TRUE for success, otherwise @error is set.
 *
 * Since: 0.1.0
 **/
gboolean
xb_builder_import_dir (XbBuilder *self,
		       const gchar *path,
		       XbBuilderImportFlags flags,
		       GCancellable *cancellable,
		       GError **error)
{
	const gchar *fn;
	g_autoptr(GDir) dir = g_dir_open (path, 0, error);
	if (dir == NULL)
		return FALSE;
	while ((fn = g_dir_read_name (dir)) != NULL) {
		if (g_str_has_suffix (fn, ".xml") ||
		    g_str_has_suffix (fn, ".xml.gz")) {
			g_autofree gchar *filename = g_build_filename (path, fn, NULL);
			g_autoptr(GFile) file = g_file_new_for_path (filename);
			if (!xb_builder_import_file (self, file, flags, cancellable, error))
				return FALSE;
		}
	}
	return TRUE;
}

/**
 * xb_builder_import_file:
 * @self: a #XbSilo
 * @file: a #GFile
 * @flags: some #XbBuilderImportFlags, e.g. %XB_BUILDER_IMPORT_FLAG_LITERAL_TEXT
 * @error: the #GError, or %NULL
 * @cancellable: a #GCancellable, or %NULL
 *
 * Parses an optionally compressed XML file and begins to build a #XbSilo.
 *
 * Returns: %TRUE for success, otherwise @error is set.
 *
 * Since: 0.1.0
 **/
gboolean
xb_builder_import_file (XbBuilder *self,
			GFile *file,
			XbBuilderImportFlags flags,
			GCancellable *cancellable,
			GError **error)
{
	const gchar *content_type = NULL;
	gsize chunk_size = 32 * 1024;
	gssize len;
	g_autofree gchar *data = NULL;
	g_autofree gchar *fn = NULL;
	g_autoptr(GConverter) conv = NULL;
	g_autoptr(GFileInfo) info = NULL;
	g_autoptr(GInputStream) file_stream = NULL;
	g_autoptr(GInputStream) stream_data = NULL;
	g_autoptr(GMarkupParseContext) ctx = NULL;
	XbBuilderImportHelper helper = {
		.self = self,
		.flags = flags,
		.current = self->current,
		.locales = g_get_language_names (),
	};
	const GMarkupParser parser = {
		xb_builder_import_start_element_cb,
		xb_builder_import_end_element_cb,
		xb_builder_import_text_cb,
		NULL, NULL };

	/* what kind of file is this */
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				  G_FILE_QUERY_INFO_NONE,
				  cancellable,
				  error);
	if (info == NULL)
		return FALSE;

	/* add data to GUID */
	fn = g_file_get_path (file);
	xb_builder_append_guid (self, fn);
	//xb_builder_append_guid (self, mtime_as_str); //FIXME

	/* decompress if required */
	file_stream = G_INPUT_STREAM (g_file_read (file, cancellable, error));
	if (file_stream == NULL)
		return FALSE;
	content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	if (g_strcmp0 (content_type, "application/gzip") == 0 ||
	    g_strcmp0 (content_type, "application/x-gzip") == 0) {
		conv = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
		stream_data = g_converter_input_stream_new (file_stream, conv);
	} else if (g_strcmp0 (content_type, "application/xml") == 0) {
		stream_data = g_object_ref (file_stream);
	} else {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "cannot process file of type %s",
			     content_type);
		return FALSE;
	}

	/* parse */
	ctx = g_markup_parse_context_new (&parser, G_MARKUP_PREFIX_ERROR_POSITION, &helper, NULL);
	data = g_malloc (chunk_size);
	while ((len = g_input_stream_read (stream_data,
					   data,
					   chunk_size,
					   cancellable,
					   error)) > 0) {
		if (!g_markup_parse_context_parse (ctx, data, len, error))
			return FALSE;
	}
	if (len < 0)
		return FALSE;

	/* more opening than closing */
	if (self->current != helper.current) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "Mismatched XML");
		return FALSE;
	}

	/* success */
	return TRUE;
}

static gboolean
xb_builder_strtab_element_names_cb (XbBuilderNode *n, gpointer user_data)
{
	XbBuilder *self = XB_BUILDER (user_data);
	XbBuilderNodeData *data = n->data;

	/* root node */
	if (data == NULL)
		return FALSE;
	if (data->is_cdata_ignore)
		return FALSE;
	data->element_name_idx = xb_builder_import_add_to_strtab (self, data->element_name);
	return FALSE;
}

static gboolean
xb_builder_strtab_attr_name_cb (XbBuilderNode *n, gpointer user_data)
{
	XbBuilder *self = XB_BUILDER (user_data);
	XbBuilderNodeData *data = n->data;

	/* root node */
	if (data == NULL)
		return FALSE;
	if (data->is_cdata_ignore)
		return FALSE;
	for (guint i = 0; i < data->attrs->len; i++) {
		XbBuilderNodeAttr *attr = g_ptr_array_index (data->attrs, i);
		attr->name_idx = xb_builder_import_add_to_strtab (self, attr->name);
	}
	return FALSE;
}

static gboolean
xb_builder_strtab_attr_value_cb (XbBuilderNode *n, gpointer user_data)
{
	XbBuilder *self = XB_BUILDER (user_data);
	XbBuilderNodeData *data = n->data;

	/* root node */
	if (data == NULL)
		return FALSE;
	if (data->is_cdata_ignore)
		return FALSE;
	for (guint i = 0; i < data->attrs->len; i++) {
		XbBuilderNodeAttr *attr = g_ptr_array_index (data->attrs, i);
		attr->value_idx = xb_builder_import_add_to_strtab (self, attr->value);
	}
	return FALSE;
}

static gboolean
xb_builder_strtab_text_cb (XbBuilderNode *n, gpointer user_data)
{
	XbBuilder *self = XB_BUILDER (user_data);
	XbBuilderNodeData *data = n->data;

	/* root node */
	if (data == NULL)
		return FALSE;
	if (data->is_cdata_ignore)
		return FALSE;
	if (data->text == NULL)
		return FALSE;
	data->text_idx = xb_builder_import_add_to_strtab (self, data->text);
	return FALSE;
}

static gboolean
xb_builder_nodetab_size_cb (XbBuilderNode *n, gpointer user_data)
{
	guint32 *sz = (guint32 *) user_data;
	XbBuilderNodeData *data = n->data;

	/* root node */
	if (data == NULL)
		return FALSE;
	if (data->is_cdata_ignore)
		return FALSE;
	*sz += xb_builder_node_size (n) + 1; /* +1 for the sentinel */
	if (data->text == NULL)
		*sz -= sizeof(guint32);
	return FALSE;
}

typedef struct {
	GString		*buf;
	guint		 level;
} XbBuilderNodetabHelper;

static void
xb_builder_nodetab_write_sentinel (XbBuilderNodetabHelper *helper)
{
	XbSiloNode n = {
		.is_node	= FALSE,
		.has_text	= FALSE,
		.nr_attrs	= 0,
	};
//	g_debug ("SENT @%u", helper->buf->len);
	XB_SILO_APPENDBUF (helper->buf, &n, xb_silo_node_get_size (&n));
}

static void
xb_builder_nodetab_write_node (XbBuilderNodetabHelper *helper, XbBuilderNode *bn)
{
	XbBuilderNodeData *data = bn->data;
	XbSiloNode n = {
		.is_node	= TRUE,
		.has_text	= data->text != NULL,
		.nr_attrs	= data->attrs->len,
		.element_name	= data->element_name_idx,
		.next		= 0x0,
		.parent		= 0x0,
		.text		= data->text_idx,
	};

	/* save this so we can set up the ->next pointers correctly */
	data->off = helper->buf->len;

//	g_debug ("NODE @%u (%s)", helper->buf->len, data->element_name);
	/* add to the buf */
	if (n.has_text) {
		XB_SILO_APPENDBUF (helper->buf, &n, sizeof(XbSiloNode));
	} else {
		XB_SILO_APPENDBUF (helper->buf, &n, sizeof(XbSiloNode) - sizeof(guint32));
	}

	/* add to the buf */
	for (guint i = 0; i < data->attrs->len; i++) {
		XbBuilderNodeAttr *ba = g_ptr_array_index (data->attrs, i);
		XbSiloAttr attr = {
			.attr_name	= ba->name_idx,
			.attr_value	= ba->value_idx,
		};
		XB_SILO_APPENDBUF (helper->buf, &attr, sizeof(attr));
	}
}

static gboolean
xb_builder_nodetab_write_cb (XbBuilderNode *bn, gpointer user_data)
{
	XbBuilderNodetabHelper *helper = (XbBuilderNodetabHelper *) user_data;
	XbBuilderNodeData *data = bn->data;
	guint depth = g_node_depth (bn);

	/* root node */
	if (data == NULL)
		return FALSE;
	if (data->is_cdata_ignore)
		return FALSE;

	depth = g_node_depth (bn);
	if (depth == 1)
		return FALSE;

	for (guint i = helper->level; i >= depth; i--)
		xb_builder_nodetab_write_sentinel (helper);
	xb_builder_nodetab_write_node (helper, bn);

	helper->level = depth;
	return FALSE;
}

static XbSiloNode *
xb_builder_get_node (GString *str, guint32 off)
{
	return (XbSiloNode *) (str->str + off);
}

static gboolean
xb_builder_nodetab_fix_cb (XbBuilderNode *bn, gpointer user_data)
{
	XbBuilderNodetabHelper *helper = (XbBuilderNodetabHelper *) user_data;
	XbBuilderNode *bn2;
	XbBuilderNodeData *data;
	XbSiloNode *n;

	/* root node */
	data = bn->data;
	if (data == NULL)
		return FALSE;
	if (data->is_cdata_ignore)
		return FALSE;

	/* get the position in the buffer */
	n = xb_builder_get_node (helper->buf, data->off);
	if (n == NULL)
		return FALSE;

	/* set the parent if the node has one */
	bn2 = bn->parent;
	if (bn2 != NULL) {
		XbBuilderNodeData *data2 = bn2->data;
		if (data2 != NULL)
			n->parent = data2->off;
	}

	/* set ->next if the node has one */ 
	bn2 = g_node_next_sibling (bn);
	if (bn2 != NULL) {
		XbBuilderNodeData *data2 = bn2->data;
		n->next = data2->off;
	}

	return FALSE;
}

/**
 * xb_builder_end:
 * @self: a #XbSilo
 * @error: the #GError, or %NULL
 *
 * Finishes building a #XbSilo. Once this method then the caller can no longer
 * call xb_builder_import_file() or xb_builder_import() to add further data.
 *
 * Returns: (transfer full): a #XbSilo, or %NULL for error
 *
 * Since: 0.1.0
 **/
XbSilo *
xb_builder_end (XbBuilder *self, GError **error)
{
	guint32 nodetabsz = sizeof(XbSiloHeader);
	g_autoptr(GBytes) blob = NULL;
	g_autoptr(XbSilo) data = NULL;
	g_autoptr(GString) buf = NULL;
	XbSiloHeader hdr = {
		.magic		= XB_SILO_MAGIC_BYTES,
		.version	= XB_SILO_VERSION,
		.strtab		= 0,
		.padding1	= { 0x0 },
		.guid		= { 0x0 },
	};
	XbBuilderNodetabHelper nodetab_helper = {
		.level = 0,
		.buf = NULL,
	};

	/* get the size of the nodetab */
	g_node_traverse (self->current, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_nodetab_size_cb, &nodetabsz);
	buf = g_string_sized_new (nodetabsz);

	/* add element names, attr name, attr value, then text to the strtab */
	g_node_traverse (self->current, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_strtab_element_names_cb, self);
	g_node_traverse (self->current, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_strtab_attr_name_cb, self);
	g_node_traverse (self->current, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_strtab_attr_value_cb, self);
	g_node_traverse (self->current, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_strtab_text_cb, self);

	/* add the initial header */
	hdr.strtab = nodetabsz;
	if (self->guid->len > 0) {
		guint8 buf2[20];
		gsize buflen = sizeof(buf2);
		g_autoptr(GChecksum) csum = g_checksum_new (G_CHECKSUM_SHA1);
		g_checksum_update (csum, (const guchar *) self->guid->str, self->guid->len);
		g_checksum_get_digest (csum, buf2, &buflen);
		memcpy (&hdr.guid, buf2, 16);
	}
	XB_SILO_APPENDBUF (buf, &hdr, sizeof(XbSiloHeader));

	/* write nodes to the nodetab */
	nodetab_helper.buf = buf;
	g_node_traverse (self->current, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_nodetab_write_cb, &nodetab_helper);
	if (nodetab_helper.level > 0) {
		for (guint i = nodetab_helper.level - 1; i > 0; i--)
			xb_builder_nodetab_write_sentinel (&nodetab_helper);
	}

	/* set all the ->next and ->parent offsets */
	g_node_traverse (self->current, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_nodetab_fix_cb, &nodetab_helper);

	/* append the string table */
	XB_SILO_APPENDBUF (buf, self->strtab->str, self->strtab->len);

	/* create data */
	blob = g_bytes_new (buf->str, buf->len);
	data = xb_silo_new ();
	if (!xb_silo_load_from_bytes (data, blob, XB_SILO_LOAD_FLAG_NONE, error))
		return NULL;

	/* can no longer import XML or add nodes */
	self->used = TRUE;

	/* success */
	return g_steal_pointer (&data);
}

/**
 * xb_builder_append_guid:
 * @self: a #XbSilo
 * @guid: any text, typcically a filename or GUID
 *
 * Adds the GUID to the internal correctness hash.
 *
 * Since: 0.1.0
 **/
void
xb_builder_append_guid (XbBuilder *self, const gchar *guid)
{
	if (self->guid->len > 0)
		g_string_append (self->guid, "&");
	g_string_append (self->guid, guid);
}

static gboolean
xb_builder_destroy_node_cb (XbBuilderNode *bn, gpointer user_data)
{
	/* root node */
	if (bn->data == NULL)
		return FALSE;
	xb_builder_node_free (bn);
	return FALSE;
}

static void
xb_builder_finalize (GObject *obj)
{
	XbBuilder *self = XB_BUILDER (obj);
	g_node_traverse (self->current, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_destroy_node_cb, NULL);
	g_node_destroy (self->current);
	g_string_free (self->strtab, TRUE);
	g_string_free (self->guid, TRUE);
	g_hash_table_unref (self->strtab_hash);
	G_OBJECT_CLASS (xb_builder_parent_class)->finalize (obj);
}

static void
xb_builder_class_init (XbBuilderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = xb_builder_finalize;
}

static void
xb_builder_init (XbBuilder *self)
{
	self->guid = g_string_new (NULL);
	self->strtab = g_string_new (NULL);
	self->strtab_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	self->current = g_node_new (NULL);
}

/**
 * xb_builder_new:
 *
 * Creates a new builder.
 *
 * Returns: a new #XbBuilder
 *
 * Since: 0.1.0
 **/
XbBuilder *
xb_builder_new (void)
{
	return g_object_new (XB_TYPE_BUILDER, NULL);
}
