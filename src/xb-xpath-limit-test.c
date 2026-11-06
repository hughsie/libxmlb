/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>

#include "xb-silo-private.h"
#include "xb-silo-query.h"
#include "xb-test-common.h"

static void
xb_xpath_or_dedup_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbSilo) silo = xb_silo_new_from_xml("<components>"
						      "<component><id>a</id></component>"
						      "<component><id>b</id></component>"
						      "</components>",
						      &error);

	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* an XPath union returns each matching node once */
	results = xb_silo_query(silo, "components/component/id|components/component/id", 0, &error);
	g_assert_no_error(error);
	g_assert_nonnull(results);
	g_assert_cmpuint(results->len, ==, 2);
	g_assert_cmpstr(xb_node_get_text(g_ptr_array_index(results, 0)), ==, "a");
	g_assert_cmpstr(xb_node_get_text(g_ptr_array_index(results, 1)), ==, "b");
}

static void
xb_xpath_or_limit_results_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbSilo) silo = xb_silo_new_from_xml("<components>"
						      "<component><id>a</id></component>"
						      "<component><id>b</id></component>"
						      "</components>",
						      &error);

	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* the limit applies to the total number of results, not per branch */
	results =
	    xb_silo_query(silo,
			  "components/component/id[text()='b']|components/component/id[text()='a']",
			  1,
			  &error);
	g_assert_no_error(error);
	g_assert_nonnull(results);
	g_assert_cmpuint(results->len, ==, 1);
	g_assert_cmpstr(xb_node_get_text(g_ptr_array_index(results, 0)), ==, "b");
}

static void
xb_xpath_or_limit_short_circuit_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbSilo) silo = xb_silo_new_from_xml("<components>"
						      "<component><id>a</id></component>"
						      "</components>",
						      &error);
	const gchar *xpath = "components/component/id[text()='a']|components//id";

	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* the first branch fills the limit, so the invalid second branch must
	 * never be evaluated -- and must not discard the collected results */
	results = xb_silo_query(silo, xpath, 1, &error);
	g_assert_no_error(error);
	g_assert_nonnull(results);
	g_assert_cmpuint(results->len, ==, 1);
	g_assert_cmpstr(xb_node_get_text(g_ptr_array_index(results, 0)), ==, "a");
}

static void
xb_silo_node_offset_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbSilo) silo =
	    xb_silo_new_from_xml("<component><id>test</id></component>", &error);
	XbSiloNode *sn = NULL;

	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* unset offsets must be rejected, not wrapped around by the bounds check */
	sn = xb_silo_get_node(silo, XB_SILO_UNSET, &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_assert_null(sn);
}

static void
xb_xpath_or_limit_func(void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(GString) xpath = g_string_new(NULL);
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<component><id>test</id></component>\n";

	ret = xb_builder_source_load_xml(source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* build a query with too many OR branches */
	for (guint i = 0; i < 100; i++) {
		if (i > 0)
			g_string_append_c(xpath, '|');
		g_string_append(xpath, "component/id");
	}
	results = xb_silo_query(silo, xpath->str, 0, &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_assert_null(results);
	g_clear_error(&error);
}

static void
xb_xpath_operator_depth_limit_func(void)
{
	XbNode *n;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(GString) xpath = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<component><id>test</id></component>\n";

	ret = xb_builder_source_load_xml(source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* a predicate with many chained operators should be rejected, not crash */
	xpath = g_string_new("component/id[");
	for (guint i = 0; i < 25; i++) {
		if (i > 0)
			g_string_append_c(xpath, '=');
		g_string_append(xpath, "'x'");
	}
	g_string_append_c(xpath, ']');
	n = xb_silo_query_first(silo, xpath->str, &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NO_SPACE);
	g_assert_null(n);
	g_clear_error(&error);

	/* a predicate within the limit should still work */
	n = xb_silo_query_first(silo, "component/id[text()='test']", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "test");
	g_clear_object(&n);
}

static void
xb_xpath_section_limit_func(void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(GString) xpath = g_string_new(NULL);
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<component><id>test</id></component>\n";

	ret = xb_builder_source_load_xml(source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* build a query with too many sections (>100) */
	for (guint i = 0; i < 102; i++) {
		if (i > 0)
			g_string_append_c(xpath, '/');
		g_string_append(xpath, "a");
	}
	results = xb_silo_query(silo, xpath->str, 0, &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_assert_null(results);
	g_clear_error(&error);
}

static void
xb_builder_strtab_ntags_limit_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderNode) root = xb_builder_node_new("root");
	g_autoptr(XbSilo) silo = NULL;

	/* build a node tree with more than 65535 unique element names */
	for (guint i = 0; i <= G_MAXUINT16; i++) {
		g_autofree gchar *name = g_strdup_printf("e%u", i);
		xb_builder_node_insert_text(root, name, NULL, NULL);
	}

	xb_builder_import_node(builder, root);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_assert_null(silo);
}

static void
xb_xpath_predicate_limit_func(void)
{
	XbNode *n;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(GString) xpath = g_string_new("component/id");
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<component><id>test</id></component>\n";

	ret = xb_builder_source_load_xml(source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* build a query with too many predicates (>20) */
	for (guint i = 0; i < 21; i++)
		g_string_append(xpath, "[text()='test']");
	n = xb_silo_query_first(silo, xpath->str, &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_assert_null(n);
	g_clear_error(&error);
}

void
xb_xpath_limit_test_add_funcs(void)
{
	g_test_add_func("/libxmlb/xpath/or-dedup", xb_xpath_or_dedup_func);
	g_test_add_func("/libxmlb/xpath/or-limit-results", xb_xpath_or_limit_results_func);
	g_test_add_func("/libxmlb/xpath/or-limit-short-circuit",
			xb_xpath_or_limit_short_circuit_func);
	g_test_add_func("/libxmlb/silo/node-offset", xb_silo_node_offset_func);
	g_test_add_func("/libxmlb/xpath/or-limit", xb_xpath_or_limit_func);
	g_test_add_func("/libxmlb/xpath/operator-depth-limit", xb_xpath_operator_depth_limit_func);
	g_test_add_func("/libxmlb/xpath/section-limit", xb_xpath_section_limit_func);
	g_test_add_func("/libxmlb/builder/strtab-ntags-limit", xb_builder_strtab_ntags_limit_func);
	g_test_add_func("/libxmlb/xpath/predicate-limit", xb_xpath_predicate_limit_func);
}
