/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>

#include "xb-node-query.h"
#include "xb-silo-query.h"
#include "xb-test-common.h"

static void
xb_node_data_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	g_autoptr(GBytes) bytes = g_bytes_new("foo", 4);

	/* import from XML */
	silo = xb_silo_new_from_xml("<id>gimp.desktop</id>", &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* get node */
	n = xb_silo_query_first(silo, "id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	xb_node_set_data(n, "store", bytes);
	xb_node_set_data(n, "store", bytes);
	g_assert_nonnull(xb_node_get_data(n, "store"));
	g_assert_null(xb_node_get_data(n, "dave"));
}

static void
xb_node_export_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	g_autofree gchar *xml_default = NULL;
	g_autofree gchar *xml_collapsed = NULL;

	/* import from XML */
	silo = xb_silo_new_from_xml("<component attr1=\"val1\" attr2=\"val2\"/>", &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* get node */
	n = xb_silo_query_first(silo, "component", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);

	/* export default */
	xml_default = xb_node_export(n, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml_default);
	g_assert_cmpstr(xml_default, ==, "<component attr1=\"val1\" attr2=\"val2\"></component>");

	/* export collapsed */
	xml_collapsed = xb_node_export(n, XB_NODE_EXPORT_FLAG_COLLAPSE_EMPTY, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml_collapsed);
	g_assert_cmpstr(xml_collapsed, ==, "<component attr1=\"val1\" attr2=\"val2\" />");
}

static void
xb_node_export_collapse_func(void)
{
	const gchar *xml = "<components><aaa /><bbb /></components>";
	g_autofree gchar *xml_collapsed = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;

	/* import from XML */
	silo = xb_silo_new_from_xml(xml, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* export collapsed */
	n = xb_silo_get_root(silo);
	g_assert_nonnull(n);
	xml_collapsed = xb_node_export(n, XB_NODE_EXPORT_FLAG_COLLAPSE_EMPTY, &error);
	g_assert_no_error(error);
	g_assert_cmpstr(xml_collapsed, ==, xml);
}

static void
xb_manual_token_search_func(void)
{
	XbNode *n;
	g_autoptr(XbNode) cpt_node = NULL;
	g_autoptr(XbBuilder) builder = NULL;
	g_autoptr(XbBuilderNode) bn_root = NULL;
	g_autoptr(XbBuilderNode) bn = NULL;
	g_autoptr(XbSilo) silo = NULL;
	g_autofree gchar *str = NULL;
	g_autoptr(XbQuery) query = NULL;
	g_autoptr(GPtrArray) result = NULL;
	g_autoptr(GError) error = NULL;
	g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT();

	/* create node tree and add a dummy token manually */
	bn_root = xb_builder_node_new("component");
	bn = xb_builder_node_new("summary");
	xb_builder_node_set_text(bn, "A strategy game", -1);
	xb_builder_node_add_token(bn, "strategy"); /* or use XB_BUILDER_NODE_FLAG_TOKENIZE_TEXT */
	xb_builder_node_add_token(bn, "strategi");
	xb_builder_node_add_child(bn_root, bn);

	builder = xb_builder_new();
	xb_builder_import_node(builder, bn_root);

	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* dump to screen */
	str = xb_silo_to_string(silo, &error);
	g_assert_no_error(error);
	g_assert_nonnull(str);
	g_debug("\n%s", str);

	/* get component node */
	cpt_node = xb_silo_query_first(silo, "component", &error);
	g_assert_no_error(error);
	g_assert_nonnull(cpt_node);

	/* search using the slow, token-less path */
	query = xb_query_new(silo, "summary[text()~=?]", &error);
	g_assert_no_error(error);
	g_assert_nonnull(query);

	xb_value_bindings_bind_str(xb_query_context_get_bindings(&context), 0, "strategy", NULL);
	result = xb_node_query_with_context(cpt_node, query, &context, &error);
	g_assert_no_error(error);
	g_assert_nonnull(result);

	n = XB_NODE(g_ptr_array_index(result, 0));
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "A strategy game");
	g_clear_object(&query);
	g_ptr_array_unref(g_steal_pointer(&result));

	/* search for token */
	xb_silo_set_profile_flags(silo, ~0);
	query = xb_query_new(silo, "summary[text()~=?]", &error);
	g_assert_no_error(error);
	g_assert_nonnull(query);

	xb_value_bindings_bind_str(xb_query_context_get_bindings(&context), 0, "strategi", NULL);
	result = xb_node_query_with_context(cpt_node, query, &context, &error);
	g_assert_no_error(error);
	g_assert_nonnull(result);

	n = XB_NODE(g_ptr_array_index(result, 0));
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "A strategy game");
}

typedef struct {
	guint cnt;
	GString *str;
} XbMarkupHelper;

static gboolean
xb_markup_head_cb(XbNode *n, gpointer user_data)
{
	XbMarkupHelper *helper = (XbMarkupHelper *)user_data;
	helper->cnt++;

	if (xb_node_get_text(n) == NULL)
		return FALSE;

	/* start */
	if (g_strcmp0(xb_node_get_element(n), "em") == 0) {
		g_string_append(helper->str, "*");
	} else if (g_strcmp0(xb_node_get_element(n), "strong") == 0) {
		g_string_append(helper->str, "**");
	} else if (g_strcmp0(xb_node_get_element(n), "code") == 0) {
		g_string_append(helper->str, "`");
	}

	/* text */
	if (xb_node_get_text(n) != NULL)
		g_string_append(helper->str, xb_node_get_text(n));

	return FALSE;
}

static gboolean
xb_markup_tail_cb(XbNode *n, gpointer user_data)
{
	XbMarkupHelper *helper = (XbMarkupHelper *)user_data;
	helper->cnt++;

	/* end */
	if (g_strcmp0(xb_node_get_element(n), "em") == 0) {
		g_string_append(helper->str, "*");
	} else if (g_strcmp0(xb_node_get_element(n), "strong") == 0) {
		g_string_append(helper->str, "**");
	} else if (g_strcmp0(xb_node_get_element(n), "code") == 0) {
		g_string_append(helper->str, "`");
	} else if (g_strcmp0(xb_node_get_element(n), "p") == 0) {
		g_string_append(helper->str, "\n\n");
	}

	/* tail */
	if (xb_node_get_tail(n) != NULL)
		g_string_append(helper->str, xb_node_get_tail(n));

	return FALSE;
}

static void
xb_markup_func(void)
{
	gboolean ret;
	g_autofree gchar *new = NULL;
	g_autofree gchar *tmp = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	XbMarkupHelper helper = {
	    .cnt = 0,
	    .str = g_string_new(NULL),
	};
	const gchar *xml =
	    "<description>"
	    "<p><code>Title</code>:</p>"
	    "<p>There is a <em>slight</em> risk of <strong>death</strong> here<a>!</a></p>"
	    "</description>";

	/* import from XML */
	silo = xb_silo_new_from_xml(xml, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* ensure we can round-trip */
	tmp = xb_silo_to_string(silo, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
	g_debug("\n%s", tmp);
	n = xb_silo_get_root(silo);
	g_assert_nonnull(n);
	new = xb_node_export(n, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_nonnull(new);
	g_assert_cmpstr(xml, ==, new);

	/* ensure we can convert this to another format */
	ret = xb_node_transmogrify(n, xb_markup_head_cb, xb_markup_tail_cb, &helper);
	g_assert_true(ret);
	g_assert_cmpstr(helper.str->str,
			==,
			"`Title`:\n\nThere is a *slight* risk of **death** here!\n\n");
	g_assert_cmpint(helper.cnt, ==, 14);
	g_string_free(helper.str, TRUE);
}

void
xb_node_test_add_funcs(void)
{
	g_test_add_func("/libxmlb/node/data", xb_node_data_func);
	g_test_add_func("/libxmlb/node/export", xb_node_export_func);
	g_test_add_func("/libxmlb/node/export-collapse", xb_node_export_collapse_func);
	g_test_add_func("/libxmlb/markup", xb_markup_func);
	g_test_add_func("/libxmlb/token-search", xb_manual_token_search_func);
}
