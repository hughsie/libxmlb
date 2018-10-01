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

static gboolean
xb_silo_query_parse_predicate (XbSilo *self,
			       XbSiloQuerySection *section,
			       const gchar *text,
			       gssize text_len,
			       GError **error)
{
	GPtrArray *opcodes;

	/* parse */
	opcodes = xb_machine_parse (xb_silo_get_machine (self), text, text_len, error);
	if (opcodes == NULL)
		return FALSE;

	/* create array if it does not exist */
	if (section->predicates == NULL)
		section->predicates = g_ptr_array_new_with_free_func ((GDestroyNotify) g_ptr_array_unref);
	g_ptr_array_add (section->predicates, opcodes);
	return TRUE;
}

static XbSiloQuerySection *
xb_silo_query_parse_section (XbSilo *self, const gchar *xpath, GError **error)
{
	XbSiloQuerySection *section = g_slice_new0 (XbSiloQuerySection);
	guint start = 0;

	/* common XPath parts */
	if (g_strcmp0 (xpath, "parent::*") == 0 ||
	    g_strcmp0 (xpath, "..") == 0) {
		section->kind = XB_SILO_QUERY_KIND_PARENT;
		return section;
	}
	if (g_strcmp0 (xpath, "child::*") == 0 ||
	    g_strcmp0 (xpath, "*") == 0) {
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
			if (!xb_silo_query_parse_predicate (self,
							    section,
							    xpath + start + 1,
							    i - start - 1,
							    error)) {
				xb_silo_query_section_free (section);
				return NULL;
			}
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
		xb_silo_query_section_free (section);
		return NULL;
	}
	return section;
}

static GPtrArray *
xb_silo_query_parse_sections (XbSilo *self, const gchar *xpath, GError **error)
{
	XbSiloQuerySection *section;
	g_autoptr(GPtrArray) sections = NULL;
	g_autoptr(GString) acc = g_string_new (NULL);

//	g_debug ("parsing XPath %s", xpath);
	sections = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_silo_query_section_free);
	for (gsize i = 0; xpath[i] != '\0'; i++) {

		/* escaped chars */
		if (xpath[i] == '\\') {
			if (xpath[i+1] == '/' ||
			    xpath[i+1] == 't' ||
			    xpath[i+1] == 'n') {
				g_string_append_c (acc, xpath[i+1]);
				i += 1;
				continue;
			}
		}

		/* split */
		if (xpath[i] == '/') {
			if (acc->len == 0) {
				g_set_error_literal (error,
						     G_IO_ERROR,
						     G_IO_ERROR_NOT_FOUND,
						     "xpath section empty");
				return NULL;
			}
			section = xb_silo_query_parse_section (self, acc->str, error);
			if (section == NULL)
				return NULL;
			g_ptr_array_add (sections, section);
			g_string_truncate (acc, 0);
			continue;
		}
		g_string_append_c (acc, xpath[i]);
	}

	/* add any remaining section */
	section = xb_silo_query_parse_section (self, acc->str, error);
	if (section == NULL)
		return NULL;
	g_ptr_array_add (sections, section);
	return g_steal_pointer (&sections);
}

static gboolean
xb_silo_query_node_matches (XbSilo *self,
			    XbMachine *machine,
			    XbSiloNode *sn,
			    XbSiloQuerySection *section,
			    guint *position,
			    gboolean *result,
			    GError **error)
{
	/* wildcard */
	if (section->kind == XB_SILO_QUERY_KIND_WILDCARD) {
		*result = TRUE;
		return TRUE;
	}

	/* we have an index into the string table */
	if (section->element_idx != sn->element_name) {
		*result = FALSE;
		return TRUE;
	}

	/* for section */
	*position += 1;

	/* check predicates */
	if (section->predicates != NULL) {
		for (guint i = 0; i < section->predicates->len; i++) {
			GPtrArray *opcodes = g_ptr_array_index (section->predicates, i);
			if (!xb_machine_run (machine, opcodes, result, error))
				return FALSE;
		}
	}

	/* success */
	return TRUE;
}

typedef struct {
	GPtrArray	*sections;	/* of XbSiloQuerySection */
	GPtrArray	*results;	/* of XbNode */
	GHashTable	*results_hash;	/* of sn:1 */
	guint		 limit;
} XbSiloQueryHelper;

static gboolean
xb_silo_query_section_add_result (XbSilo *self, XbSiloQueryHelper *helper, XbSiloNode *sn)
{
	if (g_hash_table_lookup (helper->results_hash, sn) != NULL)
		return FALSE;
	g_ptr_array_add (helper->results, xb_silo_node_create (self, sn));
	g_hash_table_add (helper->results_hash, sn);
	return helper->results->len == helper->limit;
}

/*
 * @parent: (allow-none)
 */
static gboolean
xb_silo_query_section_root (XbSilo *self,
			    XbSiloNode *sn,
			    XbSiloNode *parent,
			    guint i,
			    XbSiloQueryHelper *helper,
			    GError **error)
{
	XbMachine *machine = xb_silo_get_machine (self);
	XbSiloCurrent *current = xb_silo_get_current (self);
	XbSiloQuerySection *section = g_ptr_array_index (helper->sections, i);
	guint position = 0;

	/* set up level pointer */
	current->position = &position;

	/* handle parent */
	if (section->kind == XB_SILO_QUERY_KIND_PARENT) {
		XbSiloNode *grandparent;
		if (parent == NULL) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_ARGUMENT,
				     "no parent set for %s",
				     xb_silo_node_get_element (self, sn));
			return FALSE;
		}
		grandparent = xb_silo_node_get_parent (self, parent);
		if (grandparent == NULL) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_ARGUMENT,
				     "no grandparent set for %s",
				     xb_silo_node_get_element (self, parent));
			return FALSE;
		}
		if (i == helper->sections->len - 1) {
			xb_silo_query_section_add_result (self, helper, grandparent);
			return TRUE;
		}

		/* go back up to the first child of the grandparent */
		parent = xb_silo_node_get_child (self, grandparent);
		if (parent == NULL) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_ARGUMENT,
				     "no parent set for %s",
				     xb_silo_node_get_element (self, grandparent));
			return FALSE;
		}
//		g_debug ("PARENT @%u",
//			 xb_silo_get_offset_for_node (self, parent));
		return xb_silo_query_section_root (self, parent, grandparent, i + 1, helper, error);
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
	do {
		gboolean result = TRUE;
		current->sn = sn;
		if (!xb_silo_query_node_matches (self, machine, sn, section, &position, &result, error))
			return FALSE;
		if (result) {
			if (i == helper->sections->len - 1) {
//				g_debug ("add result %u",
//					 xb_silo_get_offset_for_node (self, sn));
				if (xb_silo_query_section_add_result (self, helper, sn))
					break;
			} else {
				XbSiloNode *c = xb_silo_node_get_child (self, sn);
//				g_debug ("MATCH @%u, deeper",
//					 xb_silo_get_offset_for_node (self, sn));
				if (!xb_silo_query_section_root (self, c, sn, i + 1, helper, error))
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
		    GHashTable *results_hash,
		    const gchar *xpath,
		    guint limit,
		    GError **error)
{
	g_autoptr(GPtrArray) sections = NULL;
	XbSiloQueryHelper helper = {
		.results = results,
		.limit = limit,
		.results_hash = results_hash,
	};

	/* handle each section */
	sections = xb_silo_query_parse_sections (self, xpath, error);
	if (sections == NULL)
		return FALSE;
	if (sections->len == 0) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "No query sections for '%s'",
			     xpath);
		return FALSE;
	}

	/* find each section */
	helper.sections = sections;
	return xb_silo_query_section_root (self, sroot, NULL, 0, &helper, error);
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
	g_autoptr(GHashTable) results_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	g_autoptr(GPtrArray) results = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	g_return_val_if_fail (xpath != NULL, NULL);

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
		sn = xb_silo_get_sroot (self);
		/* assume it's just a root query */
		if (xpath[0] == '/')
			xpath++;
	}

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
		if (!xb_silo_query_part (self, sn, results, results_hash, split[i], limit, error))
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
