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
#include "xb-builder-import.h"
#include "xb-builder-node.h"

struct _XbBuilder {
	GObject			 parent_instance;
	GPtrArray		*imports; /* of XbBuilderImport */
	XbSilo			*silo;
	GString			*guid;
};

G_DEFINE_TYPE (XbBuilder, xb_builder, G_TYPE_OBJECT)

#define XB_SILO_APPENDBUF(str,data,sz)	g_string_append_len(str,(const gchar *)data, sz);

typedef struct {
	XbBuilderNode		*root;
	XbBuilderNode		*current;
	XbBuilderCompileFlags	 flags;
	GHashTable		*strtab_hash;
	GString			*strtab;
	const gchar		*import_key;
	const gchar * const	*locales;
} XbBuilderCompileHelper;

static guint32
xb_builder_compile_add_to_strtab (XbBuilderCompileHelper *helper, const gchar *str)
{
	gpointer val;
	guint32 idx;

	/* already exists */
	if (g_hash_table_lookup_extended (helper->strtab_hash, str, NULL, &val))
		return GPOINTER_TO_UINT (val);

	/* new */
	idx = helper->strtab->len;
	XB_SILO_APPENDBUF (helper->strtab, str, strlen (str) + 1);
	g_hash_table_insert (helper->strtab_hash, g_strdup (str), GUINT_TO_POINTER (idx));
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

static void
xb_builder_compile_start_element_cb (GMarkupParseContext *context,
				     const gchar         *element_name,
				     const gchar        **attr_names,
				     const gchar        **attr_values,
				     gpointer             user_data,
				     GError             **error)
{
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;
	XbBuilderNode *bn = xb_builder_node_new (element_name);
	XbBuilderNodeData *data = bn->data;
	XbBuilderNodeData *data_parent = helper->current->data;

	/* add importkey to root element to allow querying later */
	if ((helper->flags & XB_BUILDER_COMPILE_FLAG_ADD_IMPORT_KEY) > 0 &&
	    helper->current == helper->root &&
	    helper->import_key != NULL) {
		xb_builder_node_add_attribute (bn, "XMLb::ImportKey",
					       helper->import_key);
	}

	/* parent node is being ignored */
	if (data_parent != NULL && data_parent->is_cdata_ignore)
		data->is_cdata_ignore = TRUE;

	/* check if we should ignore the locale */
	if (!data->is_cdata_ignore &&
	    helper->flags & XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS) {
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
xb_builder_compile_end_element_cb (GMarkupParseContext *context,
				  const gchar         *element_name,
				  gpointer             user_data,
				  GError             **error)
{
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;
	helper->current = helper->current->parent;
}

static void
xb_builder_compile_text_cb (GMarkupParseContext *context,
			   const gchar         *text,
			   gsize                text_len,
			   gpointer             user_data,
			   GError             **error)
{
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;
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
	if (helper->flags & XB_BUILDER_COMPILE_FLAG_LITERAL_TEXT) {
		data->text = g_strndup (text, text_len);
	} else {
		data->text = xb_builder_reflow_text (text, text_len);
	}
}

/**
 * xb_builder_import_xml:
 * @self: a #XbSilo
 * @xml: XML data
 * @error: the #GError, or %NULL
 *
 * Parses XML data and begins to build a #XbSilo.
 *
 * Returns: %TRUE for success, otherwise @error is set.
 *
 * Since: 0.1.0
 **/
gboolean
xb_builder_import_xml (XbBuilder *self, const gchar *xml, GError **error)
{
	XbBuilderImport *import;

	g_return_val_if_fail (XB_IS_BUILDER (self), FALSE);
	g_return_val_if_fail (xml != NULL, FALSE);

	/* add import */
	import = xb_builder_import_new_xml (xml, error);
	if (import == NULL)
		return FALSE;

	/* success */
	xb_builder_append_guid (self, xb_builder_import_get_guid (import));
	g_ptr_array_add (self->imports, import);
	return TRUE;
}

/**
 * xb_builder_import_dir:
 * @self: a #XbSilo
 * @path: a directory path
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
			if (!xb_builder_import_file (self, file, cancellable, error))
				return FALSE;
		}
	}
	return TRUE;
}

/**
 * xb_builder_import_file:
 * @self: a #XbSilo
 * @file: a #GFile
 * @error: the #GError, or %NULL
 * @cancellable: a #GCancellable, or %NULL
 *
 * Adds an optionally compressed XML file to build a #XbSilo.
 *
 * Returns: %TRUE for success, otherwise @error is set.
 *
 * Since: 0.1.0
 **/
gboolean
xb_builder_import_file (XbBuilder *self, GFile *file, GCancellable *cancellable, GError **error)
{
	XbBuilderImport *import;

	g_return_val_if_fail (XB_IS_BUILDER (self), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	/* add import */
	import = xb_builder_import_new_file (file, cancellable, error);
	if (import == NULL)
		return FALSE;

	/* success */
	xb_builder_append_guid (self, xb_builder_import_get_guid (import));
	g_ptr_array_add (self->imports, import);
	return TRUE;
}

static gboolean
xb_builder_compile_import (XbBuilderCompileHelper *helper,
			   XbBuilderImport *import,
			   GCancellable *cancellable,
			   GError **error)
{
	GInputStream *istream = xb_builder_import_get_istream (import);
	gsize chunk_size = 32 * 1024;
	gssize len;
	g_autofree gchar *data = NULL;
	g_autoptr(GMarkupParseContext) ctx = NULL;
	const GMarkupParser parser = {
		xb_builder_compile_start_element_cb,
		xb_builder_compile_end_element_cb,
		xb_builder_compile_text_cb,
		NULL, NULL };

	/* this is something we can query with later */
	helper->import_key = xb_builder_import_get_key (import);

	/* parse */
	ctx = g_markup_parse_context_new (&parser, G_MARKUP_PREFIX_ERROR_POSITION, helper, NULL);
	data = g_malloc (chunk_size);
	while ((len = g_input_stream_read (istream,
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
	if (helper->root != helper->current) {
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
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;
	XbBuilderNodeData *data = n->data;

	/* root node */
	if (data == NULL)
		return FALSE;
	if (data->is_cdata_ignore)
		return FALSE;
	data->element_name_idx = xb_builder_compile_add_to_strtab (helper, data->element_name);
	return FALSE;
}

static gboolean
xb_builder_strtab_attr_name_cb (XbBuilderNode *n, gpointer user_data)
{
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;
	XbBuilderNodeData *data = n->data;

	/* root node */
	if (data == NULL)
		return FALSE;
	if (data->is_cdata_ignore)
		return FALSE;
	for (guint i = 0; i < data->attrs->len; i++) {
		XbBuilderNodeAttr *attr = g_ptr_array_index (data->attrs, i);
		attr->name_idx = xb_builder_compile_add_to_strtab (helper, attr->name);
	}
	return FALSE;
}

static gboolean
xb_builder_strtab_attr_value_cb (XbBuilderNode *n, gpointer user_data)
{
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;
	XbBuilderNodeData *data = n->data;

	/* root node */
	if (data == NULL)
		return FALSE;
	if (data->is_cdata_ignore)
		return FALSE;
	for (guint i = 0; i < data->attrs->len; i++) {
		XbBuilderNodeAttr *attr = g_ptr_array_index (data->attrs, i);
		attr->value_idx = xb_builder_compile_add_to_strtab (helper, attr->value);
	}
	return FALSE;
}

static gboolean
xb_builder_strtab_text_cb (XbBuilderNode *n, gpointer user_data)
{
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;
	XbBuilderNodeData *data = n->data;

	/* root node */
	if (data == NULL)
		return FALSE;
	if (data->is_cdata_ignore)
		return FALSE;
	if (data->text == NULL)
		return FALSE;
	data->text_idx = xb_builder_compile_add_to_strtab (helper, data->text);
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
xb_builder_compile_helper_free (XbBuilderCompileHelper *helper)
{
	g_hash_table_unref (helper->strtab_hash);
	g_string_free (helper->strtab, TRUE);
	g_node_traverse (helper->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_destroy_node_cb, NULL);
	g_node_destroy (helper->root);
	g_free (helper);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XbBuilderCompileHelper, xb_builder_compile_helper_free)

static gchar *
xb_builder_generate_guid (XbBuilder *self)
{
	uuid_t ns;
	uuid_t guid;
	gchar guid_tmp[UUID_STR_LEN] = { '\0' };

	uuid_clear (ns);
	uuid_generate_sha1 (guid, ns, self->guid->str, self->guid->len);
	uuid_unparse (guid, guid_tmp);
	return g_strdup (guid_tmp);
}

/**
 * xb_builder_compile:
 * @self: a #XbSilo
 * @flags: some #XbBuilderCompileFlags, e.g. %XB_BUILDER_COMPILE_FLAG_LITERAL_TEXT
 * @cancellable: a #GCancellable, or %NULL
 * @error: the #GError, or %NULL
 *
 * Compiles a #XbSilo.
 *
 * Returns: (transfer full): a #XbSilo, or %NULL for error
 *
 * Since: 0.1.0
 **/
XbSilo *
xb_builder_compile (XbBuilder *self, XbBuilderCompileFlags flags, GCancellable *cancellable, GError **error)
{
	guint32 nodetabsz = sizeof(XbSiloHeader);
	g_autoptr(GBytes) blob = NULL;
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
	g_autoptr(XbBuilderCompileHelper) helper = NULL;

	g_return_val_if_fail (XB_IS_BUILDER (self), NULL);

	/* create helper used for compiling */
	helper = g_new0 (XbBuilderCompileHelper, 1);
	helper->flags = helper->flags;
	helper->root = g_node_new (NULL);
	helper->locales = g_get_language_names ();
	helper->strtab = g_string_new (NULL);
	helper->strtab_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	/* build node tree */
	for (guint i = 0; i < self->imports->len; i++) {
		XbBuilderImport *import = g_ptr_array_index (self->imports, i);
		g_autoptr(GError) error_local = NULL;

		/* don't allow damaged XML files to ruin all the next ones */
		helper->current = helper->root;

		g_debug ("compiling %s…", xb_builder_import_get_guid (import));
		if (!xb_builder_compile_import (helper, import, cancellable, &error_local)) {
			if (flags & XB_BUILDER_COMPILE_FLAG_IGNORE_INVALID) {
				g_debug ("ignoring invalid file %s: %s",
					 xb_builder_import_get_guid (import),
					 error_local->message);
				continue;
			}
			g_propagate_prefixed_error (error,
						    g_steal_pointer (&error_local),
						    "failed to compile %s: ",
						    xb_builder_import_get_guid (import));
			return NULL;
		}
	}

	/* get the size of the nodetab */
	g_node_traverse (helper->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_nodetab_size_cb, &nodetabsz);
	buf = g_string_sized_new (nodetabsz);

	/* add element names, attr name, attr value, then text to the strtab */
	g_node_traverse (helper->root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_strtab_element_names_cb, helper);
	g_node_traverse (helper->root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_strtab_attr_name_cb, helper);
	g_node_traverse (helper->root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_strtab_attr_value_cb, helper);
	g_node_traverse (helper->root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_strtab_text_cb, helper);

	/* add the initial header */
	hdr.strtab = nodetabsz;
	if (self->guid->len > 0) {
		uuid_t ns;
		uuid_clear (ns);
		uuid_generate_sha1 (hdr.guid, ns, self->guid->str, self->guid->len);
	}
	XB_SILO_APPENDBUF (buf, &hdr, sizeof(XbSiloHeader));

	/* write nodes to the nodetab */
	nodetab_helper.buf = buf;
	g_node_traverse (helper->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_nodetab_write_cb, &nodetab_helper);
	if (nodetab_helper.level > 0) {
		for (guint i = nodetab_helper.level - 1; i > 0; i--)
			xb_builder_nodetab_write_sentinel (&nodetab_helper);
	}

	/* set all the ->next and ->parent offsets */
	g_node_traverse (helper->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			 xb_builder_nodetab_fix_cb, &nodetab_helper);

	/* append the string table */
	XB_SILO_APPENDBUF (buf, helper->strtab->str, helper->strtab->len);

	/* create data */
	blob = g_bytes_new (buf->str, buf->len);
	if (!xb_silo_load_from_bytes (self->silo, blob, XB_SILO_LOAD_FLAG_NONE, error))
		return NULL;

	/* success */
	return g_object_ref (self->silo);
}

/**
 * xb_builder_ensure:
 * @self: a #XbSilo
 * @file: a #GFile
 * @flags: some #XbBuilderCompileFlags, e.g. %XB_BUILDER_COMPILE_FLAG_LITERAL_TEXT
 * @cancellable: a #GCancellable, or %NULL
 * @error: the #GError, or %NULL
 *
 * Ensures @file is up to date, and returns a compiled #XbSilo.
 *
 * If @silo is being used by a query (e.g. in another thread) then all node
 * data is immediately invalid.
 *
 * Returns: (transfer full): a #XbSilo, or %NULL for error
 *
 * Since: 0.1.0
 **/
XbSilo *
xb_builder_ensure (XbBuilder *self, GFile *file, XbBuilderCompileFlags flags,
		   GCancellable *cancellable, GError **error)
{
	g_autofree gchar *fn = NULL;
	g_autoptr(XbSilo) silo_tmp = xb_silo_new ();
	g_autoptr(XbSilo) silo_new = NULL;
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GFile) file_parent = NULL;

	g_return_val_if_fail (XB_IS_BUILDER (self), NULL);
	g_return_val_if_fail (G_IS_FILE (file), NULL);

	/* load the file and peek at the GUIDs */
	fn = g_file_get_path (file);
	g_debug ("attempting to load %s", fn);
	if (!xb_silo_load_from_file (silo_tmp, file, XB_SILO_LOAD_FLAG_NONE, &error_local)) {
		g_debug ("failed to load silo: %s", error_local->message);
	} else {
		g_autofree gchar *guid = xb_builder_generate_guid (self);
		g_debug ("file: %s, current:%s, cached: %s",
			 xb_silo_get_guid (silo_tmp), guid,
			 xb_silo_get_guid (self->silo));

		/* GUIDs match exactly with the thing that's already loaded */
		if (g_strcmp0 (xb_silo_get_guid (silo_tmp),
			       xb_silo_get_guid (self->silo)) == 0) {
			g_debug ("returning unchanged silo");
			return g_object_ref (self->silo);
		}

		/* reload the cached silo with the new file data */
		if (g_strcmp0 (xb_silo_get_guid (silo_tmp), guid) == 0) {
			g_autoptr(GBytes) blob = xb_silo_get_bytes (silo_tmp);
			g_debug ("loading silo with file contents");
			if (!xb_silo_load_from_bytes (self->silo, blob,
						      XB_SILO_LOAD_FLAG_NONE, error))
				return NULL;
			return g_object_ref (self->silo);
		}
	}

	/* ensure parent directories exist */
	file_parent = g_file_get_parent (file);
	if (file_parent != NULL &&
	    !g_file_query_exists (file_parent, cancellable)) {
		if (!g_file_make_directory_with_parents (file_parent,
							 cancellable,
							 error))
			return NULL;
	}

	/* fallback to just creating a new file */
	silo_new = xb_builder_compile (self, flags, cancellable, error);
	if (silo_new == NULL)
		return NULL;
	if (!xb_silo_save_to_file (silo_new, file, error))
		return NULL;
	return g_steal_pointer (&silo_new);
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

static void
xb_builder_finalize (GObject *obj)
{
	XbBuilder *self = XB_BUILDER (obj);

	g_ptr_array_unref (self->imports);
	g_object_unref (self->silo);
	g_string_free (self->guid, TRUE);

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
	self->imports = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	self->silo = xb_silo_new ();
	self->guid = g_string_new (NULL);
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
