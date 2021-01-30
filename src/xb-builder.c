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
#include "xb-string-private.h"
#include "xb-opcode-private.h"
#include "xb-builder.h"
#include "xb-builder-fixup-private.h"
#include "xb-builder-source-private.h"
#include "xb-builder-node-private.h"

typedef struct {
	GPtrArray		*sources;	/* of XbBuilderSource */
	GPtrArray		*nodes;		/* of XbBuilderNode */
	GPtrArray		*fixups;	/* of XbBuilderFixup */
	GPtrArray		*locales;	/* of str */
	XbSilo			*silo;
	XbSiloProfileFlags	 profile_flags;
	GString			*guid;
} XbBuilderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XbBuilder, xb_builder, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (xb_builder_get_instance_private (o))

#define XB_SILO_APPENDBUF(str,data,sz)	g_string_append_len(str,(const gchar *)data, sz);

typedef struct {
	XbSilo			*silo;
	XbBuilderNode		*root;		/* transfer full */
	XbBuilderNode		*current;	/* transfer none */
	XbBuilderCompileFlags	 compile_flags;
	XbBuilderSourceFlags	 source_flags;
	GHashTable		*strtab_hash;
	GString			*strtab;
	GPtrArray		*locales;
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

static gint
xb_builder_get_locale_priority (XbBuilderCompileHelper *helper, const gchar *locale)
{
	for (guint i = 0; i < helper->locales->len; i++) {
		const gchar *locale_tmp = g_ptr_array_index (helper->locales, i);
		if (g_strcmp0 (locale_tmp, locale) == 0)
			return helper->locales->len - i;
	}
	return -1;
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
	g_autoptr(XbBuilderNode) bn = xb_builder_node_new (element_name);

	/* parent node is being ignored */
	if (helper->current != NULL &&
	    xb_builder_node_has_flag (helper->current, XB_BUILDER_NODE_FLAG_IGNORE))
		xb_builder_node_add_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE);

	/* check if we should ignore the locale */
	if (!xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE) &&
	    helper->compile_flags & XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS) {
		const gchar *xml_lang = NULL;
		for (guint i = 0; attr_names[i] != NULL; i++) {
			if (g_strcmp0 (attr_names[i], "xml:lang") == 0) {
				xml_lang = attr_values[i];
				break;
			}
		}
		if (xml_lang == NULL) {
			if (helper->current != NULL) {
				gint prio = xb_builder_node_get_priority (helper->current);
				xb_builder_node_set_priority (bn, prio);
			}
		} else {
			gint prio = xb_builder_get_locale_priority (helper, xml_lang);
			if (prio < 0)
				xb_builder_node_add_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE);
			xb_builder_node_set_priority (bn, prio);
		}
	}

	/* add attributes */
	if (!xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE)) {
		for (guint i = 0; attr_names[i] != NULL; i++)
			xb_builder_node_set_attr (bn, attr_names[i], attr_values[i]);
	}

	/* add to tree */
	xb_builder_node_add_child (helper->current, bn);
	helper->current = bn;
}

static void
xb_builder_compile_end_element_cb (GMarkupParseContext *context,
				  const gchar         *element_name,
				  gpointer             user_data,
				  GError             **error)
{
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;
	g_autoptr(XbBuilderNode) parent = xb_builder_node_get_parent (helper->current);
	if (parent == NULL) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "Mismatched XML; no parent");
		return;
	}
	helper->current = parent;
}

static void
xb_builder_compile_text_cb (GMarkupParseContext *context,
			   const gchar         *text,
			   gsize                text_len,
			   gpointer             user_data,
			   GError             **error)
{
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;
	XbBuilderNode *bn = helper->current;
	XbBuilderNode *bc = xb_builder_node_get_last_child (bn);

	/* unimportant */
	if (xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE))
		return;

	/* repair text unless we know it's valid */
	if (helper->source_flags & XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT)
		xb_builder_node_add_flag (bn, XB_BUILDER_NODE_FLAG_LITERAL_TEXT);

	/* text or tail */
	if (!xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_HAS_TEXT)) {
		xb_builder_node_set_text (bn, text, text_len);
		return;
	}

	/* does this node have a child */
	if (bc != NULL) {
		xb_builder_node_set_tail (bc, text, text_len);
		return;
	}

	/* always set a tail, even if already set */
	xb_builder_node_set_tail (bn, text, text_len);
}

/**
 * xb_builder_import_source:
 * @self: a #XbSilo
 * @source: a #XbBuilderSource
 *
 * Adds a #XbBuilderSource to the #XbBuilder.
 *
 * Since: 0.1.0
 **/
void
xb_builder_import_source (XbBuilder *self, XbBuilderSource *source)
{
	XbBuilderPrivate *priv = GET_PRIVATE (self);
	g_autofree gchar *guid = NULL;

	g_return_if_fail (XB_IS_BUILDER (self));
	g_return_if_fail (XB_IS_BUILDER_SOURCE (source));

	/* get latest GUID */
	guid = xb_builder_source_get_guid (source);
	xb_builder_append_guid (self, guid);
	g_ptr_array_add (priv->sources, g_object_ref (source));
}

static gboolean
xb_builder_compile_source (XbBuilderCompileHelper *helper,
			   XbBuilderSource *source,
			   XbBuilderNode *root,
			   GCancellable *cancellable,
			   GError **error)
{
	GPtrArray *children;
	XbBuilderNode *info;
	gsize chunk_size = 32 * 1024;
	gssize len;
	g_autofree gchar *data = NULL;
	g_autofree gchar *guid = xb_builder_source_get_guid (source);
	g_autoptr(GPtrArray) children_copy = NULL;
	g_autoptr(GInputStream) istream = NULL;
	g_autoptr(GMarkupParseContext) ctx = NULL;
	g_autoptr(GTimer) timer = xb_silo_start_profile (helper->silo);
	g_autoptr(XbBuilderNode) root_tmp = xb_builder_node_new (NULL);
	const GMarkupParser parser = {
		xb_builder_compile_start_element_cb,
		xb_builder_compile_end_element_cb,
		xb_builder_compile_text_cb,
		NULL, NULL };

	/* add the source to a fake root in case it fails during processing */
	helper->current = root_tmp;
	helper->source_flags = xb_builder_source_get_flags (source);

	/* decompress */
	istream = xb_builder_source_get_istream (source, cancellable, error);
	if (istream == NULL)
		return FALSE;

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
	if (root_tmp != helper->current) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "Mismatched XML");
		return FALSE;
	}

	/* run any node functions */
	if (!xb_builder_source_fixup (source, root_tmp, error))
		return FALSE;

	/* this is something we can query with later */
	info = xb_builder_source_get_info (source);
	if (info != NULL) {
		children = xb_builder_node_get_children (helper->current);
		for (guint i = 0; i < children->len; i++) {
			XbBuilderNode *bn = g_ptr_array_index (children, i);
			xb_builder_node_add_child (bn, info);
		}
	}

	/* add the children to the main document */
	children = xb_builder_node_get_children (root_tmp);
	children_copy = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (guint i = 0; i < children->len; i++) {
		XbBuilderNode *bn = g_ptr_array_index (children, i);
		g_ptr_array_add (children_copy, g_object_ref (bn));
	}
	for (guint i = 0; i < children_copy->len; i++) {
		XbBuilderNode *bn = g_ptr_array_index (children_copy, i);
		xb_builder_node_unlink (bn);
		xb_builder_node_add_child (root, bn);
	}

	/* success */
	xb_silo_add_profile (helper->silo, timer, "compile %s", guid);
	return TRUE;
}

static gboolean
xb_builder_strtab_element_names_cb (XbBuilderNode *bn, gpointer user_data)
{
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;
	const gchar *tmp;

	/* root node */
	if (xb_builder_node_get_element (bn) == NULL)
		return FALSE;
	if (xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE))
		return FALSE;
	tmp = xb_builder_node_get_element (bn);
	xb_builder_node_set_element_idx (bn, xb_builder_compile_add_to_strtab (helper, tmp));
	return FALSE;
}

static gboolean
xb_builder_strtab_attr_name_cb (XbBuilderNode *bn, gpointer user_data)
{
	GPtrArray *attrs;
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;

	/* root node */
	if (xb_builder_node_get_element (bn) == NULL)
		return FALSE;
	if (xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE))
		return FALSE;
	attrs = xb_builder_node_get_attrs (bn);
	for (guint i = 0; attrs != NULL && i < attrs->len; i++) {
		XbBuilderNodeAttr *attr = g_ptr_array_index (attrs, i);
		attr->name_idx = xb_builder_compile_add_to_strtab (helper, attr->name);
	}
	return FALSE;
}

static gboolean
xb_builder_strtab_attr_value_cb (XbBuilderNode *bn, gpointer user_data)
{
	GPtrArray *attrs;
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;

	/* root node */
	if (xb_builder_node_get_element (bn) == NULL)
		return FALSE;
	if (xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE))
		return FALSE;
	attrs = xb_builder_node_get_attrs (bn);
	for (guint i = 0; attrs != NULL && i < attrs->len; i++) {
		XbBuilderNodeAttr *attr = g_ptr_array_index (attrs, i);
		attr->value_idx = xb_builder_compile_add_to_strtab (helper, attr->value);
	}
	return FALSE;
}

static gboolean
xb_builder_strtab_text_cb (XbBuilderNode *bn, gpointer user_data)
{
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;
	const gchar *tmp;

	/* root node */
	if (xb_builder_node_get_element (bn) == NULL)
		return FALSE;
	if (xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE))
		return FALSE;
	if (xb_builder_node_get_text (bn) != NULL) {
		tmp = xb_builder_node_get_text (bn);
		xb_builder_node_set_text_idx (bn, xb_builder_compile_add_to_strtab (helper, tmp));
	}
	if (xb_builder_node_get_tail (bn) != NULL) {
		tmp = xb_builder_node_get_tail (bn);
		xb_builder_node_set_tail_idx (bn, xb_builder_compile_add_to_strtab (helper, tmp));
	}
	return FALSE;
}

static gboolean
xb_builder_strtab_tokens_cb (XbBuilderNode *bn, gpointer user_data)
{
	XbBuilderCompileHelper *helper = (XbBuilderCompileHelper *) user_data;
	GPtrArray *tokens = xb_builder_node_get_tokens (bn);

	/* root node */
	if (xb_builder_node_get_element (bn) == NULL)
		return FALSE;
	if (xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE))
		return FALSE;
	if (tokens == NULL)
		return FALSE;
	for (guint i = 0; i < tokens->len; i++) {
		const gchar *tmp = g_ptr_array_index (tokens, i);
		if (tmp == NULL)
			continue;
		xb_builder_node_add_token_idx (bn, xb_builder_compile_add_to_strtab (helper, tmp));
	}
	return FALSE;
}

static gboolean
xb_builder_xml_lang_prio_cb (XbBuilderNode *bn, gpointer user_data)
{
	GPtrArray *nodes_to_destroy = (GPtrArray *) user_data;
	gint prio_best = 0;
	g_autoptr(GPtrArray) nodes = g_ptr_array_new ();
	GPtrArray *siblings;
	g_autoptr(XbBuilderNode) parent = xb_builder_node_get_parent (bn);

	/* root node */
	if (xb_builder_node_get_element (bn) == NULL)
		return FALSE;

	/* already ignored */
	if (xb_builder_node_get_priority (bn) == -2)
		return FALSE;

	/* get all the siblings with the same name */
	siblings = xb_builder_node_get_children (parent);
	for (guint i = 0; i < siblings->len; i++) {
		XbBuilderNode *bn2 = g_ptr_array_index (siblings, i);
		if (g_strcmp0 (xb_builder_node_get_element (bn),
			       xb_builder_node_get_element (bn2)) == 0)
			g_ptr_array_add (nodes, bn2);
	}

	/* only one thing, so bail early */
	if (nodes->len == 1)
		return FALSE;

	/* find the best locale */
	for (guint i = 0; i < nodes->len; i++) {
		XbBuilderNode *bn2 = g_ptr_array_index (nodes, i);
		if (xb_builder_node_get_priority (bn2) > prio_best)
			prio_best = xb_builder_node_get_priority (bn2);
	}

	/* add any nodes not as good as the bext locale to the kill list */
	for (guint i = 0; i < nodes->len; i++) {
		XbBuilderNode *bn2 = g_ptr_array_index (nodes, i);
		if (xb_builder_node_get_priority (bn2) < prio_best)
			g_ptr_array_add (nodes_to_destroy, bn2);

		/* never visit this node again */
		xb_builder_node_set_priority (bn2, -2);
	}

	return FALSE;
}

static gboolean
xb_builder_nodetab_size_cb (XbBuilderNode *bn, gpointer user_data)
{
	guint32 *sz = (guint32 *) user_data;

	/* root node */
	if (xb_builder_node_get_element (bn) == NULL)
		return FALSE;
	if (xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE))
		return FALSE;
	*sz += xb_builder_node_size (bn) + 1; /* +1 for the sentinel */
	return FALSE;
}

typedef struct {
	GString		*buf;
} XbBuilderNodetabHelper;

static void
xb_builder_nodetab_write_sentinel (XbBuilderNodetabHelper *helper)
{
	XbSiloNode sn = {
		.flags		= XB_SILO_NODE_FLAG_NONE,
		.attr_count	= 0,
	};
//	g_debug ("SENT @%u", (guint) helper->buf->len);
	XB_SILO_APPENDBUF (helper->buf, &sn, xb_silo_node_get_size (&sn));
}

static void
xb_builder_nodetab_write_node (XbBuilderNodetabHelper *helper, XbBuilderNode *bn)
{
	GPtrArray *attrs = xb_builder_node_get_attrs (bn);
	GArray *token_idxs = xb_builder_node_get_token_idxs (bn);
	XbSiloNode sn = {
		.flags		= XB_SILO_NODE_FLAG_IS_ELEMENT,
		.attr_count	= (attrs != NULL) ? attrs->len : 0,
		.element_name	= xb_builder_node_get_element_idx (bn),
		.next		= 0x0,
		.parent		= 0x0,
		.text		= xb_builder_node_get_text_idx (bn),
		.tail		= xb_builder_node_get_tail_idx (bn),
		.token_count	= 0,
	};

	/* add tokens */
	if (xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_TOKENIZE_TEXT))
		sn.flags |= XB_SILO_NODE_FLAG_IS_TOKENIZED;

	/* if the node had no children and the text is just whitespace then
	 * remove it even in literal mode */
	if (xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_LITERAL_TEXT)) {
		if (xb_string_isspace (xb_builder_node_get_text (bn), -1))
			sn.text = XB_SILO_UNSET;
		if (xb_string_isspace (xb_builder_node_get_tail (bn), -1))
			sn.tail = XB_SILO_UNSET;
	}

	/* save this so we can set up the ->next pointers correctly */
	xb_builder_node_set_offset (bn, helper->buf->len);

//	g_debug ("NODE @%u (%s)", (guint) helper->buf->len, xb_builder_node_get_element (bn));

	/* there is no point adding more tokens than we can match */
	if (token_idxs != NULL)
		sn.token_count = MIN (token_idxs->len, XB_OPCODE_TOKEN_MAX);

	/* add to the buf */
	XB_SILO_APPENDBUF (helper->buf, &sn, sizeof(XbSiloNode));

	/* add to the buf */
	for (guint i = 0; attrs != NULL && i < attrs->len; i++) {
		XbBuilderNodeAttr *ba = g_ptr_array_index (attrs, i);
		XbSiloNodeAttr attr = {
			.attr_name	= ba->name_idx,
			.attr_value	= ba->value_idx,
		};
		XB_SILO_APPENDBUF (helper->buf, &attr, sizeof(attr));
	}

	/* add tokens */
	for (guint i = 0; i < sn.token_count; i++) {
		guint32 idx = g_array_index (token_idxs, guint32, i);
		XB_SILO_APPENDBUF (helper->buf, &idx, sizeof(idx));
	}
}

static void
xb_builder_nodetab_write (XbBuilderNodetabHelper *helper, XbBuilderNode *bn)
{
	GPtrArray *children;

	/* ignore this */
	if (xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE))
		return;

	/* element */
	if (xb_builder_node_get_element (bn) != NULL)
		xb_builder_nodetab_write_node (helper, bn);

	/* children */
	children = xb_builder_node_get_children (bn);
	for (guint i = 0; i < children->len; i++) {
		XbBuilderNode *bc = g_ptr_array_index (children, i);
		xb_builder_nodetab_write (helper, bc);
	}

	/* sentinel */
	if (xb_builder_node_get_element (bn) != NULL)
		xb_builder_nodetab_write_sentinel (helper);
}


static XbSiloNode *
xb_builder_get_node (GString *str, guint32 off)
{
	return (XbSiloNode *) (str->str + off);
}

static gboolean
xb_builder_nodetab_fix_cb (XbBuilderNode *bn, gpointer user_data)
{
	GPtrArray *siblings;
	XbBuilderNodetabHelper *helper = (XbBuilderNodetabHelper *) user_data;
	XbSiloNode *sn;
	gboolean found = FALSE;
	g_autoptr(XbBuilderNode) parent = xb_builder_node_get_parent (bn);

	/* root node */
	if (xb_builder_node_get_element (bn) == NULL)
		return FALSE;
	if (xb_builder_node_has_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE))
		return FALSE;

	/* get the position in the buffer */
	sn = xb_builder_get_node (helper->buf, xb_builder_node_get_offset (bn));
	if (sn == NULL)
		return FALSE;

	/* set the parent if the node has one */
	if (xb_builder_node_get_element (parent) != NULL)
		sn->parent = xb_builder_node_get_offset (parent);

	/* set ->next if the node has one */
	siblings = xb_builder_node_get_children (parent);
	for (guint i = 0; i < siblings->len; i++) {
		XbBuilderNode *bn2 = g_ptr_array_index (siblings, i);
		if (bn2 == bn) {
			found = TRUE;
			continue;
		}
		if (!found)
			continue;
		if (!xb_builder_node_has_flag (bn2, XB_BUILDER_NODE_FLAG_IGNORE)) {
			sn->next = xb_builder_node_get_offset (bn2);
			break;
		}
	}

	return FALSE;
}

static void
xb_builder_compile_helper_free (XbBuilderCompileHelper *helper)
{
	g_hash_table_unref (helper->strtab_hash);
	g_string_free (helper->strtab, TRUE);
	g_object_unref (helper->root);
	g_free (helper);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XbBuilderCompileHelper, xb_builder_compile_helper_free)

static gchar *
xb_builder_generate_guid (XbBuilder *self)
{
	XbBuilderPrivate *priv = GET_PRIVATE (self);
	XbGuid guid = { 0x0 };
	if (priv->guid->len > 0) {
		xb_guid_compute_for_data (&guid,
					  (const guint8 *) priv->guid->str,
					  priv->guid->len);
	}
	return xb_guid_to_string (&guid);
}

/**
 * xb_builder_import_node:
 * @self: a #XbSilo
 * @bn: a #XbBuilderNode
 *
 * Adds a node tree to the builder.
 *
 * Since: 0.1.0
 **/
void
xb_builder_import_node (XbBuilder *self, XbBuilderNode *bn)
{
	XbBuilderPrivate *priv = GET_PRIVATE (self);
	g_autofree gchar *guid = g_strdup_printf ("bn@%p", bn);
	g_return_if_fail (XB_IS_BUILDER (self));
	g_return_if_fail (XB_IS_BUILDER_NODE (bn));
	g_ptr_array_add (priv->nodes, g_object_ref (bn));
	xb_builder_append_guid (self, guid);
}

/**
 * xb_builder_add_locale:
 * @self: a #XbSilo
 * @locale: a locale, e.g. "en_US"
 *
 * Adds a locale to the builder. Locales added first will be prioritised over
 * locales added later.
 *
 * Since: 0.1.0
 **/
void
xb_builder_add_locale (XbBuilder *self, const gchar *locale)
{
	XbBuilderPrivate *priv = GET_PRIVATE (self);

	g_return_if_fail (XB_IS_BUILDER (self));
	g_return_if_fail (locale != NULL);

	if (g_str_has_suffix (locale, ".UTF-8"))
		return;
	for (guint i = 0; i < priv->locales->len; i++) {
		const gchar *locale_tmp = g_ptr_array_index (priv->locales, i);
		if (g_strcmp0 (locale_tmp, locale) == 0)
			return;
	}
	g_ptr_array_add (priv->locales, g_strdup (locale));

	/* if the user changes LANG, the blob is no longer valid */
	xb_builder_append_guid (self, locale);
}

static gboolean
xb_builder_watch_source (XbBuilder *self,
			 XbBuilderSource *source,
			 GCancellable *cancellable,
			 GError **error)
{
	XbBuilderPrivate *priv = GET_PRIVATE (self);
	GFile *file = xb_builder_source_get_file (source);
	g_autoptr(GFile) watched_file = NULL;
	if (file == NULL)
		return TRUE;
	if ((xb_builder_source_get_flags (source) & (XB_BUILDER_SOURCE_FLAG_WATCH_FILE | XB_BUILDER_SOURCE_FLAG_WATCH_DIRECTORY)) == 0)
		return TRUE;

	if (xb_builder_source_get_flags (source) & XB_BUILDER_SOURCE_FLAG_WATCH_DIRECTORY)
		watched_file = g_file_get_parent (file);
	else
		watched_file = g_object_ref (file);

	if (!xb_silo_watch_file (priv->silo, watched_file, cancellable, error))
		return FALSE;
	return TRUE;
}

static gboolean
xb_builder_watch_sources (XbBuilder *self, GCancellable *cancellable, GError **error)
{
	XbBuilderPrivate *priv = GET_PRIVATE (self);
	for (guint i = 0; i < priv->sources->len; i++) {
		XbBuilderSource *source = g_ptr_array_index (priv->sources, i);
		if (!xb_builder_watch_source (self, source, cancellable, error))
			return FALSE;
	}
	return TRUE;
}

/**
 * xb_builder_compile:
 * @self: a #XbSilo
 * @flags: some #XbBuilderCompileFlags, e.g. %XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT
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
	XbBuilderPrivate *priv = GET_PRIVATE (self);
	guint32 nodetabsz = sizeof(XbSiloHeader);
	g_autoptr(GBytes) blob = NULL;
	g_autoptr(GString) buf = NULL;
	XbSiloHeader hdr = {
		.magic		= XB_SILO_MAGIC_BYTES,
		.version	= XB_SILO_VERSION,
		.strtab		= 0,
		.strtab_ntags	= 0,
		.padding	= { 0x0 },
		.guid		= { 0x0 },
	};
	XbBuilderNodetabHelper nodetab_helper = {
		.buf = NULL,
	};
	g_autoptr(GPtrArray) nodes_to_destroy = g_ptr_array_new ();
	g_autoptr(GTimer) timer = xb_silo_start_profile (priv->silo);
	g_autoptr(XbBuilderCompileHelper) helper = NULL;

	g_return_val_if_fail (XB_IS_BUILDER (self), NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* this is inferred */
	if (flags & XB_BUILDER_COMPILE_FLAG_SINGLE_LANG)
		flags |= XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS;

	/* the builder needs to know the locales */
	if (priv->locales->len == 0 && (flags & XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS)) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "No locales set and using NATIVE_LANGS");
		return NULL;
	}

	/* create helper used for compiling */
	helper = g_new0 (XbBuilderCompileHelper, 1);
	helper->compile_flags = flags;
	helper->root = xb_builder_node_new (NULL);
	helper->silo = priv->silo;
	helper->locales = priv->locales;
	helper->strtab = g_string_new (NULL);
	helper->strtab_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	/* build node tree */
	for (guint i = 0; i < priv->sources->len; i++) {
		XbBuilderSource *source = g_ptr_array_index (priv->sources, i);
		const gchar *prefix = xb_builder_source_get_prefix (source);
		g_autofree gchar *source_guid = xb_builder_source_get_guid (source);
		g_autoptr(XbBuilderNode) root = NULL;
		g_autoptr(GError) error_local = NULL;

		/* find, or create the prefix */
		if (prefix != NULL) {
			root = xb_builder_node_get_child (helper->root, prefix, NULL);
			if (root == NULL)
				root = xb_builder_node_insert (helper->root, prefix, NULL);
		} else {
			/* don't allow damaged XML files to ruin all the next ones */
			root = g_object_ref (helper->root);
		}

		if (priv->profile_flags & XB_SILO_PROFILE_FLAG_DEBUG)
			g_debug ("compiling %s…", source_guid);
		if (!xb_builder_compile_source (helper, source, root,
						cancellable, &error_local)) {
			if (flags & XB_BUILDER_COMPILE_FLAG_IGNORE_INVALID) {
				g_debug ("ignoring invalid file %s: %s",
					 source_guid,
					 error_local->message);
				continue;
			}
			g_propagate_prefixed_error (error,
						    g_steal_pointer (&error_local),
						    "failed to compile %s: ",
						    source_guid);
			return NULL;
		}

		/* watch the source */
		if (!xb_builder_watch_source (self, source, cancellable, error))
			return NULL;
	}

	/* run any node functions */
	for (guint i = 0; i < priv->fixups->len; i++) {
		XbBuilderFixup *fixup = g_ptr_array_index (priv->fixups, i);
		if (!xb_builder_fixup_node (fixup, helper->root, error))
			return NULL;
	}

	/* only include the highest priority translation */
	if (flags & XB_BUILDER_COMPILE_FLAG_SINGLE_LANG) {
		xb_builder_node_traverse (helper->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
					  xb_builder_xml_lang_prio_cb, nodes_to_destroy);
		for (guint i = 0; i < nodes_to_destroy->len; i++) {
			XbBuilderNode *bn = g_ptr_array_index (nodes_to_destroy, i);
			xb_builder_node_unlink (bn);
		}
		xb_silo_add_profile (priv->silo, timer, "filter single-lang");
	}

	/* add any manually build nodes */
	for (guint i = 0; i < priv->nodes->len; i++) {
		XbBuilderNode *bn = g_ptr_array_index (priv->nodes, i);
		xb_builder_node_add_child (helper->root, bn);
	}

	/* get the size of the nodetab */
	xb_builder_node_traverse (helper->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				  xb_builder_nodetab_size_cb, &nodetabsz);
	buf = g_string_sized_new (nodetabsz);
	xb_silo_add_profile (priv->silo, timer, "get size nodetab");

	/* add everything to the strtab */
	xb_builder_node_traverse (helper->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				  xb_builder_strtab_element_names_cb, helper);
	hdr.strtab_ntags = g_hash_table_size (helper->strtab_hash);
	xb_silo_add_profile (priv->silo, timer, "adding strtab element");
	xb_builder_node_traverse (helper->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				  xb_builder_strtab_attr_name_cb, helper);
	xb_silo_add_profile (priv->silo, timer, "adding strtab attr name");
	xb_builder_node_traverse (helper->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				  xb_builder_strtab_attr_value_cb, helper);
	xb_silo_add_profile (priv->silo, timer, "adding strtab attr value");
	xb_builder_node_traverse (helper->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				  xb_builder_strtab_text_cb, helper);
	xb_silo_add_profile (priv->silo, timer, "adding strtab text");
	xb_builder_node_traverse (helper->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				  xb_builder_strtab_tokens_cb, helper);
	xb_silo_add_profile (priv->silo, timer, "adding strtab tokens");

	/* add the initial header */
	hdr.strtab = nodetabsz;
	if (priv->guid->len > 0) {
		XbGuid guid_tmp;
		xb_guid_compute_for_data (&guid_tmp,
					  (const guint8 *) priv->guid->str,
					  priv->guid->len);
		memcpy (&hdr.guid, &guid_tmp, sizeof(guid_tmp));
	}
	XB_SILO_APPENDBUF (buf, &hdr, sizeof(XbSiloHeader));

	/* write nodes to the nodetab */
	nodetab_helper.buf = buf;
	xb_builder_nodetab_write (&nodetab_helper, helper->root);
	xb_silo_add_profile (priv->silo, timer, "writing nodetab");

	/* set all the ->next and ->parent offsets */
	xb_builder_node_traverse (helper->root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				  xb_builder_nodetab_fix_cb, &nodetab_helper);
	xb_silo_add_profile (priv->silo, timer, "fixing ->parent and ->next");

	/* append the string table */
	XB_SILO_APPENDBUF (buf, helper->strtab->str, helper->strtab->len);
	xb_silo_add_profile (priv->silo, timer, "appending strtab");

	/* create data */
	blob = g_bytes_new (buf->str, buf->len);
	if (!xb_silo_load_from_bytes (priv->silo, blob, XB_SILO_LOAD_FLAG_NONE, error))
		return NULL;

	/* success */
	return g_object_ref (priv->silo);
}

/**
 * xb_builder_ensure:
 * @self: a #XbSilo
 * @file: a #GFile
 * @flags: some #XbBuilderCompileFlags, e.g. %XB_BUILDER_COMPILE_FLAG_IGNORE_INVALID
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
	XbBuilderPrivate *priv = GET_PRIVATE (self);
	XbSiloLoadFlags load_flags = XB_SILO_LOAD_FLAG_NONE;
	g_autofree gchar *fn = NULL;
	g_autoptr(XbSilo) silo_tmp = xb_silo_new ();
	g_autoptr(XbSilo) silo_new = NULL;
	g_autoptr(GError) error_local = NULL;

	g_return_val_if_fail (XB_IS_BUILDER (self), NULL);
	g_return_val_if_fail (G_IS_FILE (file), NULL);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* watch the blob, so propagate flags */
	if (flags & XB_BUILDER_COMPILE_FLAG_WATCH_BLOB)
		load_flags |= XB_SILO_LOAD_FLAG_WATCH_BLOB;

	/* profile new silo if needed */
	xb_silo_set_profile_flags (silo_tmp, priv->profile_flags);

	/* load the file and peek at the GUIDs */
	fn = g_file_get_path (file);
	g_debug ("attempting to load %s", fn);
	if (!xb_silo_load_from_file (silo_tmp, file,
				     XB_SILO_LOAD_FLAG_NONE,
				     cancellable,
				     &error_local)) {
		g_debug ("failed to load silo: %s", error_local->message);
	} else {
		g_autofree gchar *guid = xb_builder_generate_guid (self);
		g_debug ("file: %s, current:%s, cached: %s",
			 xb_silo_get_guid (silo_tmp), guid,
			 xb_silo_get_guid (priv->silo));

		/* GUIDs match exactly with the thing that's already loaded */
		if (g_strcmp0 (xb_silo_get_guid (silo_tmp),
			       xb_silo_get_guid (priv->silo)) == 0) {
			g_debug ("returning unchanged silo");
			xb_silo_uninvalidate (priv->silo);
			return g_object_ref (priv->silo);
		}

		/* reload the cached silo with the new file data */
		if (g_strcmp0 (xb_silo_get_guid (silo_tmp), guid) == 0 ||
		    (flags & XB_BUILDER_COMPILE_FLAG_IGNORE_GUID) > 0) {
			g_autoptr(GBytes) blob = xb_silo_get_bytes (silo_tmp);

			g_debug ("loading silo with file contents");
			if (!xb_silo_load_from_bytes (priv->silo, blob,
						      load_flags, error))
				return NULL;

			/* ensure all the sources are watched */
			if (!xb_builder_watch_sources (self, cancellable, error))
				return NULL;

			/* ensure backing file is watched for changes */
			if (flags & XB_BUILDER_COMPILE_FLAG_WATCH_BLOB) {
				if (!xb_silo_watch_file (priv->silo, file,
							 cancellable, error))
					return NULL;
			}
			return g_object_ref (priv->silo);
		}
	}

	/* fallback to just creating a new file */
	silo_new = xb_builder_compile (self, flags, cancellable, error);
	if (silo_new == NULL)
		return NULL;
	if (!xb_silo_save_to_file (silo_new, file, NULL, error))
		return NULL;

	/* load from a file to re-mmap it */
	if (!xb_silo_load_from_file (priv->silo, file, load_flags, cancellable, error))
		return NULL;

	/* ensure all the sources are watched on the reloaded silo */
	if (!xb_builder_watch_sources (self, cancellable, error))
		return NULL;

	/* success */
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
	XbBuilderPrivate *priv = GET_PRIVATE (self);
	if (priv->guid->len > 0)
		g_string_append (priv->guid, "&");
	g_string_append (priv->guid, guid);
}

/**
 * xb_builder_set_profile_flags:
 * @self: a #XbBuilder
 * @profile_flags: some #XbSiloProfileFlags, e.g. %XB_SILO_PROFILE_FLAG_DEBUG
 *
 * Enables or disables the collection of profiling data.
 *
 * Since: 0.1.1
 **/
void
xb_builder_set_profile_flags (XbBuilder *self, XbSiloProfileFlags profile_flags)
{
	XbBuilderPrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_BUILDER (self));
	priv->profile_flags = profile_flags;
	xb_silo_set_profile_flags (priv->silo, profile_flags);
}

/**
 * xb_builder_add_fixup:
 * @self: a #XbBuilder
 * @fixup: a #XbBuilderFixup
 *
 * Adds a function that will get run on every #XbBuilderNode compile creates
 * for the silo. This is run after all the #XbBuilderSource fixups have been
 * run.
 *
 * Since: 0.1.3
 **/
void
xb_builder_add_fixup (XbBuilder *self, XbBuilderFixup *fixup)
{
	XbBuilderPrivate *priv = GET_PRIVATE (self);
	g_autofree gchar *guid = NULL;

	g_return_if_fail (XB_IS_BUILDER (self));
	g_return_if_fail (XB_IS_BUILDER_FIXUP (fixup));

	/* append function IDs */
	guid = xb_builder_fixup_get_guid (fixup);
	xb_builder_append_guid (self, guid);
	g_ptr_array_add (priv->fixups, g_object_ref (fixup));
}

static void
xb_builder_finalize (GObject *obj)
{
	XbBuilder *self = XB_BUILDER (obj);
	XbBuilderPrivate *priv = GET_PRIVATE (self);

	g_ptr_array_unref (priv->sources);
	g_ptr_array_unref (priv->nodes);
	g_ptr_array_unref (priv->locales);
	g_ptr_array_unref (priv->fixups);
	g_object_unref (priv->silo);
	g_string_free (priv->guid, TRUE);

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
	XbBuilderPrivate *priv = GET_PRIVATE (self);
	priv->sources = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->nodes = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->fixups = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	priv->locales = g_ptr_array_new_with_free_func (g_free);
	priv->silo = xb_silo_new ();
	priv->guid = g_string_new (NULL);
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
