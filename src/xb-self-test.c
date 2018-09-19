/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <gio/gio.h>

#include "xb-builder.h"
#include "xb-silo-export.h"
#include "xb-silo-query.h"

static void
xb_builder_func (void)
{
	g_autofree gchar *str = NULL;
	g_autofree gchar *xml_new = NULL;
	g_autoptr(GBytes) bytes = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
		"<components origin=\"lvfs\">\n"
		"  <header type=\"&lt;&amp;&gt;\">\n"
		"    <csum type=\"sha1\">dead</csum>\n"
		"  </header>\n"
		"  <component type=\"desktop\" attr=\"value\">\n"
		"    <id>gimp.desktop</id>\n"
		"    <name>GIMP &amp; Friends</name>\n"
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
	silo = xb_silo_new_from_xml (xml, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* convert back to XML */
	str = xb_silo_to_string (silo, &error);
	g_assert_no_error (error);
	g_assert_nonnull (str);
	g_debug ("\n%s", str);
	xml_new = xb_silo_export (silo,
				  XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE |
				  XB_NODE_EXPORT_FLAG_FORMAT_INDENT,
				  &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml_new);
	g_print ("%s", xml_new);
	g_assert_cmpstr (xml, ==, xml_new);

	/* check size */
	bytes = xb_silo_get_bytes (silo);
	g_assert_cmpint (g_bytes_get_size (bytes), ==, 529);
}

static void
xb_builder_empty_func (void)
{
	gboolean ret;
	g_autofree gchar *str = NULL;
	g_autofree gchar *xml = NULL;
	g_autoptr(GBytes) bytes = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbSilo) silo2 = xb_silo_new ();
	g_autoptr(XbSilo) silo = NULL;

	/* import from XML */
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* check size */
	bytes = xb_silo_get_bytes (silo);
	g_assert_cmpint (g_bytes_get_size (bytes), ==, 32);

	/* try to dump */
	str = xb_silo_to_string (silo, &error);
	g_assert_no_error (error);
	g_assert_nonnull (str);
	g_debug ("%s", str);

	/* try to export */
	xml = xb_silo_export (silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null (results);
	g_clear_error (&error);

	/* try to query empty silo */
	results = xb_silo_query (silo, "components/component", 0, &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null (results);
	g_clear_error (&error);

	/* load blob */
	g_assert_nonnull (bytes);
	ret = xb_silo_load_from_bytes (silo2, bytes, 0, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
}

static void
xb_xpath_func (void)
{
	XbNode *n;
	XbNode *n2;
	g_autofree gchar *str = NULL;
	g_autofree gchar *xml_sub1 = NULL;
	g_autofree gchar *xml_sub2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbNode) n3 = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
	"<components origin=\"lvfs\">\n"
	"  <header>\n"
	"    <csum type=\"sha1\">dead</csum>\n"
	"  </header>\n"
	"  <component type=\"desktop\">\n"
	"    <id>gimp.desktop</id>\n"
	"    <id>org.gnome.Gimp.desktop</id>\n"
	"  </component>\n"
	"  <component type=\"firmware\">\n"
	"    <id>org.hughski.ColorHug2.firmware</id>\n"
	"  </component>\n"
	"</components>\n";

	/* import from XML */
	silo = xb_silo_new_from_xml (xml, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* dump to screen */
	str = xb_silo_to_string (silo, &error);
	g_assert_no_error (error);
	g_assert_nonnull (str);
	g_debug ("\n%s", str);

	/* query that doesn't find anything */
	n = xb_silo_query_first (silo, "dave", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null (n);
	g_clear_error (&error);
	g_clear_object (&n);

	n = xb_silo_query_first (silo, "dave/dave", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null (n);
	g_clear_error (&error);
	g_clear_object (&n);

	n = xb_silo_query_first (silo, "components/dave", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null (n);
	g_clear_error (&error);
	g_clear_object (&n);

	n = xb_silo_query_first (silo, "components/component[@type=dave]/id", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null (n);
	g_clear_error (&error);
	g_clear_object (&n);

	n = xb_silo_query_first (silo, "components/component/id[dave]", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null (n);
	g_clear_error (&error);
	g_clear_object (&n);

	/* query with attr predicate */
	n = xb_silo_query_first (silo, "components/component[@type=firmware]/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object (&n);

	/* query with text predicate */
	n = xb_silo_query_first (silo, "components/header/csum[dead]", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_attr (n, "type"), ==, "sha1");
	g_clear_object (&n);

	/* query with backtrack */
	g_debug ("\n%s", xml);
	n = xb_silo_query_first (silo, "components/component[@type=firmware]/id[org.hughski.ColorHug2.firmware]", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object (&n);

	/* query for multiple results */
	results = xb_silo_query (silo, "components/component/id", 5, &error);
	g_assert_no_error (error);
	g_assert_nonnull (results);
	g_assert_cmpint (results->len, ==, 3);
	n2 = g_ptr_array_index (results, 2);
	g_assert_cmpstr (xb_node_get_text (n2), ==, "org.hughski.ColorHug2.firmware");

	/* subtree export */
	xml_sub1 = xb_node_export (n2, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml_sub1);
	g_assert_cmpstr (xml_sub1, ==, "<id>org.hughski.ColorHug2.firmware</id>");

	/* parent of subtree */
	n3 = xb_node_get_parent (n2);
	g_assert (n3 != NULL);
	xml_sub2 = xb_node_export (n3, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml_sub2);
	g_assert_cmpstr (xml_sub2, ==, "<component type=\"firmware\"><id>org.hughski.ColorHug2.firmware</id></component>");
}

static void
xb_xpath_parent_func (void)
{
	XbNode *n;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
	"<components origin=\"lvfs\">\n"
	"  <header>\n"
	"    <csum type=\"sha1\">dead</csum>\n"
	"  </header>\n"
	"  <component type=\"desktop\">\n"
	"    <id>gimp.desktop</id>\n"
	"    <id>org.gnome.Gimp.desktop</id>\n"
	"  </component>\n"
	"  <component type=\"firmware\">\n"
	"    <id>org.hughski.ColorHug2.firmware</id>\n"
	"  </component>\n"
	"</components>\n";

	/* import from XML */
	silo = xb_silo_new_from_xml (xml, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* get node, no parent */
	n = xb_silo_query_first (silo, "components/component[@type=firmware]/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "org.hughski.ColorHug2.firmware");
	g_assert_cmpstr (xb_node_get_element (n), ==, "id");
	g_clear_object (&n);

	/* get node, one parent */
	n = xb_silo_query_first (silo, "components/component[@type=firmware]/id/..", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_element (n), ==, "component");
	g_clear_object (&n);

	/* get node, multiple parents */
	n = xb_silo_query_first (silo, "components/component[@type=firmware]/id/../..", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_element (n), ==, "components");
	g_clear_object (&n);

	/* get node, too many parents */
	n = xb_silo_query_first (silo, "components/component[@type=firmware]/id/../../..", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
	g_assert_null (n);
}

static void
xb_xpath_glob_func (void)
{
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
	"<components origin=\"lvfs\">\n"
	"  <component type=\"desktop\">\n"
	"    <id>gimp.desktop</id>\n"
	"    <id>org.gnome.GIMP.desktop</id>\n"
	"  </component>\n"
	"</components>\n";

	/* import from XML */
	silo = xb_silo_new_from_xml (xml, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* get node, no parent */
	n = xb_silo_query_first (silo, "components/component[@type=desktop]/*", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_element (n), ==, "id");

	/* export this one node */
	xml2 = xb_node_export (n, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (xml2, ==, "<id>gimp.desktop</id>");
}

static void
xb_builder_multiple_roots_func (void)
{
	gboolean ret;
	g_autofree gchar *str = NULL;
	g_autofree gchar *xml_new = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbBuilder) builder = NULL;
	g_autoptr(XbSilo) silo = NULL;

	/* import from XML */
	builder = xb_builder_new ();
	ret = xb_builder_import_xml (builder, "<tag>value</tag>", &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	ret = xb_builder_import_xml (builder, "<tag>value2</tag>", &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* convert back to XML */
	str = xb_silo_to_string (silo, &error);
	g_assert_no_error (error);
	g_assert_nonnull (str);
	g_debug ("\n%s", str);
	xml_new = xb_silo_export (silo, XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml_new);
	g_print ("%s", xml_new);
	g_assert_cmpstr ("<tag>value</tag><tag>value2</tag>", ==, xml_new);

	/* query for multiple results */
	results = xb_silo_query (silo, "tag", 5, &error);
	g_assert_no_error (error);
	g_assert_nonnull (results);
	g_assert_cmpint (results->len, ==, 2);
}

static void
xb_speed_func (void)
{
	XbNode *n;
	const gchar *fn = "/tmp/test.xmlb";
	gboolean ret;
	guint n_components = 10000;
	g_autofree gchar *xpath1 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(GString) xml = g_string_new (NULL);
	g_autoptr(GTimer) timer = g_timer_new ();
	g_autoptr(XbSilo) silo = NULL;

	/* create a huge document */
	g_string_append (xml, "<components>");
	for (guint i = 0; i < n_components; i++) {
		g_string_append (xml, "<component>");
		g_string_append_printf (xml, "  <id>%06u.firmware</id>", i);
		g_string_append (xml, "  <name>ColorHug2</name>");
		g_string_append (xml, "  <summary>Firmware</summary>");
		g_string_append (xml, "  <description><p>New features!</p></description>");
		g_string_append (xml, "  <provides>");
		g_string_append (xml, "    <firmware type=\"flashed\">2082b5e0</firmware>");
		g_string_append (xml, "  </provides>");
		g_string_append (xml, "  <requires>");
		g_string_append (xml, "    <id compare=\"ge\" version=\"0.8.0\">fwupd</id>");
		g_string_append (xml, "    <firmware compare=\"eq\" version=\"2.0.99\"/>");
		g_string_append (xml, "  </requires>");
		g_string_append (xml, "  <url type=\"homepage\">http://com/</url>");
		g_string_append (xml, "  <metadata_license>CC0-1.0</metadata_license>");
		g_string_append (xml, "  <project_license>GPL-2.0+</project_license>");
		g_string_append (xml, "  <updatecontact>richard</updatecontact>");
		g_string_append (xml, "  <developer_name>Hughski</developer_name>");
		g_string_append (xml, "  <releases>");
		g_string_append (xml, "    <release urgency=\"medium\" version=\"2.0.3\" timestamp=\"1429362707\">");
		g_string_append (xml, "      <description><p>stable:</p><ul><li>Quicker</li></ul></description>");
		g_string_append (xml, "    </release>");
		g_string_append (xml, "  </releases>");
		g_string_append (xml, "</component>");
	}
	g_string_append (xml, "</components>");

	/* import from XML */
	silo = xb_silo_new_from_xml (xml->str, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);
	ret = xb_silo_save_to_file (silo, fn, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_clear_object (&silo);
	g_print ("import+save: %.3fms\n", g_timer_elapsed (timer, NULL) * 1000);
	g_timer_reset (timer);

	/* load from file */
	silo = xb_silo_new ();
	ret = xb_silo_load_from_file (silo, fn, XB_SILO_LOAD_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_print ("mmap load: %.3fms\n", g_timer_elapsed (timer, NULL) * 1000);
	g_timer_reset (timer);

	/* query best case */
	n = xb_silo_query_first (silo, "components/component/id[000000.firmware]", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_print ("query[first]: %.3fms\n", g_timer_elapsed (timer, NULL) * 1000);
	g_timer_reset (timer);
	g_clear_object (&n);

	/* query worst case */
	xpath1 = g_strdup_printf ("components/component/id[%06u.firmware]", n_components - 1);
	n = xb_silo_query_first (silo, xpath1, &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_print ("query[last]: %.3fms\n", g_timer_elapsed (timer, NULL) * 1000);
	g_timer_reset (timer);
	g_clear_object (&n);

	/* query all components */
	results = xb_silo_query (silo, "components/component", 0, &error);
	g_assert_no_error (error);
	g_assert_nonnull (results);
	g_assert_cmpint (results->len, ==, n_components);
	g_print ("query[all]: %.3fms\n", g_timer_elapsed (timer, NULL) * 1000);
	g_timer_reset (timer);

	/* factorial search */
	for (guint i = 0; i < n_components; i += 20) {
		g_autofree gchar *xpath2 = NULL;
		xpath2 = g_strdup_printf ("components/component/id[%06u.firmware]", i);
		n = xb_silo_query_first (silo, xpath2, &error);
		g_assert_no_error (error);
		g_assert_nonnull (n);
		g_clear_object (&n);
	}
	g_print ("query[x%u]: %.3fms\n", n_components, g_timer_elapsed (timer, NULL) * 1000);
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

	/* tests go here */
	g_test_add_func ("/libxmlb/builder", xb_builder_func);
	g_test_add_func ("/libxmlb/builder{empty}", xb_builder_empty_func);
	g_test_add_func ("/libxmlb/xpath", xb_xpath_func);
	g_test_add_func ("/libxmlb/xpath-parent", xb_xpath_parent_func);
	g_test_add_func ("/libxmlb/xpath-glob", xb_xpath_glob_func);
	g_test_add_func ("/libxmlb/multiple-roots", xb_builder_multiple_roots_func);
	g_test_add_func ("/libxmlb/speed", xb_speed_func);
	return g_test_run ();
}
