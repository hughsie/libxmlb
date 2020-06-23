/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <string.h>
#include <gio/gio.h>

#include "xb-node-private.h"
#include "xb-opcode.h"
#include "xb-silo-private.h"
#include "xb-silo-query-private.h"
#include "xb-stack-private.h"
#include "xb-query-private.h"

static gboolean
xb_silo_query_node_matches (XbSilo *self,
			    XbMachine *machine,
			    XbSiloNode *sn,
			    XbQuerySection *section,
			    XbSiloQueryData *query_data,
			    gboolean *result,
			    GError **error)
{
	/* we have an index into the string table */
	if (section->element_idx != sn->element_name &&
	    section->kind != XB_SILO_QUERY_KIND_WILDCARD) {
		*result = FALSE;
		return TRUE;
	}

	/* for section */
	query_data->position += 1;

	/* check predicates */
	if (section->predicates != NULL) {
		for (guint i = 0; i < section->predicates->len; i++) {
			XbStack *opcodes = g_ptr_array_index (section->predicates, i);
			if (!xb_machine_run (machine, opcodes, result, query_data, error))
				return FALSE;
		}
	}

	/* success */
	return TRUE;
}

/**
 * XbSiloQueryHelperFlags:
 * @XB_SILO_QUERY_HELPER_NONE: No flags set.
 * @XB_SILO_QUERY_HELPER_USE_SN: Return #XbSiloNodes as results, rather than
 *    wrapping them in #XbNode. This assumes that they’ll be wrapped later.
 * @XB_SILO_QUERY_HELPER_FORCE_NODE_CACHE: Always cache the #XbNode objects
 *
 * Flags for #XbSiloQueryHelper.
 *
 * Since: 0.2.0
 */
typedef enum {
	XB_SILO_QUERY_HELPER_NONE		= 0,
	XB_SILO_QUERY_HELPER_USE_SN		= 1 << 0,
	XB_SILO_QUERY_HELPER_FORCE_NODE_CACHE	= 1 << 1,
} XbSiloQueryHelperFlags;

typedef struct {
	GPtrArray	*sections;	/* of XbQuerySection */
	GPtrArray	*results;	/* of XbNode or XbSiloNode (see @flags) */
	GHashTable	*results_hash;	/* of sn:1 */
	guint		 limit;
	XbSiloQueryHelperFlags	 flags;
	XbSiloQueryData	*query_data;
} XbSiloQueryHelper;

static gboolean
xb_silo_query_section_add_result (XbSilo *self, XbSiloQueryHelper *helper, XbSiloNode *sn)
{
	if (g_hash_table_lookup (helper->results_hash, sn) != NULL)
		return FALSE;
	if (helper->flags & XB_SILO_QUERY_HELPER_USE_SN) {
		g_ptr_array_add (helper->results, sn);
	} else {
		gboolean force_node_cache = (helper->flags & XB_SILO_QUERY_HELPER_FORCE_NODE_CACHE) > 0;
		g_ptr_array_add (helper->results,
				 xb_silo_node_create (self, sn, force_node_cache));
	}
	g_hash_table_add (helper->results_hash, sn);
	return helper->results->len == helper->limit;
}

/*
 * @parent: (allow-none)
 */
static gboolean
xb_silo_query_section_root (XbSilo *self,
			    XbSiloNode *sn,
			    guint i,
			    XbSiloQueryHelper *helper,
			    GError **error)
{
	XbMachine *machine = xb_silo_get_machine (self);
	XbSiloQueryData *query_data = helper->query_data;
	XbQuerySection *section = g_ptr_array_index (helper->sections, i);

	/* handle parent */
	if (section->kind == XB_SILO_QUERY_KIND_PARENT) {
		XbSiloNode *parent;
		if (sn == NULL) {
			g_set_error_literal (error,
					     G_IO_ERROR,
					     G_IO_ERROR_INVALID_ARGUMENT,
					     "cannot obtain parent for root");
			return FALSE;
		}
		parent = xb_silo_node_get_parent (self, sn);
		if (parent == NULL) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_ARGUMENT,
				     "no parent set for %s",
				     xb_silo_node_get_element (self, sn));
			return FALSE;
		}
		if (i == helper->sections->len - 1) {
			xb_silo_query_section_add_result (self, helper, parent);
			return TRUE;
		}
//		g_debug ("PARENT @%u",
//			 xb_silo_get_offset_for_node (self, parent));
		return xb_silo_query_section_root (self, parent, i + 1, helper, error);
	}

	/* no node means root */
	if (sn == NULL) {
		sn = xb_silo_get_sroot (self);
		if (sn == NULL) {
			g_set_error_literal (error,
					     G_IO_ERROR,
					     G_IO_ERROR_NOT_FOUND,
					     "silo root not found");
			return FALSE;
		}
	} else {
		sn = xb_silo_node_get_child (self, sn);
		if (sn == NULL)
			return TRUE;
	}

	/* set up level pointer */
	query_data->position = 0;

	/* continue matching children ".." */
	do {
		gboolean result = TRUE;
		query_data->sn = sn;
		if (!xb_silo_query_node_matches (self, machine, sn, section,
						 query_data, &result, error))
			return FALSE;
		if (result) {
			if (i == helper->sections->len - 1) {
//				g_debug ("add result %u",
//					 xb_silo_get_offset_for_node (self, sn));
				if (xb_silo_query_section_add_result (self, helper, sn))
					break;
			} else {
//				g_debug ("MATCH %s at @%u, deeper",
//					 xb_silo_node_get_element (self, sn),
//					 xb_silo_get_offset_for_node (self, sn));
				if (!xb_silo_query_section_root (self, sn, i + 1, helper, error))
					return FALSE;
				if (helper->results->len > 0 &&
				    helper->results->len == helper->limit)
					break;
			}
		}
		if (sn->next == 0x0)
			break;
		sn = xb_silo_get_node (self, sn->next);
	} while (TRUE);
	return TRUE;
}

static gboolean
xb_silo_query_part (XbSilo *self,
		    XbSiloNode *sroot,
		    GPtrArray *results,
		    GHashTable *results_hash,
		    XbQuery *query,
		    XbSiloQueryData *query_data,
		    XbSiloQueryHelperFlags flags,
		    GError **error)
{
	XbSiloQueryHelper helper = {
		.results = results,
		.limit = xb_query_get_limit (query),
		.flags = flags,
		.results_hash = results_hash,
		.query_data = query_data,
	};

	/* find each section */
	helper.sections = xb_query_get_sections (query);
	if (xb_query_get_flags (query) & XB_QUERY_FLAG_FORCE_NODE_CACHE)
		helper.flags |= XB_SILO_QUERY_HELPER_FORCE_NODE_CACHE;
	return xb_silo_query_section_root (self, sroot, 0, &helper, error);
}

/* Returns an array with (element-type XbSiloNode) if
 * %XB_SILO_QUERY_HELPER_USE_SN is set, and (element-type XbNode) otherwise. */
static GPtrArray *
silo_query_with_root (XbSilo *self, XbNode *n, const gchar *xpath, guint limit, XbSiloQueryHelperFlags flags, GError **error)
{
	XbSiloNode *sn = NULL;
	g_auto(GStrv) split = NULL;
	g_autoptr(GHashTable) results_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(GTimer) timer = xb_silo_start_profile (self);
	XbSiloQueryData query_data = {
		.sn = NULL,
		.position = 0,
	};

	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	g_return_val_if_fail (xpath != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* empty silo */
	if (xb_silo_is_empty (self)) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_FOUND,
				     "silo has no data");
		return NULL;
	}

	if (flags & XB_SILO_QUERY_HELPER_USE_SN)
		results = g_ptr_array_new_with_free_func (NULL);
	else
		results = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* subtree query */
	if (n != NULL) {
		sn = xb_node_get_sn (n);
		if (xpath[0] == '/') {
			g_set_error_literal (error,
					     G_IO_ERROR,
					     G_IO_ERROR_NOT_SUPPORTED,
					     "XPath node query not supported");
			return NULL;
		}
	} else {
		/* assume it's just a root query */
		if (xpath[0] == '/')
			xpath++;
	}

	/* do 'or' searches */
	split = g_strsplit (xpath, "|", -1);
	for (guint i = 0; split[i] != NULL; i++) {
		g_autoptr(GError) error_local = NULL;
		g_autoptr(XbQuery) query = xb_query_new (self, split[i], &error_local);
		if (query == NULL) {
			if (g_error_matches (error_local, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT) &&
			    (split[i + 1] != NULL || results->len > 0)) {
				g_debug ("ignoring for OR statement: %s",
					 error_local->message);
				continue;
			}
			g_propagate_prefixed_error (error,
						    g_steal_pointer (&error_local),
						    "failed to process %s: ",
						    xpath);
			return NULL;
		}
		xb_query_set_limit (query, limit);
		if (!xb_silo_query_part (self, sn,
					 results, results_hash,
					 query, &query_data,
					 flags,
					 error)) {
			return NULL;
		}
	}

	/* profile */
	if (xb_silo_get_profile_flags (self) & XB_SILO_PROFILE_FLAG_XPATH) {
		xb_silo_add_profile (self, timer,
				     "query on %s with `%s` limit=%u -> %u results",
				     n != NULL ? xb_node_get_element (n) : "/",
				     xpath, limit, results->len);
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
 * xb_silo_query_with_root: (skip)
 * @self: a #XbSilo
 * @n: (allow-none): a #XbNode
 * @xpath: an XPath, e.g. `/components/component[@type=desktop]/id[abe.desktop]`
 * @limit: maximum number of results to return, or 0 for "all"
 * @error: the #GError, or %NULL
 *
 * Searches the silo using an XPath query, returning up to @limit results.
 *
 * It is safe to call this function from a different thread to the one that
 * created the #XbSilo.
 *
 * Please note: Only a subset of XPath is supported.
 *
 * Returns: (transfer container) (element-type XbNode): results, or %NULL if unfound
 *
 * Since: 0.1.0
 **/
GPtrArray *
xb_silo_query_with_root (XbSilo *self, XbNode *n, const gchar *xpath, guint limit, GError **error)
{
	return silo_query_with_root (self, n, xpath, limit, XB_SILO_QUERY_HELPER_NONE, error);
}

/**
 * xb_silo_query_sn_with_root: (skip)
 * @self: a #XbSilo
 * @n: (allow-none): a #XbNode
 * @xpath: an XPath, e.g. `/components/component[@type=desktop]/id[abe.desktop]`
 * @limit: maximum number of results to return, or 0 for "all"
 * @error: the #GError, or %NULL
 *
 * A version of xb_silo_query_with_root() which returns results as #XbSiloNodes,
 * rather than as #XbNodes. This is intended to be used internally to save on
 * intermediate #XbNode allocations.
 *
 * Returns: (transfer container) (element-type XbSiloNode): results, or %NULL if unfound
 *
 * Since: 0.2.0
 **/
GPtrArray *
xb_silo_query_sn_with_root (XbSilo *self, XbNode *n, const gchar *xpath, guint limit, GError **error)
{
	return silo_query_with_root (self, n, xpath, limit, XB_SILO_QUERY_HELPER_USE_SN, error);
}

static void
_g_ptr_array_reverse (GPtrArray *array)
{
	guint last_idx = array->len - 1;
	for (guint i = 0; i < array->len / 2; i++) {
		gpointer tmp = array->pdata[i];
		array->pdata[i] = array->pdata[last_idx - i];
		array->pdata[last_idx - i] = tmp;
	}
}

/**
 * xb_silo_query_with_root_full: (skip)
 * @self: a #XbSilo
 * @n: (allow-none): a #XbNode
 * @query: an #XbQuery
 * @error: the #GError, or %NULL
 *
 * Searches the silo using an XPath query, returning up to @limit results.
 *
 * It is safe to call this function from a different thread to the one that
 * created the #XbSilo.
 *
 * Please note: Only a subset of XPath is supported.
 *
 * Returns: (transfer container) (element-type XbNode): results, or %NULL if unfound
 *
 * Since: 0.1.4
 **/
GPtrArray *
xb_silo_query_with_root_full (XbSilo *self, XbNode *n, XbQuery *query, GError **error)
{
	XbSiloNode *sn = NULL;
	g_autoptr(GHashTable) results_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	g_autoptr(GPtrArray) results = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	g_autoptr(GTimer) timer = xb_silo_start_profile (self);
	XbSiloQueryData query_data = {
		.sn = NULL,
		.position = 0,
	};

	/* empty silo */
	if (xb_silo_is_empty (self)) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_FOUND,
				     "silo has no data");
		return NULL;
	}

	/* subtree query */
	if (n != NULL)
		sn = xb_node_get_sn (n);

	/* only one query allowed */
	if (!xb_silo_query_part (self, sn, results, results_hash,
				 query, &query_data,
				 XB_SILO_QUERY_HELPER_NONE, error))
		return NULL;

	/* profile */
	if (xb_silo_get_profile_flags (self) & XB_SILO_PROFILE_FLAG_XPATH) {
		g_autofree gchar *tmp = xb_query_to_string (query);
		xb_silo_add_profile (self, timer,
				     "query on %s with `%s` limit=%u -> %u results",
				     n != NULL ? xb_node_get_element (n) : "/",
				     tmp,
				     xb_query_get_limit (query),
				     results->len);
	}

	/* nothing found */
	if (results->len == 0) {
		g_autofree gchar *tmp = xb_query_to_string (query);
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_FOUND,
			     "no results for XPath query '%s'",
			     tmp);
		return NULL;
	}

	/* reverse order */
	if (xb_query_get_flags (query) & XB_QUERY_FLAG_REVERSE)
		_g_ptr_array_reverse (results);

	return g_steal_pointer (&results);
}

/**
 * xb_silo_query_full:
 * @self: a #XbSilo
 * @query: an #XbQuery
 * @error: the #GError, or %NULL
 *
 * Searches the silo using an XPath query.
 *
 * It is safe to call this function from a different thread to the one that
 * created the #XbSilo.
 *
 * Please note: Only a subset of XPath is supported.
 *
 * Returns: (transfer container) (element-type XbNode): results, or %NULL if unfound
 *
 * Since: 0.1.13
 **/
GPtrArray *
xb_silo_query_full (XbSilo *self, XbQuery *query, GError **error)
{
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	g_return_val_if_fail (XB_IS_QUERY (query), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	return xb_silo_query_with_root_full (self, NULL, query, error);
}

/**
 * xb_silo_query_first_full:
 * @self: a #XbSilo
 * @query: an #XbQuery
 * @error: the #GError, or %NULL
 *
 * Searches the silo using an XPath query, returning up to one result.
 *
 * It is safe to call this function from a different thread to the one that
 * created the #XbSilo.
 *
 * Please note: Only a tiny subset of XPath 1.0 is supported.
 *
 * Returns: (transfer none): a #XbNode, or %NULL if unfound
 *
 * Since: 0.1.13
 **/
XbNode *
xb_silo_query_first_full (XbSilo *self, XbQuery *query, GError **error)
{
	g_autoptr(GPtrArray) results = NULL;
	results = xb_silo_query_full (self, query, error);
	if (results == NULL)
		return NULL;
	return g_object_ref (g_ptr_array_index (results, 0));
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
 * It is safe to call this function from a different thread to the one that
 * created the #XbSilo.
 *
 * Please note: Only a subset of XPath is supported.
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
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
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
 * It is safe to call this function from a different thread to the one that
 * created the #XbSilo.
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

/**
 * xb_silo_query_build_index:
 * @self: a #XbSilo
 * @xpath: An XPath, e.g. `/components/component[@type=desktop]/id[abe.desktop]`
 * @attr: (nullable): Attribute name, e.g. `type`, or NULL
 * @error: the #GError, or %NULL
 *
 * Adds the `attr()` or `text()` results of a query to the index.
 *
 * Returns: %TRUE for success
 *
 * Since: 0.1.4
 **/
gboolean
xb_silo_query_build_index (XbSilo *self,
			   const gchar *xpath,
			   const gchar *attr,
			   GError **error)
{
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GPtrArray) array = NULL;

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (xpath != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* do the query */
	array = silo_query_with_root (self, NULL, xpath, 0,
				      XB_SILO_QUERY_HELPER_USE_SN,
				      &error_local);
	if (array == NULL) {
		if (g_error_matches (error_local,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_ARGUMENT) ||
		    g_error_matches (error_local,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_FOUND)) {
			g_debug ("ignoring index: %s", error_local->message);
			return TRUE;
		}
		g_propagate_error (error, g_steal_pointer (&error_local));
		return FALSE;
	}

	/* add each attribute name AND value */
	for (guint i = 0; i < array->len; i++) {
		XbSiloNode *sn = g_ptr_array_index (array, i);
		if (attr != NULL) {
			guint32 off = xb_silo_get_offset_for_node (self, sn);
			for (guint8 j = 0; j < sn->nr_attrs; j++) {
				XbSiloAttr *a = xb_silo_get_attr (self, off, j);
				xb_silo_strtab_index_insert (self, a->attr_name);
				xb_silo_strtab_index_insert (self, a->attr_value);
			}
		} else {
			xb_silo_strtab_index_insert (self, sn->text);
		}
	}

	/* success */
	return TRUE;
}
