/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>

#include "xb-silo-export.h"
#include "xb-silo-query.h"
#include "xb-stack-private.h"
#include "xb-test-common.h"

static void
xb_builder_node_as_uint_func(void)
{
	g_autoptr(XbBuilderNode) n = xb_builder_node_new("value");

	/* decimal values must parse unsigned, like the hex values */
	xb_builder_node_set_attr(n, "hex", "0x20");
	xb_builder_node_set_attr(n, "big", "10000000000000000000");
	xb_builder_node_set_text(n, "10000000000000000000", -1);
	g_assert_cmpuint(xb_builder_node_get_attr_as_uint(n, "hex"), ==, 32);
	g_assert_true(xb_builder_node_get_attr_as_uint(n, "big") ==
		      G_GUINT64_CONSTANT(10000000000000000000));
	g_assert_true(xb_builder_node_get_text_as_uint(n) ==
		      G_GUINT64_CONSTANT(10000000000000000000));
}

static void
xb_builder_node_export_collapse_func(void)
{
	g_autofree gchar *xml = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilderNode) n = xb_builder_node_new("empty");

	/* getting the children lazily allocates the array, which must not
	 * stop the node being collapsed on export */
	g_assert_nonnull(xb_builder_node_get_children(n));
	g_assert_cmpuint(xb_builder_node_get_children(n)->len, ==, 0);

	xml = xb_builder_node_export(n, XB_NODE_EXPORT_FLAG_COLLAPSE_EMPTY, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml);
	g_assert_cmpstr("<empty />", ==, xml);
}

static void
xb_builder_node_token_max_func(void)
{
	g_autofree gchar *xml = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderNode) components = NULL;
	g_autoptr(XbBuilderNode) root = xb_builder_node_new(NULL);
	g_autoptr(XbSilo) silo = NULL;

	/* create a simple document */
	components = xb_builder_node_insert(root, "components", NULL);
	for (guint i = 0; i < XB_OPCODE_TOKEN_MAX * 2; i++) {
		g_autofree gchar *tmp = g_strdup_printf("foobarbaz%04u", i);
		xb_builder_node_add_token(components, tmp);
	}

	/* import the doc */
	xb_builder_import_node(builder, root);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* check the XML */
	xml = xb_silo_export(silo, XB_NODE_EXPORT_FLAG_COLLAPSE_EMPTY, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml);
	g_assert_cmpstr("<components />", ==, xml);
}

static void
xb_builder_node_nul_func(void)
{
	const gchar buf[] = {'A', '\n', 'B'};
	g_autoptr(XbBuilderNode) root = xb_builder_node_new(NULL);

	xb_builder_node_set_text(root, "\n", 0);
	g_assert_cmpstr(xb_builder_node_get_text(root), ==, NULL);
	xb_builder_node_set_text(root, "dave", -1);
	g_assert_cmpstr(xb_builder_node_get_text(root), ==, "dave");
	xb_builder_node_set_text(root, buf, sizeof(buf));
	g_assert_cmpstr(xb_builder_node_get_text(root), ==, "A B");
}

static void
xb_builder_node_func(void)
{
	g_autofree gchar *xml = NULL;
	g_autofree gchar *xml_src = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderNode) child_by_element = NULL;
	g_autoptr(XbBuilderNode) child_by_text = NULL;
	g_autoptr(XbBuilderNode) component = NULL;
	g_autoptr(XbBuilderNode) components = NULL;
	g_autoptr(XbBuilderNode) id = NULL;
	g_autoptr(XbBuilderNode) description = xb_builder_node_new("description");
	g_autoptr(XbBuilderNode) em = xb_builder_node_new("em");
	g_autoptr(XbBuilderNode) empty = NULL;
	g_autoptr(XbBuilderNode) root = xb_builder_node_new(NULL);
	g_autoptr(XbSilo) silo = NULL;

	/* create a simple document */
	components = xb_builder_node_insert(root, "components", "origin", "lvfs", NULL);
	g_assert_cmpint(xb_builder_node_depth(components), ==, 1);
	component = xb_builder_node_insert(components, "component", NULL);
	g_assert_cmpint(xb_builder_node_depth(component), ==, 2);
	xb_builder_node_set_attr(component, "type", "firmware");
	xb_builder_node_set_attr(component, "type", "desktop");
	g_assert_cmpstr(xb_builder_node_get_attr(component, "type"), ==, "desktop");
	g_assert_cmpstr(xb_builder_node_get_attr(component, "dave"), ==, NULL);
	id = xb_builder_node_new("id");
	xb_builder_node_add_flag(id, XB_BUILDER_NODE_FLAG_TOKENIZE_TEXT);
	xb_builder_node_add_token(id, "foobarbaz");
	xb_builder_node_add_child(component, id);
	xb_builder_node_set_text(id, "gimp.desktop", -1);
	xb_builder_node_insert_text(component, "icon", "dave", "type", "stock", NULL);
	g_assert_cmpint(xb_builder_node_depth(id), ==, 3);

	xb_builder_node_add_flag(em, XB_BUILDER_NODE_FLAG_LITERAL_TEXT);
	xb_builder_node_set_text(em, "world!", -1);
	xb_builder_node_set_tail(em, "    ", -1);
	xb_builder_node_add_child(description, em);
	xb_builder_node_set_text(description, "hello ", -1);
	xb_builder_node_add_child(component, description);

	/* no text contents */
	empty = xb_builder_node_insert(component, "empty", NULL);
	xb_builder_node_set_text(empty, NULL, -1);
	xb_builder_node_set_tail(empty, NULL, -1);

	/* get specific child */
	child_by_element = xb_builder_node_get_child(components, "component", NULL);
	g_assert_nonnull(child_by_element);
	g_assert_cmpstr(xb_builder_node_get_element(child_by_element), ==, "component");
	child_by_text = xb_builder_node_get_child(component, "id", "gimp.desktop");
	g_assert_nonnull(child_by_text);
	g_assert_cmpstr(xb_builder_node_get_element(child_by_text), ==, "id");

	/* check the source XML */
	xml_src = xb_builder_node_export(components,
					 XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE |
					     XB_NODE_EXPORT_FLAG_COLLAPSE_EMPTY,
					 &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml_src);
	g_print("%s", xml_src);
	g_assert_cmpstr("<components origin=\"lvfs\">\n"
			"<component type=\"desktop\">\n"
			"<id>gimp.desktop</id>\n"
			"<icon type=\"stock\">dave</icon>\n"
			"<description>hello <em>world!</em>    \n"
			"</description>\n"
			"<empty />\n"
			"</component>\n"
			"</components>\n",
			==,
			xml_src);
	/* import the doc */
	xb_builder_import_node(builder, root);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* check the XML */
	xml = xb_silo_export(silo,
			     XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS |
				 XB_NODE_EXPORT_FLAG_COLLAPSE_EMPTY,
			     &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml);
	g_print("%s", xml);
	g_assert_cmpstr("<components origin=\"lvfs\">"
			"<component type=\"desktop\">"
			"<id>gimp.desktop</id>"
			"<icon type=\"stock\">dave</icon>"
			"<description>hello <em>world!</em>"
			"</description>"
			"<empty />"
			"</component>"
			"</components>",
			==,
			xml);
}

static void
xb_builder_node_literal_text_func(void)
{
	gboolean ret;
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "  <component>\n"
			   "    <description>\n"
			   "      <p>Really long content\n"
			   "spanning multiple lines\n"
			   "</p>\n"
			   "    </description>\n"
			   "  </component>\n";

	/* import some XML */
	ret = xb_builder_source_load_xml(source, xml, XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT, &error);
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
	g_assert_cmpstr(
	    "<component>"
	    "<description><p>Really long content\nspanning multiple lines\n</p></description>"
	    "</component>",
	    ==,
	    xml2);
}

static void
xb_builder_node_source_text_func(void)
{
	gboolean ret;
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "  <component>\n"
			   "    <description>\n"
			   "      <p>Really long content\n"
			   "spanning multiple lines\n"
			   "</p>\n"
			   "    </description>\n"
			   "  </component>\n";

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
	g_assert_cmpstr(
	    "<component>"
	    "<description><p>Really long content spanning multiple lines</p></description>"
	    "</component>",
	    ==,
	    xml2);
}

static void
xb_builder_node_info_func(void)
{
	gboolean ret;
	g_autofree gchar *xml = NULL;
	g_autofree gchar *tmp_xml =
	    g_build_filename(g_get_tmp_dir(), "xb-test-builder-node-info.xml", NULL);
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilderSource) import1 = xb_builder_source_new();
	g_autoptr(XbBuilderSource) import2 = xb_builder_source_new();
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbBuilderNode) info1 = NULL;
	g_autoptr(XbBuilderNode) info2 = NULL;
	g_autoptr(XbSilo) silo = NULL;
	g_autoptr(GFile) file = NULL;

	/* create a simple document with some info */
	ret = g_file_set_contents(tmp_xml,
				  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				  "<component><id type=\"desktop\">dave</id></component>",
				  -1,
				  &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	info1 = xb_builder_node_insert(NULL, "info", NULL);
	xb_builder_node_insert_text(info1, "scope", "user", NULL);
	info2 = xb_builder_node_insert(NULL, "info", NULL);
	xb_builder_node_insert_text(info2, "scope", "system", NULL);

	/* import the doc */
	file = g_file_new_for_path(tmp_xml);
	ret = xb_builder_source_load_file(import1, file, XB_BUILDER_SOURCE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_source_set_info(import1, info1);
	xb_builder_source_set_prefix(import1, "local");
	xb_builder_import_source(builder, import1);
	ret = xb_builder_source_load_file(import2, file, XB_BUILDER_SOURCE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_source_set_info(import2, info2);
	xb_builder_source_set_prefix(import2, "local");
	xb_builder_import_source(builder, import2);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* get info */
	n = xb_silo_query_first(silo, "local/component/id[text()='dave']/../info/scope", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "user");

	/* check the XML */
	xml = xb_silo_export(silo, XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml);
	g_assert_cmpstr("<local>"
			"<component>"
			"<id type=\"desktop\">dave</id>"
			"<info>"
			"<scope>user</scope>"
			"</info>"
			"</component>"
			"<component>"
			"<id type=\"desktop\">dave</id>"
			"<info>"
			"<scope>system</scope>"
			"</info>"
			"</component>"
			"</local>",
			==,
			xml);
}

static void
xb_builder_node_info_flag_func(void)
{
	g_autoptr(XbBuilderNode) info = xb_builder_node_new("info");
	g_autoptr(XbBuilderNode) other = xb_builder_node_new("other");
	g_autoptr(XbBuilderNode) child = xb_builder_node_new("child");
	g_autoptr(XbBuilderNode) grandchild = xb_builder_node_new("grandchild");
	g_autoptr(XbBuilderNode) child2 = xb_builder_node_new("child2");

	/* a child added to an info node after it was flagged is info too */
	xb_builder_node_add_flag(info, XB_BUILDER_NODE_FLAG_IS_INFO);
	xb_builder_node_add_child(child, grandchild);
	g_assert_false(xb_builder_node_has_flag(child, XB_BUILDER_NODE_FLAG_IS_INFO));
	xb_builder_node_add_child(info, child);
	g_assert_true(xb_builder_node_has_flag(child, XB_BUILDER_NODE_FLAG_IS_INFO));
	g_assert_true(xb_builder_node_has_flag(grandchild, XB_BUILDER_NODE_FLAG_IS_INFO));

	/* but not when the parent is not info */
	xb_builder_node_add_child(other, child2);
	g_assert_false(xb_builder_node_has_flag(child2, XB_BUILDER_NODE_FLAG_IS_INFO));
}

void
xb_builder_node_test_add_funcs(void)
{
	g_test_add_func("/libxmlb/builder-node", xb_builder_node_func);
	g_test_add_func("/libxmlb/builder-node/as-uint", xb_builder_node_as_uint_func);
	g_test_add_func("/libxmlb/builder-node/export-collapse",
			xb_builder_node_export_collapse_func);
	g_test_add_func("/libxmlb/builder-node/nul", xb_builder_node_nul_func);
	g_test_add_func("/libxmlb/builder-node/token-max", xb_builder_node_token_max_func);
	g_test_add_func("/libxmlb/builder-node/info", xb_builder_node_info_func);
	g_test_add_func("/libxmlb/builder-node/info-flag", xb_builder_node_info_flag_func);
	g_test_add_func("/libxmlb/builder-node/literal-text", xb_builder_node_literal_text_func);
	g_test_add_func("/libxmlb/builder-node/source-text", xb_builder_node_source_text_func);
}
