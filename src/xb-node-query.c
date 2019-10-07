/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbNode"

#include "config.h"

#include <gio/gio.h>

#include "xb-node-private.h"
#include "xb-node-query.h"
#include "xb-silo-query-private.h"

/**
 * xb_node_query:
 * @self: a #XbNode
 * @xpath: an XPath, e.g. `id[abe.desktop]`
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
xb_node_query (XbNode *self, const gchar *xpath, guint limit, GError **error)
{
	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	g_return_val_if_fail (xpath != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	return xb_silo_query_with_root (xb_node_get_silo (self), self, xpath, limit, error);
}

/**
 * xb_node_query_full:
 * @self: a #XbNode
 * @query: an #XbQuery
 * @error: the #GError, or %NULL
 *
 * Searches the silo using an prepared query.
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
xb_node_query_full (XbNode *self, XbQuery *query, GError **error)
{
	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	g_return_val_if_fail (XB_IS_QUERY (query), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	return xb_silo_query_with_root_full (xb_node_get_silo (self), self, query, error);
}

/**
 * xb_node_query_first_full:
 * @self: a #XbNode
 * @query: an #XbQuery
 * @error: the #GError, or %NULL
 *
 * Searches the silo using an prepared query, returning up to one result.
 *
 * It is safe to call this function from a different thread to the one that
 * created the #XbSilo.
 *
 * Please note: Only a subset of XPath is supported.
 *
 * Returns: (transfer full): a #XbNode, or %NULL if unfound
 *
 * Since: 0.1.11
 **/
XbNode *
xb_node_query_first_full (XbNode *self, XbQuery *query, GError **error)
{
	g_autoptr(GPtrArray) results = NULL;

	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	g_return_val_if_fail (XB_IS_QUERY (query), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* nodes don't have to include themselves as part of the query */
	results = xb_silo_query_with_root_full (xb_node_get_silo (self), self, query, error);
	if (results == NULL)
		return NULL;
	return g_object_ref (g_ptr_array_index (results, 0));
}

/**
 * xb_node_query_first:
 * @self: a #XbNode
 * @xpath: An XPath, e.g. `/components/component[@type=desktop]/id[abe.desktop]`
 * @error: the #GError, or %NULL
 *
 * Searches the node using an XPath query, returning up to one result.
 *
 * Please note: Only a tiny subset of XPath 1.0 is supported.
 *
 * Returns: (transfer full): a #XbNode, or %NULL if unfound
 *
 * Since: 0.1.0
 **/
XbNode *
xb_node_query_first (XbNode *self, const gchar *xpath, GError **error)
{
	g_autoptr(GPtrArray) results = NULL;

	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	g_return_val_if_fail (xpath != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* nodes don't have to include themselves as part of the query */
	results = xb_silo_query_with_root (xb_node_get_silo (self), self, xpath, 1, error);
	if (results == NULL)
		return NULL;
	return g_object_ref (g_ptr_array_index (results, 0));
}

/**
 * xb_node_query_text:
 * @self: a #XbNode
 * @xpath: An XPath, e.g. `/components/component[@type=desktop]/id[abe.desktop]`
 * @error: the #GError, or %NULL
 *
 * Searches the node using an XPath query, returning up to one result.
 *
 * It is safe to call this function from a different thread to the one that
 * created the #XbSilo.
 *
 * Please note: Only a subset of XPath is supported.
 *
 * Returns: (transfer none): a string, or %NULL if unfound
 *
 * Since: 0.1.0
 **/
const gchar *
xb_node_query_text (XbNode *self, const gchar *xpath, GError **error)
{
	const gchar *tmp;
	g_autoptr(XbNode) n = NULL;

	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	g_return_val_if_fail (xpath != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	n = xb_node_query_first (self, xpath, error);
	if (n == NULL)
		return NULL;
	tmp = xb_node_get_text (n);
	if (tmp == NULL) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_FOUND,
				     "no text data");
		return NULL;
	}
	return tmp;
}

/**
 * xb_node_query_attr:
 * @self: a #XbNode
 * @xpath: An XPath, e.g. `/components/component[@type=desktop]/id[abe.desktop]`
 * @name: an attribute name, e.g. `type`
 * @error: the #GError, or %NULL
 *
 * Searches the node using an XPath query, returning up to one result.
 *
 * It is safe to call this function from a different thread to the one that
 * created the #XbSilo.
 *
 * Please note: Only a subset of XPath is supported.
 *
 * Returns: (transfer none): a string, or %NULL if unfound
 *
 * Since: 0.1.0
 **/
const gchar *
xb_node_query_attr (XbNode *self, const gchar *xpath, const gchar *name, GError **error)
{
	const gchar *tmp;
	g_autoptr(XbNode) n = NULL;

	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	g_return_val_if_fail (xpath != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	n = xb_node_query_first (self, xpath, error);
	if (n == NULL)
		return NULL;
	tmp = xb_node_get_attr (n, name);
	if (tmp == NULL) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_FOUND,
				     "no text data");
		return NULL;
	}
	return tmp;
}

/**
 * xb_node_query_export:
 * @self: a #XbNode
 * @xpath: An XPath, e.g. `/components/component[@type=desktop]/id[abe.desktop]`
 * @error: the #GError, or %NULL
 *
 * Searches the node using an XPath query, returning an XML string of the
 * result and any children.
 *
 * It is safe to call this function from a different thread to the one that
 * created the #XbSilo.
 *
 * Please note: Only a subset of XPath is supported.
 *
 * Returns: (transfer none): a string, or %NULL if unfound
 *
 * Since: 0.1.0
 **/
gchar *
xb_node_query_export (XbNode *self, const gchar *xpath, GError **error)
{
	g_autoptr(XbNode) n = NULL;

	g_return_val_if_fail (XB_IS_NODE (self), NULL);
	g_return_val_if_fail (xpath != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	n = xb_node_query_first (self, xpath, error);
	if (n == NULL)
		return NULL;
	return xb_node_export (n, XB_NODE_EXPORT_FLAG_NONE, error);
}

/**
 * xb_node_query_text_as_uint:
 * @self: a #XbNode
 * @xpath: An XPath, e.g. `/components/component[@type=desktop]/id[abe.desktop]`
 * @error: the #GError, or %NULL
 *
 * Searches the node using an XPath query, returning up to one result.
 *
 * It is safe to call this function from a different thread to the one that
 * created the #XbSilo.
 *
 * Please note: Only a subset of XPath is supported.
 *
 * Returns: a guint64, or %G_MAXUINT64 if unfound
 *
 * Since: 0.1.0
 **/
guint64
xb_node_query_text_as_uint (XbNode *self, const gchar *xpath, GError **error)
{
	g_autoptr(XbNode) n = NULL;

	g_return_val_if_fail (XB_IS_NODE (self), G_MAXUINT64);
	g_return_val_if_fail (xpath != NULL, G_MAXUINT64);
	g_return_val_if_fail (error == NULL || *error == NULL, G_MAXUINT64);

	n = xb_node_query_first (self, xpath, error);
	if (n == NULL)
		return G_MAXUINT64;
	return xb_node_get_text_as_uint (n);
}

/**
 * xb_node_query_attr_as_uint:
 * @self: a #XbNode
 * @xpath: An XPath, e.g. `/components/component[@type=desktop]/id[abe.desktop]`
 * @name: an attribute name, e.g. `type`
 * @error: the #GError, or %NULL
 *
 * Searches the node using an XPath query, returning up to one result.
 *
 * It is safe to call this function from a different thread to the one that
 * created the #XbSilo.
 *
 * Please note: Only a subset of XPath is supported.
 *
 * Returns: a guint64, or %G_MAXUINT64 if unfound
 *
 * Since: 0.1.0
 **/
guint64
xb_node_query_attr_as_uint (XbNode *self, const gchar *xpath, const gchar *name, GError **error)
{
	g_autoptr(XbNode) n = NULL;

	g_return_val_if_fail (XB_IS_NODE (self), G_MAXUINT64);
	g_return_val_if_fail (xpath != NULL, G_MAXUINT64);
	g_return_val_if_fail (error == NULL || *error == NULL, G_MAXUINT64);

	n = xb_node_query_first (self, xpath, error);
	if (n == NULL)
		return G_MAXUINT64;
	return xb_node_get_attr_as_uint (n, name);
}
