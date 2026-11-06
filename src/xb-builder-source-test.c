/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>

#include "xb-silo-export.h"
#include "xb-test-common.h"

static GInputStream *
xb_builder_custom_mime_cb(XbBuilderSource *self,
			  XbBuilderSourceCtx *ctx,
			  gpointer user_data,
			  GCancellable *cancellable,
			  GError **error)
{
	gchar *xml = g_strdup_printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
				     "<component type=\"desktop\">"
				     "<id>%s</id></component>",
				     xb_builder_source_ctx_get_filename(ctx));
	return g_memory_input_stream_new_from_data(g_steal_pointer(&xml), -1, g_free);
}

static void
xb_builder_custom_mime_func(void)
{
	gboolean ret;
	g_autofree gchar *xml = NULL;
	g_autofree gchar *tmp_desktop =
	    g_build_filename(g_get_tmp_dir(), "xb-test-custom-mime.desktop", NULL);
	g_autofree gchar *tmp_xmlb =
	    g_build_filename(g_get_tmp_dir(), "xb-test-custom-mime.xmlb", NULL);
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file_desktop = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;

	/* add support for desktop files */
	xb_builder_source_add_adapter(source,
				      "application/x-desktop",
				      xb_builder_custom_mime_cb,
				      NULL,
				      NULL);

	/* import a source file */
	ret = g_file_set_contents(tmp_desktop, "[Desktop Entry]", -1, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	file_desktop = g_file_new_for_path(tmp_desktop);
	ret = xb_builder_source_load_file(source,
					  file_desktop,
					  XB_BUILDER_SOURCE_FLAG_WATCH_FILE,
					  NULL,
					  &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	file = g_file_new_for_path(tmp_xmlb);
	silo = xb_builder_ensure(builder, file, XB_BUILDER_COMPILE_FLAG_WATCH_BLOB, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* check contents */
	xml = xb_silo_export(silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml);
	g_print("%s", xml);
	g_assert_cmpstr("<component type=\"desktop\">"
			"<id>xb-test-custom-mime.desktop</id>"
			"</component>",
			==,
			xml);
}

static void
xb_builder_source_lzma_func(void)
{
	gboolean ret;
	g_autofree gchar *path = NULL;
	g_autofree gchar *tmp_xmlb =
	    g_build_filename(g_get_tmp_dir(), "xb-test-source-lzma.xmlb", NULL);
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GFile) file_src = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;

#ifndef HAVE_LZMA
	/* not supported */
	g_test_skip("compiled without -Dlzma");
	return;
#endif

	/* import a source file */
	path = g_test_build_filename(G_TEST_DIST, "test.xml.xz", NULL);
	file_src = g_file_new_for_path(path);
	if (!g_file_query_exists(file_src, NULL)) {
		g_test_skip("does not work in subproject test");
		return;
	}
	ret = xb_builder_source_load_file(source,
					  file_src,
					  XB_BUILDER_SOURCE_FLAG_NONE,
					  NULL,
					  &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	file = g_file_new_for_path(tmp_xmlb);
	silo = xb_builder_ensure(builder, file, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
}

static void
xb_builder_source_zstd_func(void)
{
	gboolean ret;
	g_autofree gchar *path = NULL;
	g_autofree gchar *tmp_xmlb =
	    g_build_filename(g_get_tmp_dir(), "xb-test-source-zstd.xmlb", NULL);
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GFile) file_src = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;

#ifndef HAVE_ZSTD
	/* not supported */
	g_test_skip("compiled without -Dzstd");
	return;
#endif

	/* import a source file */
	path = g_test_build_filename(G_TEST_DIST, "test.xml.zst", NULL);
	file_src = g_file_new_for_path(path);
	if (!g_file_query_exists(file_src, NULL)) {
		g_test_skip("does not work in subproject test");
		return;
	}
	ret = xb_builder_source_load_file(source,
					  file_src,
					  XB_BUILDER_SOURCE_FLAG_NONE,
					  NULL,
					  &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	file = g_file_new_for_path(tmp_xmlb);
	silo = xb_builder_ensure(builder, file, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
}

static void
xb_builder_chained_adapters_func(void)
{
	gboolean ret;
	g_autofree gchar *xml = NULL;
	g_autofree gchar *path = NULL;
	g_autofree gchar *tmp_xmlb =
	    g_build_filename(g_get_tmp_dir(), "xb-test-chained-adapters.xmlb", NULL);
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file_src = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;

	/* import a source file */
	path = g_test_build_filename(G_TEST_DIST, "test.xml.gz.gz.gz", NULL);
	file_src = g_file_new_for_path(path);
	if (!g_file_query_exists(file_src, NULL)) {
		g_test_skip("does not work in subproject test");
		return;
	}
	ret = xb_builder_source_load_file(source,
					  file_src,
					  XB_BUILDER_SOURCE_FLAG_NONE,
					  NULL,
					  &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	file = g_file_new_for_path(tmp_xmlb);
	silo = xb_builder_ensure(builder, file, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* check contents */
	xml = xb_silo_export(silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml);
	g_print("%s", xml);
	g_assert_cmpstr("<id>Hello world!</id>", ==, xml);
}

void
xb_builder_source_test_add_funcs(void)
{
	g_test_add_func("/libxmlb/builder/custom-mime", xb_builder_custom_mime_func);
	g_test_add_func("/libxmlb/builder/chained-adapters", xb_builder_chained_adapters_func);
	g_test_add_func("/libxmlb/builder/source-lzma", xb_builder_source_lzma_func);
	g_test_add_func("/libxmlb/builder/source-zstd", xb_builder_source_zstd_func);
}
