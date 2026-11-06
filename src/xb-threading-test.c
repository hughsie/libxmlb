/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>

#include "xb-builder.h"
#include "xb-query-context.h"
#include "xb-silo-query.h"
#include "xb-test-common.h"

static void
xb_threading_cb(gpointer data, gpointer user_data)
{
	XbSilo *silo = XB_SILO(user_data);
	gint i = g_random_int_range(0, 50);
	g_autofree gchar *xpath = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) components = NULL;

	/* do query */
	xpath = g_strdup_printf("components/component/id[text()='%06i.firmware']", i);
	components = xb_silo_query(silo, xpath, 0, &error);
	g_assert_no_error(error);
	g_assert_nonnull(components);
	g_assert_cmpint(components->len, ==, 1);
	g_print(".");
}

static void
xb_threading_func(void)
{
	GThreadPool *pool;
	gboolean ret;
	guint n_components = 10000;
	g_autoptr(GError) error = NULL;
	g_autoptr(GString) xml = g_string_new(NULL);
	g_autoptr(XbSilo) silo = NULL;

#ifdef __s390x__
	/* this is run with qemu and takes too much time */
	g_test_skip("s390 too slow, skipping");
	return;
#endif

	/* create a huge document */
	g_string_append(xml, "<components>");
	for (guint i = 0; i < n_components; i++) {
		g_string_append(xml, "<component>");
		g_string_append_printf(xml, "  <id>%06u.firmware</id>", i);
		g_string_append(xml, "  <name>ColorHug2</name>");
		g_string_append(xml, "  <summary>Firmware</summary>");
		g_string_append(xml, "  <description><p>New features!</p></description>");
		g_string_append(xml, "</component>");
	}
	g_string_append(xml, "</components>");

	/* import from XML */
	silo = xb_silo_new_from_xml(xml->str, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* create thread pool */
	pool = g_thread_pool_new(xb_threading_cb, silo, 20, TRUE, &error);
	g_assert_no_error(error);
	g_assert_nonnull(pool);

	/* run threads */
	for (guint i = 0; i < 100; i++) {
		ret = g_thread_pool_push(pool, &i, &error);
		g_assert_no_error(error);
		g_assert_true(ret);
	}
	g_thread_pool_free(pool, FALSE, TRUE);
}

static void
xb_threading_run_tasks(GFunc func, gpointer user_data)
{
	GThreadPool *pool;
	gboolean ret;
	guint n_threads = 8;
	g_autoptr(GError) error = NULL;

	pool = g_thread_pool_new(func, user_data, n_threads, TRUE, &error);
	g_assert_no_error(error);
	g_assert_nonnull(pool);

	/* tasks are numbered from 1 as the pool cannot queue NULL */
	for (guint i = 1; i <= n_threads; i++) {
		ret = g_thread_pool_push(pool, GUINT_TO_POINTER(i), &error);
		g_assert_no_error(error);
		g_assert_true(ret);
	}
	g_thread_pool_free(pool, FALSE, TRUE);
}

static XbSilo *
xb_threading_search_silo_new(void)
{
	gboolean ret;
	XbSilo *silo;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderFixup) fixup = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();

	/* tokenize <name> so search() compares the interned tokens, not the raw text */
	fixup = xb_builder_fixup_new("TextTokenize", xb_test_fixup_tokenize_cb, NULL, NULL);
	xb_builder_source_add_fixup(source, fixup);
	ret = xb_builder_source_load_xml(source,
					 "<components>"
					 "<component><name>ColorHug2</name></component>"
					 "</components>",
					 XB_BUILDER_SOURCE_FLAG_NONE,
					 &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
	return silo;
}

typedef struct {
	XbSilo *silo;
	XbQuery *query;
} XbThreadingSearchHelper;

static guint
xb_threading_search_count(XbThreadingSearchHelper *helper, const gchar *term)
{
	g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT();
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;

	xb_value_bindings_bind_str(xb_query_context_get_bindings(&context), 0, term, NULL);
	results = xb_silo_query_with_context(helper->silo, helper->query, &context, &error);
	if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
		return 0;
	g_assert_no_error(error);
	return results->len;
}

static void
xb_threading_search_cb(gpointer data, gpointer user_data)
{
	XbThreadingSearchHelper *helper = (XbThreadingSearchHelper *)user_data;
	guint n_queries = 1000;
	guint offset = GPOINTER_TO_UINT(data) * n_queries;

	/* every thread shares one matching term, and the unique terms would grow
	 * any token table shared between threads */
	for (guint i = 0; i < n_queries; i++) {
		g_autofree gchar *term = g_strdup_printf("term%u", offset + i);
		g_assert_cmpint(xb_threading_search_count(helper, "colorhug"), ==, 1);
		g_assert_cmpint(xb_threading_search_count(helper, term), ==, 0);
	}
}

static void
xb_threading_search_func(void)
{
	XbThreadingSearchHelper helper = {NULL};
	g_autoptr(GError) error = NULL;
	g_autoptr(XbSilo) silo = xb_threading_search_silo_new();
	g_autoptr(XbQuery) query = NULL;

	query = xb_query_new(silo, "components/component/name[text()~=?]", &error);
	g_assert_no_error(error);
	g_assert_nonnull(query);
	helper.silo = silo;
	helper.query = query;
	xb_threading_run_tasks(xb_threading_search_cb, &helper);
}

static void
xb_threading_search_literal_cb(gpointer data, gpointer user_data)
{
	XbSilo *silo = XB_SILO(user_data);
	guint n_queries = 1000;
	guint offset = GPOINTER_TO_UINT(data) * n_queries;

	/* each literal is tokenized and interned while its query compiles */
	for (guint i = 0; i < n_queries; i++) {
		g_autofree gchar *xpath =
		    g_strdup_printf("components/component/name[text()~='term%u']", offset + i);
		g_autoptr(GError) error = NULL;
		g_autoptr(GPtrArray) results = xb_silo_query(silo, xpath, 0, &error);

		g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
		g_assert_null(results);
	}
}

static void
xb_threading_search_literal_func(void)
{
	g_autoptr(XbSilo) silo = xb_threading_search_silo_new();
	xb_threading_run_tasks(xb_threading_search_literal_cb, silo);
}

void
xb_threading_test_add_funcs(void)
{
	g_test_add_func("/libxmlb/threading/search", xb_threading_search_func);
	g_test_add_func("/libxmlb/threading/search-literal", xb_threading_search_literal_func);
	if (g_test_perf())
		g_test_add_func("/libxmlb/threading", xb_threading_func);
}
