/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>

#include "xb-silo-export.h"
#include "xb-silo-query.h"
#include "xb-test-common.h"

static void
xb_builder_func(void)
{
	g_autofree gchar *str = NULL;
	g_autofree gchar *xml_new = NULL;
	g_autoptr(GBytes) bytes = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<components origin=\"lvfs\">\n"
			   "  <header type=\"&lt;&amp;&gt;\">\n"
			   "    <csum type=\"sha1\">dead</csum>\n"
			   "  </header>\n"
			   "  <component type=\"desktop\" attr=\"value\">\n"
			   "    <id>gimp.desktop</id>\n"
			   "    <name>GIMP &amp; Friendẞ</name>\n"
			   "    <id>org.gnome.Gimp.desktop</id>\n"
			   "  </component>\n"
			   "  <component type=\"desktop\">\n"
			   "    <id>gnome-software.desktop</id>\n"
			   "  </component>\n"
			   "  <component type=\"firmware\">\n"
			   "    <id>org.hughski.ColorHug2.firmware</id>\n"
			   "    <requires>\n"
			   "      <bootloader>1.2.3</bootloader>\n"
			   "    </requires>\n"
			   "  </component>\n"
			   "</components>\n";

	/* import from XML */
	silo = xb_silo_new_from_xml(xml, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
	g_assert_true(xb_silo_is_valid(silo));

	/* convert back to XML */
	str = xb_silo_to_string(silo, &error);
	g_assert_no_error(error);
	g_assert_nonnull(str);
	g_debug("\n%s", str);
	xml_new =
	    xb_silo_export(silo,
			   XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE | XB_NODE_EXPORT_FLAG_FORMAT_INDENT,
			   &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml_new);
	g_print("%s", xml_new);
	g_assert_cmpstr(xml, ==, xml_new);

	/* check size */
	bytes = xb_silo_get_bytes(silo);
	g_assert_cmpint(g_bytes_get_size(bytes), ==, 628);
}

static void
xb_builder_ensure_invalidate_cb(XbSilo *silo, GParamSpec *pspec, gpointer user_data)
{
	guint *invalidate_cnt = (guint *)user_data;
	(*invalidate_cnt)++;
	xb_test_loop_quit();
}

static void
xb_builder_ensure_watch_source_func(void)
{
	gboolean ret;
	guint invalidate_cnt = 0;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GFile) file_xml = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	g_autofree gchar *tmp_xml =
	    g_build_filename(g_get_tmp_dir(), "xb-test-ensure-watch-source.xml", NULL);
	g_autofree gchar *tmp_xmlb =
	    g_build_filename(g_get_tmp_dir(), "xb-test-ensure-watch-source.xmlb", NULL);

#ifdef _WIN32
	/* no inotify */
	g_test_skip("inotify does not work on mingw");
	return;
#endif

	/* import a source file */
	ret = g_file_set_contents(tmp_xml,
				  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				  "<id>gimp</id>",
				  -1,
				  &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	file_xml = g_file_new_for_path(tmp_xml);
	ret = xb_builder_source_load_file(source,
					  file_xml,
					  XB_BUILDER_SOURCE_FLAG_WATCH_FILE,
					  NULL,
					  &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	file = g_file_new_for_path(tmp_xmlb);
	g_file_delete(file, NULL, NULL);
	silo = xb_builder_ensure(builder, file, XB_BUILDER_COMPILE_FLAG_WATCH_BLOB, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
	g_assert_true(xb_silo_is_valid(silo));
	g_signal_connect(silo,
			 "notify::valid",
			 G_CALLBACK(xb_builder_ensure_invalidate_cb),
			 &invalidate_cnt);

	/* change source file */
	ret = g_file_set_contents(tmp_xml,
				  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				  "<id>inkscape</id>",
				  -1,
				  &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_test_loop_run_with_timeout(XB_TEST_INOTIFY_TIMEOUT);
	g_assert_false(xb_silo_is_valid(silo));
	g_assert_cmpint(invalidate_cnt, ==, 1);
	g_assert_false(xb_silo_is_valid(silo));
}

static void
xb_builder_ensure_func(void)
{
	gboolean ret;
	guint invalidate_cnt = 0;
	g_autofree gchar *bytes1str = NULL;
	g_autofree gchar *bytes2str = NULL;
	g_autofree gchar *bytes3str = NULL;
	g_autofree gchar *tmp_xmlb = g_build_filename(g_get_tmp_dir(), "xb-test-ensure.xmlb", NULL);
	g_autoptr(GBytes) bytes1 = NULL;
	g_autoptr(GBytes) bytes2 = NULL;
	g_autoptr(GBytes) bytes3 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<components origin=\"lvfs\">\n"
			   "  <header type=\"&lt;&amp;&gt;\">\n"
			   "    <csum type=\"sha1\">dead</csum>\n"
			   "  </header>\n"
			   "  <component type=\"desktop\" attr=\"value\">\n"
			   "    <id>gimp.desktop</id>\n"
			   "    <name>GIMP &amp; Friendẞ</name>\n"
			   "    <id>org.gnome.Gimp.desktop</id>\n"
			   "  </component>\n"
			   "  <component type=\"desktop\">\n"
			   "    <id>gnome-software.desktop</id>\n"
			   "  </component>\n"
			   "  <component type=\"firmware\">\n"
			   "    <id>org.hughski.ColorHug2.firmware</id>\n"
			   "    <requires>\n"
			   "      <bootloader>1.2.3</bootloader>\n"
			   "    </requires>\n"
			   "  </component>\n"
			   "</components>\n";

#ifdef _WIN32
	/* no inotify */
	g_test_skip("inotify does not work on mingw");
	return;
#endif
#ifdef __sun
	/* no inotify */
	g_test_skip("missing support for inotify");
	return;
#endif

	/* import some XML */
	xb_builder_set_profile_flags(builder, XB_SILO_PROFILE_FLAG_DEBUG);
	ret = xb_test_import_xml(builder, xml, &error);
	g_assert_no_error(error);
	g_assert_true(ret);

	/* create file if it does not exist */
	file = g_file_new_for_path(tmp_xmlb);
	g_file_delete(file, NULL, NULL);
	silo = xb_builder_ensure(builder, file, XB_BUILDER_COMPILE_FLAG_WATCH_BLOB, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
	g_signal_connect(silo,
			 "notify::valid",
			 G_CALLBACK(xb_builder_ensure_invalidate_cb),
			 &invalidate_cnt);
	g_assert_cmpint(invalidate_cnt, ==, 0);
	bytes1 = xb_silo_get_bytes(silo);
	bytes1str = g_compute_checksum_for_bytes(G_CHECKSUM_SHA1, bytes1);

	/* recreate file if it is invalid */
	ret = g_file_replace_contents(file,
				      "dave",
				      4,
				      NULL,
				      FALSE,
				      G_FILE_CREATE_NONE,
				      NULL,
				      NULL,
				      &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_test_loop_run_with_timeout(XB_TEST_INOTIFY_TIMEOUT);
	g_assert_false(xb_silo_is_valid(silo));
	g_assert_cmpint(invalidate_cnt, ==, 1);

	g_clear_object(&silo);
	silo = xb_builder_ensure(builder, file, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
	g_assert_true(xb_silo_is_valid(silo));
	bytes2 = xb_silo_get_bytes(silo);
	bytes2str = g_compute_checksum_for_bytes(G_CHECKSUM_SHA1, bytes2);
	g_assert_cmpstr(bytes1str, !=, bytes2str);
	g_clear_object(&silo);

	/* don't recreate file if perfectly valid */
	silo = xb_builder_ensure(builder, file, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
	g_assert_true(xb_silo_is_valid(silo));
	bytes3 = xb_silo_get_bytes(silo);
	bytes3str = g_compute_checksum_for_bytes(G_CHECKSUM_SHA1, bytes3);
	g_assert_cmpstr(bytes2str, ==, bytes3str);
	g_clear_object(&silo);
	g_clear_object(&builder);

	/* don't re-create for a new builder with the same XML added */
	builder = xb_builder_new();
	ret = xb_test_import_xml(builder, xml, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	silo = xb_builder_ensure(builder, file, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
}

static gboolean
xb_builder_error_cb(XbBuilderFixup *self, XbBuilderNode *bn, gpointer user_data, GError **error)
{
	g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_BUSY, "engine was busy");
	return FALSE;
}

static void
xb_builder_node_vfunc_error_func(void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderFixup) fixup = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;

	/* add fixup */
	fixup = xb_builder_fixup_new("AlwaysError", xb_builder_error_cb, NULL, NULL);
	xb_builder_source_add_fixup(source, fixup);

	/* import some XML */
	ret = xb_builder_source_load_xml(source,
					 "<id>gimp.desktop</id>",
					 XB_BUILDER_SOURCE_FLAG_NONE,
					 &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_BUSY);
	g_assert_null(silo);
}

static gboolean
xb_builder_ignore_cb(XbBuilderFixup *self, XbBuilderNode *bn, gpointer user_data, GError **error)
{
	if (xb_builder_node_get_element(bn) != NULL)
		xb_builder_node_add_flag(bn, XB_BUILDER_NODE_FLAG_IGNORE);
	return TRUE;
}

static void
xb_builder_node_vfunc_ignore_func(void)
{
	XbNodeChildIter iter;
	gboolean ret;
	const gchar *element;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderFixup) fixup = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbBuilderNode) info = NULL;
	g_autoptr(XbSilo) silo = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbNode) c = NULL;
	g_autoptr(XbNode) c2 = NULL;

	/* add fixup */
	fixup = xb_builder_fixup_new("AlwaysIgnore", xb_builder_ignore_cb, NULL, NULL);
	xb_builder_source_add_fixup(source, fixup);

	/* import some XML with metadata */
	ret = xb_builder_source_load_xml(source,
					 "<component><id>gimp.desktop</id></component>",
					 XB_BUILDER_SOURCE_FLAG_NONE,
					 &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	info = xb_builder_node_insert(NULL, "info", NULL);
	xb_builder_node_insert_text(info, "foo", "bar", NULL);
	xb_builder_source_set_info(source, info);
	xb_builder_import_source(builder, source);

	/* compile */
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
	n = xb_silo_get_root(silo);
	g_assert_nonnull(n);
	element = xb_node_get_element(n);
	g_assert_null(element);
	c = xb_node_get_child(n);
	g_assert_null(c);
	c = xb_node_get_next(n);
	g_assert_null(c);
	xb_node_child_iter_init(&iter, n);
	ret = xb_node_child_iter_next(&iter, &c2);
	g_assert_false(ret);
	g_assert_null(c2);
}

static gboolean
xb_builder_upgrade_appstream_cb(XbBuilderFixup *self,
				XbBuilderNode *bn,
				gpointer user_data,
				GError **error)
{
	if (g_strcmp0(xb_builder_node_get_element(bn), "application") == 0) {
		g_autoptr(XbBuilderNode) id = xb_builder_node_get_child(bn, "id", NULL);
		g_autofree gchar *kind = NULL;
		if (id != NULL) {
			kind = g_strdup(xb_builder_node_get_attr(id, "type"));
			xb_builder_node_remove_attr(id, "type");
		}
		if (kind != NULL)
			xb_builder_node_set_attr(bn, "type", kind);
		xb_builder_node_set_element(bn, "component");
	} else if (g_strcmp0(xb_builder_node_get_element(bn), "metadata") == 0) {
		xb_builder_node_set_element(bn, "custom");
	}
	return TRUE;
}

static void
xb_builder_node_vfunc_func(void)
{
	gboolean ret;
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderFixup) fixup = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "  <application>\n"
			   "    <id type=\"desktop\">gimp.desktop</id>\n"
			   "  </application>\n";

	/* add fixup */
	fixup =
	    xb_builder_fixup_new("AppStreamUpgrade", xb_builder_upgrade_appstream_cb, NULL, NULL);
	xb_builder_source_add_fixup(source, fixup);

	/* import some XML */
	ret = xb_builder_source_load_xml(source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* check the XML */
	xml2 = xb_silo_export(silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml2);
	g_print("%s\n", xml2);
	g_assert_cmpstr("<component type=\"desktop\">"
			"<id>gimp.desktop</id>"
			"</component>",
			==,
			xml2);
}

static gboolean
xb_builder_fixup_ignore_node_cb(XbBuilderFixup *self,
				XbBuilderNode *bn,
				gpointer user_data,
				GError **error)
{
	if (g_strcmp0(xb_builder_node_get_element(bn), "component") == 0) {
		g_autoptr(XbBuilderNode) id = xb_builder_node_get_child(bn, "id", NULL);
		if (id != NULL && g_strcmp0(xb_builder_node_get_text(id), "gimp.desktop") == 0)
			xb_builder_node_add_flag(bn, XB_BUILDER_NODE_FLAG_IGNORE);
	} else {
		g_debug("ignoring %s", xb_builder_node_get_element(bn));
	}
	return TRUE;
}

static void
xb_builder_node_vfunc_remove_func(void)
{
	gboolean ret;
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderFixup) fixup = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "  <components>\n"
			   "    <component>\n"
			   "      <id>gimp.desktop</id>\n"
			   "    </component>\n"
			   "    <component>\n"
			   "      <id>inkscape.desktop</id>\n"
			   "    </component>\n"
			   "  </components>\n";

	/* add fixup */
	fixup = xb_builder_fixup_new("RemoveGimp", xb_builder_fixup_ignore_node_cb, NULL, NULL);
	xb_builder_source_add_fixup(source, fixup);

	/* import some XML */
	ret = xb_builder_source_load_xml(source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* check the XML */
	xml2 = xb_silo_export(silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml2);
	g_print("%s\n", xml2);
	g_assert_cmpstr("<components>"
			"<component>"
			"<id>inkscape.desktop</id>"
			"</component>"
			"</components>",
			==,
			xml2);
}

static gboolean
xb_builder_fixup_root_node_only_cb(XbBuilderFixup *self,
				   XbBuilderNode *bn,
				   gpointer user_data,
				   GError **error)
{
	g_debug(">%s<", xb_builder_node_get_element(bn));
	g_assert_cmpstr(xb_builder_node_get_element(bn), ==, NULL);
	return TRUE;
}

static void
xb_builder_node_vfunc_depth_func(void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderFixup) fixup = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "  <components>\n"
			   "    <component>\n"
			   "      <id>gimp.desktop</id>\n"
			   "    </component>\n"
			   "  </components>\n";

	/* add fixup */
	fixup = xb_builder_fixup_new("OnlyRoot", xb_builder_fixup_root_node_only_cb, NULL, NULL);
	xb_builder_fixup_set_max_depth(fixup, 0);
	xb_builder_source_add_fixup(source, fixup);

	/* import some XML */
	ret = xb_builder_source_load_xml(source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
}

static void
xb_builder_ignore_invalid_func(void)
{
	gboolean ret;
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbSilo) silo = NULL;

	/* import some correct XML */
	ret = xb_test_import_xml(builder, "<book><id>foobar</id></book>", &error);
	g_assert_no_error(error);
	g_assert_true(ret);

	/* import some incorrect XML */
	ret = xb_test_import_xml(builder, "<book><id>foobar</id>", &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_IGNORE_INVALID, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* check the XML */
	xml2 = xb_silo_export(silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml2);
	g_print("%s\n", xml2);
	g_assert_cmpstr("<book><id>foobar</id></book>", ==, xml2);
}

static void
xb_builder_ignore_invalid_depth_func(void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(GString) deep = g_string_new("<root>");
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbSilo) silo = NULL;

	/* the depth limit is per source, not per compile; nest to just inside
	 * the 100 level limit supported by the builder */
	for (guint i = 0; i < 98; i++)
		g_string_append(deep, "<child>");
	g_string_append(deep, "<leaf/>");
	for (guint i = 0; i < 98; i++)
		g_string_append(deep, "</child>");
	g_string_append(deep, "</root>");

	/* import some incorrect XML, leaving the parser mid-tree */
	ret = xb_test_import_xml(builder, "<book><id>foobar</id>", &error);
	g_assert_no_error(error);
	g_assert_true(ret);

	/* import a valid document nested exactly to the depth limit */
	ret = xb_test_import_xml(builder, deep->str, &error);
	g_assert_no_error(error);
	g_assert_true(ret);

	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_IGNORE_INVALID, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* the deep document must not be dropped for exceeding a leaked depth count */
	results = xb_silo_query(silo, "root", 1, &error);
	g_assert_no_error(error);
	g_assert_nonnull(results);
	g_assert_cmpuint(results->len, ==, 1);
}

static void
xb_builder_empty_func(void)
{
	gboolean ret;
	g_autofree gchar *str = NULL;
	g_autofree gchar *xml = NULL;
	g_autoptr(GBytes) bytes = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbSilo) silo2 = xb_silo_new();
	g_autoptr(XbSilo) silo = NULL;

	/* import from XML */
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
	g_assert_true(xb_silo_is_valid(silo));

	/* check size */
	bytes = xb_silo_get_bytes(silo);
	g_assert_cmpint(g_bytes_get_size(bytes), ==, 40);

	/* try to dump */
	str = xb_silo_to_string(silo, &error);
	g_assert_no_error(error);
	g_assert_nonnull(str);
	g_debug("%s", str);

	/* try to export */
	xml = xb_silo_export(silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(xml);
	g_clear_error(&error);

	/* try to query empty silo */
	results = xb_silo_query(silo, "components/component", 0, &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(results);
	g_clear_error(&error);

	/* load blob */
	g_assert_nonnull(bytes);
	ret = xb_silo_load_from_bytes(silo2, bytes, 0, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
}

static void
xb_builder_native_lang_func(void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *str = NULL;
	g_autofree gchar *tmp = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<components>\n"
			   "  <component>\n"
			   "    <p xml:lang=\"de_DE\">Wilcommen</p>\n"
			   "    <p>Hello</p>\n"
			   "    <p xml:lang=\"fr\">Salut</p>\n"
			   "    <p>Goodbye</p>\n"
			   "    <p xml:lang=\"de_DE\">Auf Wiedersehen</p>\n"
			   "    <p xml:lang=\"fr\">Au revoir</p>\n"
			   "  </component>\n"
			   "</components>\n";

	/* import from XML */
	ret = xb_test_import_xml(builder, xml, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_add_locale(builder, "fr_FR.UTF-8");
	xb_builder_add_locale(builder, "fr_FR");
	xb_builder_add_locale(builder, "fr_FR");
	xb_builder_add_locale(builder, "fr");
	xb_builder_add_locale(builder, "C");
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_SINGLE_LANG, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* test we removed other languages */
	str = xb_silo_to_string(silo, &error);
	g_assert_no_error(error);
	g_assert_nonnull(str);
	g_debug("\n%s", str);
	g_assert_null(g_strstr_len(str, -1, "Wilcommen"));
	g_assert_null(g_strstr_len(str, -1, "Hello"));
	g_assert_nonnull(g_strstr_len(str, -1, "Salut"));
	g_assert_null(g_strstr_len(str, -1, "Goodbye"));
	g_assert_null(g_strstr_len(str, -1, "Auf Wiedersehen"));
	g_assert_nonnull(g_strstr_len(str, -1, "Au revoir"));

	/* test we traversed the tree correctly */
	n = xb_silo_query_first(silo, "components/component/*", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	tmp = xb_node_export(n, XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS, &error);
	g_assert_cmpstr(tmp, ==, "<p xml:lang=\"fr\">Salut</p><p xml:lang=\"fr\">Au revoir</p>");
}

static void
xb_builder_native_lang_bcp47_func(void)
{
	gboolean ret;
	g_autofree gchar *str = NULL;
	g_autofree gchar *tmp = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<components>\n"
			   "  <component>\n"
			   "    <p xml:lang=\"de-DE\">Wilcommen</p>\n"
			   "    <p>Hello</p>\n"
			   "    <p xml:lang=\"fr-FR\">Salut</p>\n"
			   "    <p xml:lang=\"fr_FR\">Au revoir</p>\n"
			   "  </component>\n"
			   "</components>\n";

	/* import from XML */
	ret = xb_test_import_xml(builder, xml, &error);
	g_assert_no_error(error);
	g_assert_true(ret);

	/* the locales are added in POSIX form, but the `xml:lang` attributes
	 * are in BCP47 form -- both have to match */
	xb_builder_add_locale(builder, "fr_FR");
	xb_builder_add_locale(builder, "C");
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_SINGLE_LANG, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* test we removed other languages */
	str = xb_silo_to_string(silo, &error);
	g_assert_no_error(error);
	g_assert_nonnull(str);
	g_debug("\n%s", str);
	g_assert_null(g_strstr_len(str, -1, "Wilcommen"));
	g_assert_null(g_strstr_len(str, -1, "Hello"));
	g_assert_nonnull(g_strstr_len(str, -1, "Salut"));
	g_assert_nonnull(g_strstr_len(str, -1, "Au revoir"));

	/* the attribute value itself is preserved as-authored */
	n = xb_silo_query_first(silo, "components/component/*", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	tmp = xb_node_export(n, XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS, &error);
	g_assert_no_error(error);
	g_assert_cmpstr(tmp,
			==,
			"<p xml:lang=\"fr-FR\">Salut</p><p xml:lang=\"fr_FR\">Au revoir</p>");
}

static void
xb_builder_native_lang_bcp47_match_func(void)
{
	gboolean ret;
	g_autofree gchar *str = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<components>\n"
			   "  <component>\n"
			   "    <p xml:lang=\"en-gb\">Colour</p>\n"
			   "    <p xml:lang=\"zh-Hans-CN\">Simplified</p>\n"
			   "    <p xml:lang=\"zh-Hant-CN\">Traditional</p>\n"
			   "    <p xml:lang=\"de-DE\">Wilcommen</p>\n"
			   "  </component>\n"
			   "</components>\n";

	/* import from XML */
	ret = xb_test_import_xml(builder, xml, &error);
	g_assert_no_error(error);
	g_assert_true(ret);

	/* tags differing in case or by an implied script match, but ones in a
	 * different script do not */
	xb_builder_add_locale(builder, "en_GB");
	xb_builder_add_locale(builder, "zh_CN");
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	str = xb_silo_to_string(silo, &error);
	g_assert_no_error(error);
	g_assert_nonnull(str);
	g_debug("\n%s", str);
	g_assert_nonnull(g_strstr_len(str, -1, "Colour"));
	g_assert_nonnull(g_strstr_len(str, -1, "Simplified"));
	g_assert_null(g_strstr_len(str, -1, "Traditional"));
	g_assert_null(g_strstr_len(str, -1, "Wilcommen"));
}

static void
xb_builder_comments_func(void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *str = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<?xml version=\"1.0\" ?>\n"
			   "<components>\n"
			   "  <!-- one -->\n"
			   "  <!-- two -->\n"
			   "</components>\n";

	/* import XML */
	ret = xb_test_import_xml(builder, xml, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* export */
	str = xb_silo_export(silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_cmpstr(str, ==, "<components></components>");
}

static void
xb_builder_cdata_func(void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *str = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
	    "<?xml version=\"1.0\" ?>\n"
	    "<components>\n"
	    "  <component>\n"
	    "     <text>plain text</text>\n"
	    "     <summary><![CDATA[1 2 3]]></summary>\n"
	    "     <description><p>s <![CDATA[1]]> p <![CDATA[2]]> l <![CDATA[3]]> i <![CDATA[4]]> "
	    "t</p><p><![CDATA[><]]></p><p><![CDATA[\"]]></p></description>\n"
	    "  </component>\n"
	    "</components>\n";

	/* import XML */
	ret = xb_test_import_xml(builder, xml, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* export */
	str = xb_silo_export(silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_cmpstr(str,
			==,
			"<components><component><text>plain text</text><summary>1 2 "
			"3</summary><description><p>s 1 p 2 l 3 i 4 "
			"t</p><p>&gt;&lt;</p><p>&quot;</p></description></component></components>");
}

static void
xb_builder_native_lang2_func(void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *str = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<components>\n"
			   "  <component>\n"
			   "    <description xml:lang=\"de_DE\"><p>Wilcommen</p></description>\n"
			   "    <description><p>Hello</p></description>\n"
			   "    <description xml:lang=\"fr\"><p>Salut</p></description>\n"
			   "  </component>\n"
			   "</components>\n";

	/* import from XML */
	ret = xb_test_import_xml(builder, xml, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_add_locale(builder, "fr_FR");
	xb_builder_add_locale(builder, "fr");
	xb_builder_add_locale(builder, "C");
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_SINGLE_LANG, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* test we removed other languages */
	str = xb_silo_to_string(silo, &error);
	g_assert_no_error(error);
	g_assert_nonnull(str);
	g_assert_null(g_strstr_len(str, -1, "Wilcommen"));
	g_assert_null(g_strstr_len(str, -1, "Hello"));
	g_assert_nonnull(g_strstr_len(str, -1, "Salut"));
	g_debug("\n%s", str);
}

static void
xb_builder_multiple_roots_func(void)
{
	gboolean ret;
	g_autofree gchar *str = NULL;
	g_autofree gchar *xml_new = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbBuilder) builder = NULL;
	g_autoptr(XbSilo) silo = NULL;

	/* import from XML */
	builder = xb_builder_new();
	ret = xb_test_import_xml(builder, "<tag>value</tag>", &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	ret = xb_test_import_xml(builder, "<tag>value2</tag><tag>value3</tag>", &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* convert back to XML */
	str = xb_silo_to_string(silo, &error);
	g_assert_no_error(error);
	g_assert_nonnull(str);
	g_debug("\n%s", str);
	xml_new = xb_silo_export(silo, XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml_new);
	g_print("%s", xml_new);
	g_assert_cmpstr("<tag>value</tag><tag>value2</tag><tag>value3</tag>", ==, xml_new);

	/* query for multiple results */
	results = xb_silo_query(silo, "tag", 5, &error);
	g_assert_no_error(error);
	g_assert_nonnull(results);
	g_assert_cmpint(results->len, ==, 3);
}

static void
xb_builder_single_root_func(void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = NULL;
	g_autoptr(XbSilo) silo = NULL;

	/* import from XML */
	builder = xb_builder_new();
	ret = xb_test_import_xml(builder, "<tag>value2</tag><tag>value3</tag>", &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_SINGLE_ROOT, NULL, &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_assert_null(silo);
}

static void
xb_builder_multiroot_info_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbBuilderNode) info = xb_builder_node_insert(NULL, "info", NULL);
	g_autoptr(XbSilo) silo = NULL;
	g_autoptr(XbNode) na = NULL;
	g_autoptr(XbNode) nb = NULL;
	gboolean ret;

	/* a document with two root elements and an info node: each root must get
	 * its own copy of the info metadata */
	xb_builder_node_insert_text(info, "foo", "bar", NULL);
	ret = xb_builder_source_load_xml(source,
					 "<a>1</a><b>2</b>",
					 XB_BUILDER_SOURCE_FLAG_NONE,
					 &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_source_set_info(source, info);
	xb_builder_import_source(builder, source);

	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	na = xb_silo_query_first(silo, "a/info/foo", &error);
	g_assert_no_error(error);
	g_assert_nonnull(na);
	g_assert_cmpstr(xb_node_get_text(na), ==, "bar");

	nb = xb_silo_query_first(silo, "b/info/foo", &error);
	g_assert_no_error(error);
	g_assert_nonnull(nb);
	g_assert_cmpstr(xb_node_get_text(nb), ==, "bar");
}

void
xb_builder_test_add_funcs(void)
{
	g_test_add_func("/libxmlb/builder/multiroot-info", xb_builder_multiroot_info_func);
	g_test_add_func("/libxmlb/builder", xb_builder_func);
	g_test_add_func("/libxmlb/builder/comments", xb_builder_comments_func);
	g_test_add_func("/libxmlb/builder/cdata", xb_builder_cdata_func);
	g_test_add_func("/libxmlb/builder/native-lang", xb_builder_native_lang_func);
	g_test_add_func("/libxmlb/builder/native-lang-bcp47", xb_builder_native_lang_bcp47_func);
	g_test_add_func("/libxmlb/builder/native-lang-bcp47-match",
			xb_builder_native_lang_bcp47_match_func);
	g_test_add_func("/libxmlb/builder/native-lang-nested", xb_builder_native_lang2_func);
	g_test_add_func("/libxmlb/builder/empty", xb_builder_empty_func);
	g_test_add_func("/libxmlb/builder/ensure", xb_builder_ensure_func);
	g_test_add_func("/libxmlb/builder/ensure-watch-source",
			xb_builder_ensure_watch_source_func);
	g_test_add_func("/libxmlb/builder/node-vfunc", xb_builder_node_vfunc_func);
	g_test_add_func("/libxmlb/builder/node-vfunc-remove", xb_builder_node_vfunc_remove_func);
	g_test_add_func("/libxmlb/builder/node-vfunc-depth", xb_builder_node_vfunc_depth_func);
	g_test_add_func("/libxmlb/builder/node-vfunc-error", xb_builder_node_vfunc_error_func);
	g_test_add_func("/libxmlb/builder/node-vfunc-ignore", xb_builder_node_vfunc_ignore_func);
	g_test_add_func("/libxmlb/builder/ignore-invalid", xb_builder_ignore_invalid_func);
	g_test_add_func("/libxmlb/builder/ignore-invalid-depth",
			xb_builder_ignore_invalid_depth_func);
	g_test_add_func("/libxmlb/multiple-roots", xb_builder_multiple_roots_func);
	g_test_add_func("/libxmlb/single-root", xb_builder_single_root_func);
}
