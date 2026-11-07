/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>

#include "xb-silo-query.h"
#include "xb-string-private.h"
#include "xb-test-common.h"

static void
xb_xpath_in_func(void)
{
	XbNode *n;
	gboolean ret;
	g_autoptr(GError) error = NULL;
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

	/* in() with valid match should work */
	n = xb_silo_query_first(silo, "component/id[text()=('test','other')]", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "test");
	g_clear_object(&n);

	/* in() with no match should return NOT_FOUND, not crash */
	n = xb_silo_query_first(silo, "component/id[text()=('nope','nada')]", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);
}

static void
xb_xpath_in_lower_func(void)
{
	XbNode *n;
	gboolean ret;
	g_autoptr(GError) error = NULL;
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

	/* in() with lower-case() should match */
	n = xb_silo_query_first(silo, "component/id[in(text(),lower-case('TEST'))]", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "test");
	g_clear_object(&n);

	/* in() with lower-case() no match */
	n = xb_silo_query_first(silo, "component/id[in(text(),lower-case('NOPE'))]", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);
}

static void
xb_xpath_in_null_needle_func(void)
{
	XbNode *n;
	gboolean ret;
	g_autoptr(GError) error = NULL;
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

	/* in() with a missing attribute (NULL needle) should not crash */
	n = xb_silo_query_first(silo, "component/id[attr('missing')=('a','b')]", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);

	/* in() with a NULL haystack entry (from attr on missing attribute) should not crash */
	n = xb_silo_query_first(silo,
				"component/id[in(text(),lower-case(attr('missing')))]",
				&error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);
}

static void
xb_xpath_stem_func(void)
{
	XbNode *n;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<component type=\"desktop\">\n"
			   "  <id>test</id>\n"
			   "</component>\n";

	ret = xb_builder_source_load_xml(source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* stem should type-check the top of stack, not the bottom */
	n = xb_silo_query_first(silo, "component/id[stem(text())='test']", &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
	g_assert_cmpstr(xb_node_get_text(n), ==, "test");
	g_clear_object(&n);

	/* stem with no match */
	n = xb_silo_query_first(silo, "component/id[stem(text())='nope']", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null(n);
	g_clear_error(&error);
}

typedef struct {
	const gchar *pred;
	gboolean matches;
	gboolean literal; /* all the operands are literals */
} XbXpathPredicateTest;

/* runs each predicate both optimized and unoptimized, checking the reason for
 * any failure rather than just that one happened */
static void
xb_xpath_predicate_assert(XbSilo *silo, const XbXpathPredicateTest *tests)
{
	for (guint i = 0; tests[i].pred != NULL; i++) {
		g_autofree gchar *xpath = g_strdup_printf("component/id[%s]", tests[i].pred);

		for (guint j = 0; j < 2; j++) {
			gboolean optimize = (j == 0);
			g_autoptr(GError) error = NULL;
			g_autoptr(XbNode) n = NULL;
			g_autoptr(XbQuery) query = NULL;

			query = xb_query_new_full(silo,
						  xpath,
						  optimize ? XB_QUERY_FLAG_OPTIMIZE
							   : XB_QUERY_FLAG_NONE,
						  &error);

			/* a predicate of literals that can never be true is
			 * folded away by the optimizer at parse time */
			if (optimize && tests[i].literal && !tests[i].matches) {
				g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
				g_assert_true(g_str_has_prefix(
				    error->message,
				    "the predicate will always evaluate to FALSE"));
				g_assert_null(query);
				continue;
			}
			g_assert_no_error(error);
			g_assert_nonnull(query);

			/* everything else has to be decided at run time */
			n = xb_silo_query_first_full(silo, query, &error);
			if (tests[i].matches) {
				g_assert_no_error(error);
				g_assert_nonnull(n);
			} else {
				g_assert_error(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
				g_assert_null(n);
			}
		}
	}
}

static void
xb_xpath_compare_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbSilo) silo =
	    xb_silo_new_from_xml("<component><id>test</id></component>", &error);
	const XbXpathPredicateTest tests[] = {{"5<'3'", FALSE, TRUE},
					      {"'3'<5", TRUE, TRUE},
					      {"5<='3'", FALSE, TRUE},
					      {"'3'<=5", TRUE, TRUE},
					      {"5>'3'", TRUE, TRUE},
					      {"'3'>5", FALSE, TRUE},
					      {"5>='3'", TRUE, TRUE},
					      {"'3'>=5", FALSE, TRUE},
					      /* compared against the node text at run time */
					      {"text()<'zz'", TRUE, FALSE},
					      {"text()>'zz'", FALSE, FALSE},
					      {"text()<='test'", TRUE, FALSE},
					      {"text()>='zz'", FALSE, FALSE},
					      {NULL, FALSE, FALSE}};

	g_assert_no_error(error);
	g_assert_nonnull(silo);

	xb_xpath_predicate_assert(silo, tests);
}

static void
xb_xpath_func_args_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbSilo) silo =
	    xb_silo_new_from_xml("<component><id>test</id></component>", &error);
	const XbXpathPredicateTest tests[] = {
	    {"contains('test','es')", TRUE, TRUE},
	    {"contains('es','test')", FALSE, TRUE},
	    {"starts-with('test','te')", TRUE, TRUE},
	    {"starts-with('te','test')", FALSE, TRUE},
	    {"ends-with('test','st')", TRUE, TRUE},
	    {"ends-with('st','test')", FALSE, TRUE},
	    {"normalize-space('  a \t b\n\nc  ')='a b c'", TRUE, TRUE},
	    {"normalize-space('   ')=''", TRUE, TRUE},
	    {"normalize-space('a  b')='a  b'", FALSE, TRUE},
	    {"normalize-space(12)='12'", TRUE, TRUE},
	    /* arguments not separated by the comma parser */
	    {"contains(text(),'es')", TRUE, FALSE},
	    {"contains(text(),'zz')", FALSE, FALSE},
	    {"starts-with(text(),'te')", TRUE, FALSE},
	    {"starts-with(text(),'zz')", FALSE, FALSE},
	    {"normalize-space(text())='test'", TRUE, FALSE},
	    {"normalize-space(@missing)=''", TRUE, FALSE},
	    {NULL, FALSE, FALSE},
	};

	g_assert_no_error(error);
	g_assert_nonnull(silo);

	xb_xpath_predicate_assert(silo, tests);
}

static void
xb_xpath_func_normalize_space_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbSilo) silo =
	    xb_silo_new_from_xml("<component><id> a  b </id></component>", &error);
	const XbXpathPredicateTest tests[] = {
	    {"normalize-space(text())='a b'", TRUE, FALSE},
	    {"normalize-space(text())=' a  b '", FALSE, FALSE},
	    {NULL, FALSE, FALSE},
	};

	g_assert_no_error(error);
	g_assert_nonnull(silo);

	xb_xpath_predicate_assert(silo, tests);
}

static void
xb_xpath_unfinished_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo =
	    xb_silo_new_from_xml("<component><id>test</id></component>", &error);

	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* an unfinished predicate is an error even at the start of a section */
	n = xb_silo_query_first(silo, "[id", &error);
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
	g_assert_null(n);
}

static void
xb_xpath_unbound_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbQuery) query = NULL;
	g_autoptr(XbSilo) silo =
	    xb_silo_new_from_xml("<component timestamp=\"1631923200\"><id>test</id></component>",
				 &error);
	g_auto(XbQueryContext) context_bound = XB_QUERY_CONTEXT_INIT();
	g_auto(XbQueryContext) context_unbound = XB_QUERY_CONTEXT_INIT();

	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* a query with an unbound variable must error, not compare against 0 */
	query = xb_query_new(silo, "component[@timestamp=?]", &error);
	g_assert_no_error(error);
	g_assert_nonnull(query);
	g_assert_null(xb_silo_query_first_full(silo, query, &error));
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_clear_error(&error);

	/* ...and the same when the bindings exist but were never set, which
	 * takes a different path in xb_machine_run_with_bindings() */
	g_assert_null(xb_silo_query_first_with_context(silo, query, &context_unbound, &error));
	g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_clear_error(&error);

	/* binding the value still works */
	xb_value_bindings_bind_val(xb_query_context_get_bindings(&context_bound), 0, 1631923200);
	n = xb_silo_query_first_with_context(silo, query, &context_bound, &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
}

static void
xb_xpath_bound_in_func(void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbQuery) query = NULL;
	g_autoptr(XbSilo) silo =
	    xb_silo_new_from_xml("<component><id>test</id></component>", &error);
	g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT();
	g_auto(XbQueryContext) context2 = XB_QUERY_CONTEXT_INIT();

	g_assert_no_error(error);
	g_assert_nonnull(silo);
	xb_value_bindings_bind_str(xb_query_context_get_bindings(&context2), 0, "test", NULL);

	/* the bound value is one element of a tuple, so it has to keep the
	 * level the parser gave it or in() stops scanning at the wrong place */
	query = xb_query_new(silo, "component/id[text()=(?,'b')]", &error);
	g_assert_no_error(error);
	g_assert_nonnull(query);
	xb_value_bindings_bind_str(xb_query_context_get_bindings(&context), 0, "test", NULL);
	n = xb_silo_query_first_with_context(silo, query, &context, &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);

	/* ...and with the binding as the last element of the tuple */
	g_clear_object(&n);
	g_clear_object(&query);
	query = xb_query_new(silo, "component/id[text()=('a',?)]", &error);
	g_assert_no_error(error);
	g_assert_nonnull(query);
	n = xb_silo_query_first_with_context(silo, query, &context2, &error);
	g_assert_no_error(error);
	g_assert_nonnull(n);
}

static void
xb_xpath_escape_func(void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml = "<component>"
			   "<summary>foo\tbar\nbaz</summary>"
			   "<description>back\\slash</description>"
			   "<name>literal\\tescape</name>"
			   "</component>";
	struct {
		const gchar *element;
		const gchar *text;
		const gchar *escaped;
	} tests[] = {
	    /* real control characters */
	    {"summary", "foo\tbar\nbaz", "foo\\tbar\\nbaz"},
	    /* a literal backslash has to survive the round-trip */
	    {"description", "back\\slash", "back\\\\slash"},
	    /* ...including when it is followed by an escape character */
	    {"name", "literal\\tescape", "literal\\\\tescape"},
	};

	/* import some XML, keeping the literal control characters in the text */
	ret = xb_builder_source_load_xml(source, xml, XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT, &error);
	g_assert_no_error(error);
	g_assert_true(ret);
	xb_builder_import_source(builder, source);
	silo = xb_builder_compile(builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error(error);
	g_assert_nonnull(silo);

	/* text escaped with xb_string_escape() must round-trip through the
	 * query parser back to the original text */
	for (gsize i = 0; i < G_N_ELEMENTS(tests); i++) {
		g_autofree gchar *esc = xb_string_escape(tests[i].text);
		g_autofree gchar *xpath = NULL;
		g_autoptr(XbNode) n = NULL;

		g_assert_cmpstr(esc, ==, tests[i].escaped);
		xpath = g_strdup_printf("component/%s[text()='%s']", tests[i].element, esc);
		n = xb_silo_query_first(silo, xpath, &error);
		g_assert_no_error(error);
		g_assert_nonnull(n);
		g_assert_cmpstr(xb_node_get_text(n), ==, tests[i].text);
	}
}

void
xb_xpath_predicate_test_add_funcs(void)
{
	g_test_add_func("/libxmlb/xpath/in", xb_xpath_in_func);
	g_test_add_func("/libxmlb/xpath/in-lower", xb_xpath_in_lower_func);
	g_test_add_func("/libxmlb/xpath/in-null-needle", xb_xpath_in_null_needle_func);
	g_test_add_func("/libxmlb/xpath/stem", xb_xpath_stem_func);
	g_test_add_func("/libxmlb/xpath/compare", xb_xpath_compare_func);
	g_test_add_func("/libxmlb/xpath/func-args", xb_xpath_func_args_func);
	g_test_add_func("/libxmlb/xpath/func-normalize-space", xb_xpath_func_normalize_space_func);
	g_test_add_func("/libxmlb/xpath/unfinished", xb_xpath_unfinished_func);
	g_test_add_func("/libxmlb/xpath/unbound", xb_xpath_unbound_func);
	g_test_add_func("/libxmlb/xpath/bound-in", xb_xpath_bound_in_func);
	g_test_add_func("/libxmlb/xpath/escape", xb_xpath_escape_func);
}
