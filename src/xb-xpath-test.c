/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>

#include "xb-node-query.h"
#include "xb-silo-private.h"
#include "xb-silo-query.h"
#include "xb-test-common.h"

static void
xb_xpath_node_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<components origin=\"lvfs\">\n"
			   "  <component type=\"desktop\">\n"
			   "    <id>gimp.desktop</id>\n"
			   "    <id>org.gnome.Gimp.desktop</id>\n"
			   "  </component>\n"
			   "  <component type=\"firmware\">\n"
			   "    <id>org.hughski.ColorHug2.firmware</id>\n"
			   "  </component>\n"
			   "</components>\n";

	/* import from XML */
	silo = xb_silo_new_from_xml(xml, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* get node */
	n = xb_silo_query_first(silo, "components/component", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_attr(n, "type"), ==, "desktop");

	/* query with text opcodes */
	results = xb_node_query(n, "id", 0, &error);
	g_assert_no_error(error);
	g_assert_nonnull(results);
	g_assert_cmpint(results->len, ==, 2);
}

static void
xb_xpath_parent_subnode_func(void)
{
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) children = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbNode) c = NULL;
	g_autoptr(XbNode) p = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<components origin=\"lvfs\">\n"
			   "  <component type=\"desktop\">\n"
			   "    <id>gimp.desktop</id>\n"
			   "    <id>org.gnome.Gimp.desktop</id>\n"
			   "  </component>\n"
			   "  <component type=\"firmware\">\n"
			   "    <id>org.hughski.ColorHug2.firmware</id>\n"
			   "  </component>\n"
			   "</components>\n";

	/* import from XML */
	silo = xb_silo_new_from_xml(xml, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);
	xb_silo_set_enable_node_cache(silo, TRUE);

	/* get node */
	n = xb_silo_query_first(silo, "components/component", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_attr(n, "type"), ==, "desktop");
	g_assert_cmpint(xb_node_get_depth(n), ==, 1);

	/* export a child */
	xml2 = xb_node_query_export(n, "id", &error);
	g_assert_cmpstr(xml2, ==, "<id>gimp.desktop</id>");

	/* get sibling */
	c = xb_node_get_next(n);
	g_assert_nonnull(c);
	g_assert_cmpstr(xb_node_get_attr(c, "type"), ==, "firmware");
	p = xb_node_get_next(c);
	g_assert_null(p);
	g_clear_object(&c);

	/* use the node to go back up the tree */
	c = xb_node_query_first(n, "..", &error);
	g_assert_no_error(error);
	g_assert_nonnull(c);
	g_assert_cmpstr(xb_node_get_attr(c, "origin"), ==, "lvfs");

	/* verify this is the parent */
	p = xb_node_get_root(n);
	g_assert_cmpint(xb_node_get_depth(p), ==, 0);
	g_assert(c == p);
	children = xb_node_get_children(p);
	g_assert_nonnull(children);
	g_assert_cmpint(children->len, ==, 2);
}

static void
xb_xpath_helpers_func(void)
{
	const gchar *tmp;
	guint64 val;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;

	/* import from XML */
	silo = xb_silo_new_from_xml("<release><checksum size=\"123\">456</checksum></release>",
				    &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* as char */
	n = xb_silo_get_root(silo);
	g_assert_nonnull(n);
	tmp = xb_node_query_text(n, "checksum", &error);
	g_assert_no_error(error);
	g_assert_cmpstr(tmp, ==, "456");
	tmp = xb_node_query_attr(n, "checksum", "size", &error);
	g_assert_no_error(error);
	g_assert_cmpstr(tmp, ==, "123");

	/* as uint64 */
	val = xb_node_query_text_as_uint(n, "checksum", &error);
	g_assert_no_error(error);
	g_assert_cmpint(val, ==, 456);
	val = xb_node_query_attr_as_uint(n, "checksum", "size", &error);
	g_assert_no_error(error);
	g_assert_cmpint(val, ==, 123);
}

static void
xb_xpath_query_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<components>\n"
			   "  <component timestamp=\"1631923200\">\n"
			   "    <id>n/a</id>\n"
			   "  </component>\n"
			   "</components>\n";

	/* import from XML */
	silo = xb_silo_new_from_xml(xml, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* query with slash */
	n = xb_silo_query_first(silo, "components/component/id[text()='n\\/a']", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "n/a");
	g_clear_object(&n);

	/* query with an OR, where the first section contains an unknown element */
	n = xb_silo_query_first(silo, "components/dave|components/component/id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "n/a");
	g_clear_object(&n);

	/* query with an OR, where the last section contains an unknown element */
	n = xb_silo_query_first(silo, "components/component/id|components/dave", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "n/a");
	g_clear_object(&n);

	/* query using an integer */
	n = xb_silo_query_first(silo, "components/component[@timestamp=1631923200]", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_attr(n, "timestamp"), ==, "1631923200");
	g_clear_object(&n);

	/* query with an OR, all sections contains an unknown element */
	n = xb_silo_query_first(silo, "components/dave|components/mike", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
}

static void
xb_xpath_incomplete_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<components>\n"
			   "  <component>\n"
			   "    <id>gimp.desktop</id>\n"
			   "  </component>\n"
			   "</components>\n";

	/* import from XML */
	silo = xb_silo_new_from_xml(xml, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* query with MISSING '[' */
	n = xb_silo_query_first(silo, "components/component/id[text()='dave'", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
	g_assert_null(n);
}

static gboolean
xb_builder_fixup_strip_inner_cb(XbBuilderFixup *self,
				XbBuilderNode *bn,
				gpointer user_data,
				GError **error)
{
	if (xb_builder_node_get_first_child(bn) == NULL)
		xb_builder_node_add_flag(bn, XB_BUILDER_NODE_FLAG_STRIP_TEXT);
	return TRUE;
}

static void
xb_xpath_func(void)
{
	XbNode *n;
	XbNode *n2;
	gboolean ret;
	g_autofree gchar *str = NULL;
	g_autofree gchar *xml_sub1 = NULL;
	g_autofree gchar *xml_sub2 = NULL;
	g_autofree gchar *xml_sub3 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbNode) n3 = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderFixup) fixup = NULL;
	g_autoptr(XbBuilderFixup) fixup2 = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<components origin=\"lvfs\">\n"
			   "  <header>\n"
			   "    <csum type=\"sha1\">dead</csum>\n"
			   "  </header>\n"
			   "  <component type=\"desktop\">\n"
			   "    <id> gimp.desktop </id>\n"
			   "    <id>org.gnome.Gimp.desktop</id>\n"
			   "    <name>Mêẞ</name>\n"
			   "    <custom>\n"
			   "      <value key=\"KEY\">TRUE</value>\n"
			   "    </custom>\n"
			   "  </component>\n"
			   "  <component type=\"firmware\">\n"
			   "    <id>org.hughski.ColorHug2.firmware</id>\n"
			   "  </component>\n"
			   "</components>\n";

	/* tokenize specific fields */
	fixup = xb_builder_fixup_new("TextTokenize", xb_test_fixup_tokenize_cb, NULL, NULL);
	xb_builder_source_add_fixup(source, fixup);

	/* strip inner nodes without children */
	fixup2 =
	    xb_builder_fixup_new("TextStripInner", xb_builder_fixup_strip_inner_cb, NULL, NULL);
	xb_builder_source_add_fixup(source, fixup2);

	/* import from XML */
	ret = xb_builder_source_load_xml(source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* set up debugging */
	xb_machine_set_debug_flags(xb_silo_get_machine(silo), XB_MACHINE_DEBUG_FLAG_SHOW_STACK);

	/* dump to screen */
	str = xb_silo_to_string(silo, &error);
	g_assert_no_error(error);
	g_assert_nonnull(str);
	g_debug("\n%s", str);

	/* query with predicate 'in' */
	xb_silo_set_profile_flags(silo, XB_SILO_PROFILE_FLAG_OPTIMIZER);
	n = xb_silo_query_first(
	    silo,
	    "components/component/id[text()=('gimp.desktop','another.desktop','dave.desktop')]",
	    &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with predicate 'in' -- single-entry tuple */
	xb_silo_set_profile_flags(silo, XB_SILO_PROFILE_FLAG_OPTIMIZER);
	n = xb_silo_query_first(silo, "components/component/id[text()=('gimp.desktop')]", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with predicate logical and */
	n = xb_silo_query_first(
	    silo,
	    "components/component/custom/value[(@key='KEY') and (text()='TRUE')]/../../id",
	    &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with predicate logical and; failure */
	n = xb_silo_query_first(
	    silo,
	    "components/component/custom/value[(@key='KEY')&&(text()='FALSE')]/../../id",
	    &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);
	g_clear_object(&n);

	/* query with predicate logical and, alternate form */
	n = xb_silo_query_first(
	    silo,
	    "components/component/custom/value[and((@key='KEY'),(text()='TRUE'))]/../../id",
	    &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query that doesn't find anything */
	n = xb_silo_query_first(silo, "dave", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);
	g_clear_object(&n);

	n = xb_silo_query_first(silo, "dave/dave", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);
	g_clear_object(&n);

	n = xb_silo_query_first(silo, "components/dave", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);
	g_clear_object(&n);

	n = xb_silo_query_first(silo, "components/component[@type='dave']/id", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);
	g_clear_object(&n);

	n = xb_silo_query_first(silo, "components/component[@percentage>=90]", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);
	g_clear_object(&n);

	n = xb_silo_query_first(silo, "components/component/id[text()='dave']", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);
	g_clear_object(&n);

	/* query with attr opcodes */
	n = xb_silo_query_first(silo, "components/component[@type='firmware']/id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object(&n);

	/* query with attr opcodes */
	n = xb_silo_query_first(silo, "components/component[@type!='firmware']/id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with attr opcodes with quotes */
	n = xb_silo_query_first(silo, "components/component[@type='firmware']/id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object(&n);

	/* query with position */
	n = xb_silo_query_first(silo, "components/component[2]/id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object(&n);

	/* last() with position */
	n = xb_silo_query_first(silo, "components/component[last()]/id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object(&n);

	/* query with attr opcodes that exists */
	n = xb_silo_query_first(silo, "components/component[@type]/id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with attrs that do not exist */
	n = xb_silo_query_first(silo, "components/component[not(@dave)]/id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with wildcard with predicate */
	n = xb_silo_query_first(silo, "components/*[@type]/id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with text opcodes */
	n = xb_silo_query_first(silo, "components/header/csum[text()='dead']", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_attr(n, "type"), ==, "sha1");
	g_clear_object(&n);

	/* query with search */
	n = xb_silo_query_first(silo, "components/component/id[text()~='gimp']", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query no normalize */
	n = xb_silo_query_first(silo, "components/component/name", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "Mêẞ");
	g_clear_object(&n);

	/* query name not UTF-8 */
	n = xb_silo_query_first(silo, "components/component/name[text()~='mEss']/../id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with stem */
#ifdef HAVE_LIBSTEMMER
	xb_machine_set_debug_flags(xb_silo_get_machine(silo),
				   XB_MACHINE_DEBUG_FLAG_SHOW_STACK |
				       XB_MACHINE_DEBUG_FLAG_SHOW_PARSING);
	n = xb_silo_query_first(silo, "components/component/id[text()~=stem('gimping')]", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);
	xb_machine_set_debug_flags(xb_silo_get_machine(silo), XB_MACHINE_DEBUG_FLAG_SHOW_STACK);
#endif

	/* query with text:integer */
	n = xb_silo_query_first(silo, "components/component/id['123'=123]", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with integer:text */
	n = xb_silo_query_first(silo, "components/component/id[123='123']", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with prefix */
	n = xb_silo_query_first(silo,
				"components/component/id[starts-with(text(),'gimp')]",
				&error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with suffix */
	n = xb_silo_query_first(silo,
				"components/component/id[ends-with(text(),'.desktop')]",
				&error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with contains */
	n = xb_silo_query_first(silo, "components/component/id[contains(text(),'imp')]", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with type-conversion */
	n = xb_silo_query_first(silo, "components/component[position()=number('2')]/id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object(&n);

	/* query with another type-conversion */
	n = xb_silo_query_first(silo, "components/component['2'=string(2)]/id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query with backtrack */
	g_debug("\n%s", xml);
	n = xb_silo_query_first(
	    silo,
	    "components/component[@type='firmware']/id[text()='org.hughski.ColorHug2.firmware']",
	    &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object(&n);

	/* query with nesting */
	g_debug("\n%s", xml);
	n = xb_silo_query_first(
	    silo,
	    "components/component/id[text()=lower-case(upper-case('Gimp.DESKTOP'))]",
	    &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "gimp.desktop");
	g_clear_object(&n);

	/* query for multiple results */
	results = xb_silo_query(silo, "components/component/id", 5, &error);
	g_assert_no_error(error);
	g_assert_nonnull(results);
	g_assert_cmpint(results->len, ==, 3);
	n2 = g_ptr_array_index(results, 2);
	g_assert_cmpstr(xb_node_get_text(n2), ==, "org.hughski.ColorHug2.firmware");

	/* subtree export */
	xml_sub1 = xb_node_export(n2, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml_sub1);
	g_assert_cmpstr(xml_sub1, ==, "<id>org.hughski.ColorHug2.firmware</id>");

	/* parent of subtree */
	n3 = xb_node_get_parent(n2);
	g_assert(n3 != NULL);
	xml_sub2 = xb_node_export(n3, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml_sub2);
	g_assert_cmpstr(
	    xml_sub2,
	    ==,
	    "<component type=\"firmware\"><id>org.hughski.ColorHug2.firmware</id></component>");

	/* only children of parent */
	xml_sub3 = xb_node_export(n3, XB_NODE_EXPORT_FLAG_ONLY_CHILDREN, &error);
	g_assert_no_error(error);
	g_assert_nonnull(xml_sub3);
	g_assert_cmpstr(xml_sub3, ==, "<id>org.hughski.ColorHug2.firmware</id>");
}

static void
xb_xpath_null_attr_func(void)
{
	XbNode *n;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<component type=\"desktop\">\n"
			   "  <id>org.gnome.Software</id>\n"
			   "</component>\n";

	ret = xb_builder_source_load_xml(source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* lower-case with nonexistent attr should not crash */
	n = xb_silo_query_first(silo, "component[lower-case(attr('nonexistent'))='x']", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);

	/* upper-case with nonexistent attr should not crash */
	n = xb_silo_query_first(silo, "component[upper-case(attr('nonexistent'))='x']", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);

	/* starts-with with nonexistent attr should not crash */
	n = xb_silo_query_first(silo, "component[starts-with(attr('nonexistent'),'x')]", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);

	/* ends-with with nonexistent attr should not crash */
	n = xb_silo_query_first(silo, "component[ends-with(attr('nonexistent'),'x')]", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);

#ifdef HAVE_LIBSTEMMER
	/* stem with nonexistent attr should not crash */
	n = xb_silo_query_first(silo, "component[stem(attr('nonexistent'))='x']", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);
#endif
}

static void
xb_xpath_parent_func(void)
{
	XbNode *n;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<components origin=\"lvfs\">\n"
			   "  <header>\n"
			   "    <csum type=\"sha1\">dead</csum>\n"
			   "  </header>\n"
			   "  <component type=\"desktop\">\n"
			   "    <id>gimp.desktop</id>\n"
			   "    <id>org.gnome.Gimp.desktop</id>\n"
			   "  </component>\n"
			   "  <component type=\"firmware\">\n"
			   "    <id>org.hughski.ColorHug2.firmware</id>\n"
			   "    <pkgname>colorhug-client</pkgname>\n"
			   "    <description xml:lang=\"de_DE\"><p>Wilcommen!</p></description>\n"
			   "    <description><p>hello!</p></description>\n"
			   "    <description xml:lang=\"fr_FR\"><p>Bonjour!</p></description>\n"
			   "    <project_license>GPL-2.0</project_license>\n"
			   "  </component>\n"
			   "</components>\n";

	/* import from XML */
	ret = xb_test_import_xml(builder, xml, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_add_locale(builder, "C");
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* get node, no parent */
	n = xb_silo_query_first(silo, "components/component[@type='firmware']/id", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "org.hughski.ColorHug2.firmware");
	g_assert_cmpstr(xb_node_get_element(n), ==, "id");
	g_clear_object(&n);

	/* get node, one parent */
	n = xb_silo_query_first(silo, "components/component[@type='firmware']/id/..", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_element(n), ==, "component");
	g_clear_object(&n);

	/* get node, multiple parents */
	n = xb_silo_query_first(silo, "components/component[@type='firmware']/id/../..", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_element(n), ==, "components");
	g_clear_object(&n);

	/* descend, ascend, descend */
	n = xb_silo_query_first(silo,
				"components/component[@type='firmware']/pkgname/../project_license",
				&error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "GPL-2.0");
	g_clear_object(&n);

	/* descend, ascend, descend */
	n = xb_silo_query_first(silo,
				"components/component/pkgname[text()~='colorhug']/../id",
				&error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object(&n);

	/* get node, too many parents */
	n = xb_silo_query_first(silo, "components/component[@type='firmware']/id/../../..", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
	g_assert_null(n);
	g_clear_error(&error);

	/* can't go lower than root */
	n = xb_silo_query_first(silo, "..", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
	g_assert_null(n);
	g_clear_error(&error);

	/* fuzzy substring match */
	n = xb_silo_query_first(silo, "components/component/pkgname[text()~='colorhug']", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "colorhug-client");
	g_clear_object(&n);

	/* strlen match */
	n = xb_silo_query_first(silo,
				"components/component/pkgname[string-length(text())==15]",
				&error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "colorhug-client");
	g_clear_object(&n);

	/* fuzzy substring match */
	n = xb_silo_query_first(silo, "components/component[@type~='firm']/pkgname", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "colorhug-client");
	g_clear_object(&n);
}

static void
xb_xpath_prepared_func(void)
{
	XbNode *n;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbSilo) silo = NULL;
	g_autoptr(XbQuery) query = NULL;
	g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT();
	g_autoptr(XbNode) component = NULL;
	g_autoptr(GPtrArray) components = NULL;
	const gchar *xml = "<components origin=\"lvfs\">\n"
			   "  <component type=\"desktop\">\n"
			   "    <id>gimp.desktop</id>\n"
			   "    <id>org.gnome.Gimp.desktop</id>\n"
			   "  </component>\n"
			   "  <component type=\"firmware\">\n"
			   "    <id>org.hughski.ColorHug2.firmware</id>\n"
			   "    <pkgname>colorhug-client</pkgname>\n"
			   "  </component>\n"
			   "</components>\n";

	/* import from XML */
	ret = xb_test_import_xml(builder, xml, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_add_locale(builder, "C");
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* get first component */
	component = xb_silo_query_first(silo, "components/component", &error);
	g_assert_no_error(error);
	g_assert_nonnull(component);

	/* prepared statement on node */
	query = xb_query_new(silo, "id[text()=?]/..", &error);
	g_assert_no_error(error);
	g_assert_nonnull(query);
	xb_value_bindings_bind_str(xb_query_context_get_bindings(&context),
				   0,
				   "gimp.desktop",
				   NULL);
	components = xb_node_query_with_context(component, query, &context, &error);
	g_assert_no_error(error);
	g_assert_nonnull(components);
	g_assert_cmpint(components->len, ==, 1);
	n = g_ptr_array_index(components, 0);
	g_assert_cmpstr(xb_node_get_attr(n, "type"), ==, "desktop");
}

static void
xb_xpath_query_reverse_func(void)
{
	XbNode *n;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbSilo) silo = NULL;
	g_autoptr(XbQuery) query = NULL;
	g_autoptr(GPtrArray) names = NULL;
	const gchar *xml = "<names>\n"
			   "  <name>foo</name>\n"
			   "  <name>bar</name>\n"
			   "  <name>baz</name>\n"
			   "</names>\n";

	/* import from XML */
	ret = xb_test_import_xml(builder, xml, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* get first when reversed */
	query = xb_query_new_full(silo, "names/name", XB_QUERY_FLAG_REVERSE, &error);
	g_assert_no_error(error);
	g_assert_nonnull(query);
	names = xb_silo_query_with_context(silo, query, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(names);
	g_assert_cmpint(names->len, ==, 3);
	n = g_ptr_array_index(names, 0);
	g_assert_cmpstr(xb_node_get_text(n), ==, "baz");
}

static void
xb_xpath_query_force_node_cache_func(void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbNode) n1 = NULL;
	g_autoptr(XbNode) n2 = NULL;
	g_autoptr(XbQuery) query = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<names>\n"
			   "  <name>foo</name>\n"
			   "</names>\n";

	/* import from XML */
	ret = xb_test_import_xml(builder, xml, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* use a cache for this specific result */
	query = xb_query_new_full(silo, "names/name", XB_QUERY_FLAG_FORCE_NODE_CACHE, &error);
	g_assert_no_error(error);
	g_assert_nonnull(query);
	n1 = xb_silo_query_first_with_context(silo, query, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(n1);
	n2 = xb_silo_query_first_with_context(silo, query, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(n2);
	g_assert(n1 == n2);
}

static void
xb_xpath_glob_func(void)
{
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<components origin=\"lvfs\">\n"
			   "  <component type=\"desktop\">\n"
			   "    <id>gimp.desktop</id>\n"
			   "    <id>org.gnome.GIMP.desktop</id>\n"
			   "  </component>\n"
			   "</components>\n";

	/* import from XML */
	silo = xb_silo_new_from_xml(xml, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* get node, no parent */
	n = xb_silo_query_first(silo, "components/component[@type='desktop']/*", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_element(n), ==, "id");

	/* export this one node */
	xml2 = xb_node_export(n, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_cmpstr(xml2, ==, "<id>gimp.desktop</id>");
}

void
xb_xpath_test_add_funcs(void)
{
	g_test_add_func("/libxmlb/xpath", xb_xpath_func);
	g_test_add_func("/libxmlb/xpath/null-attr", xb_xpath_null_attr_func);
	g_test_add_func("/libxmlb/xpath-query", xb_xpath_query_func);
	g_test_add_func("/libxmlb/xpath-query/reverse", xb_xpath_query_reverse_func);
	g_test_add_func("/libxmlb/xpath-query/force-node-cache",
			xb_xpath_query_force_node_cache_func);
	g_test_add_func("/libxmlb/xpath/helpers", xb_xpath_helpers_func);
	g_test_add_func("/libxmlb/xpath/prepared", xb_xpath_prepared_func);
	g_test_add_func("/libxmlb/xpath/incomplete", xb_xpath_incomplete_func);
	g_test_add_func("/libxmlb/xpath-parent", xb_xpath_parent_func);
	g_test_add_func("/libxmlb/xpath-glob", xb_xpath_glob_func);
	g_test_add_func("/libxmlb/xpath-node", xb_xpath_node_func);
	g_test_add_func("/libxmlb/xpath-parent-subnode", xb_xpath_parent_subnode_func);
}
