/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>

#include "xb-silo-query.h"
#include "xb-test-common.h"

static void
xb_speed_func(void)
{
	XbNode *n;
	gboolean ret;
	guint n_components = 5000;
	g_autofree gchar *tmp_xmlb = g_build_filename(g_get_tmp_dir(), "xb-test-speed.xmlb", NULL);
	g_autofree gchar *xpath1 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(GString) xml = g_string_new(NULL);
	g_autoptr(GTimer) timer = g_timer_new();
	g_autoptr(XbSilo) silo = NULL;

#ifdef __s390x__
	/* this is run with qemu and takes too much time */
	g_test_skip("s390 too slow, skipping");
	return;
#endif

	/* create a huge document */
	g_string_append(xml, "<components>");
	for (guint i = 0; i < n_components; i++) {
		g_string_append(xml, "<component type=\"firmware\">");
		g_string_append_printf(xml, "  <id>%06u.firmware</id>", i);
		g_string_append(xml, "  <name>ColorHug2</name>");
		g_string_append(xml, "  <summary>Firmware</summary>");
		g_string_append(xml, "  <description><p>New features!</p></description>");
		g_string_append(xml, "  <provides>");
		g_string_append(xml, "    <firmware type=\"flashed\">2082b5e0</firmware>");
		g_string_append(xml, "  </provides>");
		g_string_append(xml, "  <requires>");
		g_string_append(xml, "    <id compare=\"ge\" version=\"0.8.0\">fwupd</id>");
		g_string_append(xml, "    <firmware compare=\"eq\" version=\"2.0.99\"/>");
		g_string_append(xml, "  </requires>");
		g_string_append(xml, "  <url type=\"homepage\">http://com/</url>");
		g_string_append(xml, "  <metadata_license>CC0-1.0</metadata_license>");
		g_string_append(xml, "  <project_license>GPL-2.0+</project_license>");
		g_string_append(xml, "  <updatecontact>richard</updatecontact>");
		g_string_append(xml, "  <developer_name>Hughski</developer_name>");
		g_string_append(xml, "  <releases>");
		g_string_append(
		    xml,
		    "    <release urgency=\"medium\" version=\"2.0.3\" timestamp=\"1429362707\">");
		g_string_append(
		    xml,
		    "      <description><p>stable:</p><ul><li>Quicker</li></ul></description>");
		g_string_append(xml, "    </release>");
		g_string_append(xml, "  </releases>");
		g_string_append(xml, "</component>");
	}
	g_string_append(xml, "</components>");

	/* import from XML */
	silo = xb_silo_new_from_xml(xml->str, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
	file = g_file_new_for_path(tmp_xmlb);
	ret = xb_silo_save_to_file(silo, file, NULL, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	g_clear_object(&silo);
	g_print("import+save: %.3fms\n", g_timer_elapsed(timer, NULL) * 1000);
	g_timer_reset(timer);

	/* load from file */
	silo = xb_silo_new();
	ret = xb_silo_load_from_file(silo, file, XB_SILO_LOAD_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	g_print("mmap load: %.3fms\n", g_timer_elapsed(timer, NULL) * 1000);
	g_timer_reset(timer);

	/* query best case */
	n = xb_silo_query_first(silo, "components/component/id[text()='000000.firmware']", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_print("query[first]: %.3fms\n", g_timer_elapsed(timer, NULL) * 1000);
	g_timer_reset(timer);
	g_clear_object(&n);

	/* query worst case */
	xpath1 =
	    g_strdup_printf("components/component/id[text()='%06u.firmware']", n_components - 1);
	n = xb_silo_query_first(silo, xpath1, &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_print("query[last]: %.3fms\n", g_timer_elapsed(timer, NULL) * 1000);
	g_timer_reset(timer);
	g_clear_object(&n);

	/* query all components */
	results = xb_silo_query(silo, "components/component", 0, &error);
	g_assert_no_error(error);
	g_assert_nonnull(results);
	g_assert_cmpint(results->len, ==, n_components);
	g_print("query[all]: %.3fms\n", g_timer_elapsed(timer, NULL) * 1000);
	g_timer_reset(timer);

	/* factorial search */
	for (guint i = 0; i < n_components; i += 20) {
		g_autofree gchar *xpath2 = NULL;
		xpath2 = g_strdup_printf(
		    "components/component[@type='firmware']/id[text()='%06u.firmware']",
		    i);
		n = xb_silo_query_first(silo, xpath2, &error);
		g_assert_no_error(error);
		g_assert_nonnull(n);
		g_clear_object(&n);
	}
	g_print("query[x%u]: %.3fms\n", n_components, g_timer_elapsed(timer, NULL) * 1000);
	g_timer_reset(timer);

	/* factorial search, again */
	for (guint i = 0; i < n_components; i += 20) {
		g_autofree gchar *xpath2 = NULL;
		xpath2 = g_strdup_printf(
		    "components/component[@type='firmware']/id[text()='%06u.firmware']",
		    i);
		n = xb_silo_query_first(silo, xpath2, &error);
		g_assert_no_error(error);
		g_assert_nonnull(n);
		g_clear_object(&n);
	}
	g_print("query[x%u]: %.3fms\n", n_components, g_timer_elapsed(timer, NULL) * 1000);
	g_timer_reset(timer);

	/* create an index */
	ret = xb_silo_query_build_index(silo, "components/component/id", NULL, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	ret =
	    xb_silo_query_build_index(silo, "components/component/id[text()='dave']", NULL, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	ret = xb_silo_query_build_index(silo, "components/component/DAVE", NULL, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	ret = xb_silo_query_build_index(silo, "components/component", "type", &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	g_print("create index: %.3fms\n", g_timer_elapsed(timer, NULL) * 1000);
	g_timer_reset(timer);

	/* index not found */
	n = xb_silo_query_first(silo, "components[text()=$'dave']", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
	g_assert_null(n);
	g_clear_error(&error);

	/* do the search again, this time with an index */
	g_timer_reset(timer);
	for (guint i = 0; i < n_components; i += 20) {
		g_autofree gchar *xpath2 = NULL;
		xpath2 = g_strdup_printf(
		    "components/component[attr($'type')=$'firmware']/id[text()=$'%06u.firmware']",
		    i);
		n = xb_silo_query_first(silo, xpath2, &error);
		g_assert_no_error(error);
		g_assert_nonnull(n);
		g_clear_object(&n);
	}
	g_print("query[x%u]: %.3fms\n", n_components, g_timer_elapsed(timer, NULL) * 1000);
}

static void
xb_speed_precompiled_func(void)
{
	guint n_components = 5000;
	g_autoptr(GError) error = NULL;
	g_autoptr(GString) xml = g_string_new(NULL);
	g_autoptr(GTimer) timer = g_timer_new();
	g_autoptr(XbSilo) silo = NULL;
	g_autoptr(XbQuery) query = NULL;

#ifdef __s390x__
	g_test_skip("s390 too slow, skipping");
	return;
#endif

	/* create a huge document */
	g_string_append(xml, "<components>");
	for (guint i = 0; i < n_components; i++) {
		g_string_append(xml, "<component type=\"firmware\">");
		g_string_append_printf(xml, "  <id>%06u.firmware</id>", i);
		g_string_append(xml, "  <name>ColorHug2</name>");
		g_string_append(xml, "  <summary>Firmware</summary>");
		g_string_append(xml, "</component>");
	}
	g_string_append(xml, "</components>");

	silo = xb_silo_new_from_xml(xml->str, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	query = xb_query_new(silo, "components/component[@type='firmware']", &error);
	g_assert_no_error(error);
	g_assert_nonnull(query);

	g_timer_reset(timer);
	for (guint i = 0; i < n_components; i += 20) {
		g_autoptr(GPtrArray) results = NULL;
		results = xb_silo_query_full(silo, query, &error);
		g_assert_no_error(error);
		g_assert_nonnull(results);
		if (i == 0)
			g_assert_cmpint(results->len, ==, n_components);
	}
	g_print("query_full[x%u]: %.3fms\n", n_components, g_timer_elapsed(timer, NULL) * 1000);
}

void
xb_speed_test_add_funcs(void)
{
	/* these only make sense when measuring */
	if (!g_test_perf())
		return;
	g_test_add_func("/libxmlb/speed", xb_speed_func);
	g_test_add_func("/libxmlb/speed-precompiled", xb_speed_precompiled_func);
}
