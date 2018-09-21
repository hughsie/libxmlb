/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <gio/gio.h>

#include "xb-node-private.h"
#include "xb-silo-private.h"
#include "xb-silo-query-private.h"

static gboolean
xb_silo_query_check_predicate_attr (XbSilo *self,
				    XbSiloNode *sn,
				    const gchar *key,
				    const gchar *val)
{
	return g_strcmp0 (xb_silo_node_get_attr (self, sn, key), val) == 0;
}

static gboolean
xb_silo_query_check_predicate_text (XbSilo *self, XbSiloNode *sn, const gchar *val)
{
	return g_strcmp0 (xb_silo_node_get_text (self, sn), val) == 0;
}

static gboolean
xb_silo_query_check_predicate (XbSilo *self, XbSiloNode *sn, const gchar *predicate)
{
	if (g_str_has_prefix (predicate, "@")) {
		g_auto(GStrv) split = g_strsplit (predicate + 1, "=", -1);
		if (g_strv_length (split) != 2) {
			g_warning ("failed to parse predicate %s", predicate);
			return FALSE;
		}
		return xb_silo_query_check_predicate_attr (self, sn, split[0], split[1]);
	}
	return xb_silo_query_check_predicate_text (self, sn, predicate);
}

static gboolean
xb_silo_query_check_predicates (XbSilo *self, XbSiloNode *sn, GPtrArray *predicates)
{
	for (guint i = 0; i < predicates->len; i++) {
		const gchar *predicate = g_ptr_array_index (predicates, i);
		if (!xb_silo_query_check_predicate (self, sn, predicate))
			return FALSE;
	}
	return TRUE;
}

typedef enum {
	XB_SILO_QUERY_KIND_UNKNOWN,
	XB_SILO_QUERY_KIND_WILDCARD,
	XB_SILO_QUERY_KIND_PARENT,
	XB_SILO_QUERY_KIND_LAST
} XbSiloQueryKind;

typedef struct {
	gchar		*element;
	guint32		 element_idx;
	GPtrArray	*predicates;
	gboolean	 is_wildcard;
	gboolean	 is_parent;
	XbSiloQueryKind	 kind;
} XbSiloQuerySection;

static void
xb_silo_query_section_free (XbSiloQuerySection *section)
{
	if (section->predicates != NULL)
		g_ptr_array_unref (section->predicates);
	g_free (section->element);
	g_slice_free (XbSiloQuerySection, section);
}

static XbSiloQuerySection *
xb_silo_query_parse_section (XbSilo *self, const gchar *xpath, GError **error)
{
	XbSiloQuerySection *section;
	guint start = 0;

	section = g_slice_new0 (XbSiloQuerySection);

	/* common XPath parts */
	if (g_strcmp0 (xpath, "parent::") == 0 ||
	    g_strcmp0 (xpath, "..") == 0) {
		section->kind = XB_SILO_QUERY_KIND_PARENT;
		return section;
	}
	if (g_strcmp0 (xpath, "*") == 0) {
		section->kind = XB_SILO_QUERY_KIND_WILDCARD;
		return section;
	}

	/* parse element and predicate */
	for (guint i = 0; xpath[i] != '\0'; i++) {
		if (start == 0 && xpath[i] == '[') {
			if (section->element == NULL)
				section->element = g_strndup (xpath, i);
			start = i;
			continue;
		}
		if (start > 0 && xpath[i] == ']') {
			if (section->predicates == NULL)
				section->predicates = g_ptr_array_new_with_free_func (g_free);
			g_ptr_array_add (section->predicates,
					 g_strndup (xpath + start + 1,
						    i - start - 1));
			start = 0;
			continue;
		}
	}
	if (section->element == NULL)
		section->element = g_strdup (xpath);
	section->element_idx = xb_silo_get_strtab_idx (self, section->element);
	if (section->element_idx == XB_SILO_UNSET) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_FOUND,
			     "element name %s is unknown in silo",
			     section->element);
		return NULL;
	}
	return section;
}

static GPtrArray *
xb_silo_query_parse_sections (XbSilo *self, const gchar *xpath, GError **error)
{
	g_autoptr(GPtrArray) sections = NULL;
	g_auto(GStrv) split = NULL;

//	g_debug ("parsing XPath %s", xpath);
	sections = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_silo_query_section_free);
	split = g_strsplit (xpath, "/", -1);
	for (guint i = 0; split[i] != NULL; i++) {
		XbSiloQuerySection *section = xb_silo_query_parse_section (self, split[i], error);
		if (section == NULL)
			return NULL;
		g_ptr_array_add (sections, section);
	}
	return g_steal_pointer (&sections);
}

static gboolean
xb_silo_query_node_matches (XbSilo *self, XbSiloNode *sn, XbSiloQuerySection *section)
{
	/* wildcard */
	if (section->kind == XB_SILO_QUERY_KIND_WILDCARD)
		return TRUE;

	/* we have an index into the string table */
	if (section->element_idx != sn->element_name)
		return FALSE;

	/* check predicates */
	if (section->predicates != NULL)
		return xb_silo_query_check_predicates (self, sn, section->predicates);

	/* success */
	return TRUE;
}

typedef struct {
	GPtrArray	*sections;
	GPtrArray	*results;
	XbSiloNode	*root;
	guint		 limit;
} XbSiloQueryHelper;

static gboolean
xb_silo_query_section_add_result (XbSilo *self, XbSiloQueryHelper *helper, XbSiloNode *sn)
{
	g_ptr_array_add (helper->results, xb_silo_node_create (self, sn));
	return helper->results->len == helper->limit;
}

static gboolean
xb_silo_query_section_root (XbSilo *self, XbSiloNode *sn, guint i, XbSiloQueryHelper *helper, GError **error)
{
	XbSiloQuerySection *section = g_ptr_array_index (helper->sections, i);

	/* handle parent */
	if (section->kind == XB_SILO_QUERY_KIND_PARENT) {
		XbSiloNode *parent = xb_silo_node_get_parent (self, helper->root);
		if (parent == NULL) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_ARGUMENT,
				     "no parent for %s",
				     xb_silo_node_get_element (self, sn));
			return FALSE;
		}
		if (i == helper->sections->len - 1) {
			xb_silo_query_section_add_result (self, helper, parent);
			return TRUE;
		}
		helper->root = parent;
		return xb_silo_query_section_root (self, parent, i + 1, helper, error);
	}

	/* no child to process */
	if (sn == NULL) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "internal corruption when processing %s",
			     section->element);
		return FALSE;
	}

	/* save the parent so we can support ".." */
	helper->root = sn;
	do {
		if (xb_silo_query_node_matches (self, sn, section)) {
			if (i == helper->sections->len - 1) {
//				g_debug ("add result %u",
//					 xb_silo_get_offset_for_node (self, sn));
				if (xb_silo_query_section_add_result (self, helper, sn))
					break;
			} else {
				XbSiloNode *c = xb_silo_node_get_child (self, sn);
//				g_debug ("MATCH @%u, deeper",
//					 xb_silo_get_offset_for_node (self, sn));
				if (!xb_silo_query_section_root (self, c, i + 1, helper, error))
					return FALSE;
				if (helper->results->len > 0 &&
				    helper->results->len == helper->limit)
					break;
			}
		}
		sn = xb_silo_node_get_next (self, sn);
	} while (sn != NULL);
	return TRUE;
}

static gboolean
xb_silo_query_part (XbSilo *self,
		    XbSiloNode *sroot,
		    GPtrArray *results,
		    const gchar *xpath,
		    guint limit,
		    GError **error)
{
	g_autoptr(GPtrArray) sections = NULL;
	XbSiloQueryHelper helper = {
		.results = results,
		.limit = limit,
	};

	/* handle each section */
	sections = xb_silo_query_parse_sections (self, xpath, error);
	if (sections == NULL)
		return FALSE;

	/* find each section */
	helper.sections = sections;
	return xb_silo_query_section_root (self, sroot, 0, &helper, error);
}

/**
 * xb_silo_query_with_root: (skip)
 * @self: a #XbSilo
 * @n: a #XbNode
 * @xpath: an XPath, e.g. `/components/component[@type=desktop]/id[abe.desktop]`
 * @limit: maximum number of results to return, or 0 for "all"
 * @error: the #GError, or %NULL
 *
 * Searches the silo using an XPath query, returning up to @limit results.
 *
 * Important note: Only a tiny subset of XPath 1.0 is supported.
 *
 * Returns: (transfer container) (element-type XbNode): results, or %NULL if unfound
 *
 * Since: 0.1.0
 **/
GPtrArray *
xb_silo_query_with_root (XbSilo *self, XbNode *n, const gchar *xpath, guint limit, GError **error)
{
	XbSiloNode *sn;
	g_auto(GStrv) split = NULL;
	g_autoptr(GPtrArray) results = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	g_return_val_if_fail (xpath != NULL, NULL);

	/* invalid */
	if (xpath[0] == '/') {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "XPath root query not supported");
		return NULL;
	}

	/* subtree query */
	sn = n != NULL ? xb_node_get_sn (n) :xb_silo_get_sroot (self);

	/* no root */
	if (sn == NULL) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_FOUND,
				     "no data to query");
		return NULL;
	}

	/* do 'or' searches */
	split = g_strsplit (xpath, "|", -1);
	for (guint i = 0; split[i] != NULL; i++) {
		if (!xb_silo_query_part (self, sn, results, split[i], limit, error))
			return NULL;
	}

	/* nothing found */
	if (results->len == 0) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_FOUND,
			     "no results for XPath query '%s'",
			     xpath);
		return NULL;
	}

	return g_steal_pointer (&results);
}

/**
 * xb_silo_query:
 * @self: a #XbSilo
 * @xpath: an XPath, e.g. `/components/component[@type=desktop]/id[abe.desktop]`
 * @limit: maximum number of results to return, or 0 for "all"
 * @error: the #GError, or %NULL
 *
 * Searches the silo using an XPath query, returning up to @limit results.
 *
 * Important note: Only a tiny subset of XPath 1.0 is supported.
 *
 * Returns: (transfer container) (element-type XbNode): results, or %NULL if unfound
 *
 * Since: 0.1.0
 **/
GPtrArray *
xb_silo_query (XbSilo *self, const gchar *xpath, guint limit, GError **error)
{
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	g_return_val_if_fail (xpath != NULL, NULL);
	return xb_silo_query_with_root (self, NULL, xpath, limit, error);
}

/**
 * xb_silo_query_first:
 * @self: a #XbSilo
 * @xpath: An XPath, e.g. `/components/component[@type=desktop]/id[abe.desktop]`
 * @error: the #GError, or %NULL
 *
 * Searches the silo using an XPath query, returning up to one result.
 *
 * Please note: Only a tiny subset of XPath 1.0 is supported.
 *
 * Returns: (transfer none): a #XbNode, or %NULL if unfound
 *
 * Since: 0.1.0
 **/
XbNode *
xb_silo_query_first (XbSilo *self, const gchar *xpath, GError **error)
{
	g_autoptr(GPtrArray) results = NULL;
	results = xb_silo_query_with_root (self, NULL, xpath, 1, error);
	if (results == NULL)
		return NULL;
	return g_object_ref (g_ptr_array_index (results, 0));
}
