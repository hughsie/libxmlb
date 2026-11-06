/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>

#include "xb-common-private.h"
#include "xb-query-context.h"
#include "xb-stack-private.h"
#include "xb-string-private.h"
#include "xb-test-common.h"

static void
xb_stack_func(void)
{
	XbOpcode *op1, *op2, *op3, *op4;
	g_auto(XbOpcode) op1_popped = XB_OPCODE_INIT();
	g_auto(XbOpcode) op2_popped = XB_OPCODE_INIT();
	g_auto(XbOpcode) op3_popped = XB_OPCODE_INIT();
	g_autoptr(XbStack) stack = xb_stack_new(3);

	/* push three opcodes */
	g_assert_true(xb_stack_push(stack, &op3, NULL));
	xb_opcode_text_init(op3, "dave");
	g_assert_true(xb_stack_push(stack, &op2, NULL));
	xb_opcode_integer_init(op2, 1);
	g_assert_true(xb_stack_push(stack, &op1, NULL));
	xb_opcode_func_init(op1, 0);
	g_assert_false(xb_stack_push(stack, &op4, NULL));
	g_assert_null(op4);

	/* pop the same opcodes */
	g_assert_true(xb_stack_pop(stack, &op1_popped, NULL));
	g_assert_cmpint(xb_opcode_get_kind(&op1_popped), ==, XB_OPCODE_KIND_FUNCTION);

	g_assert_true(xb_stack_pop(stack, &op2_popped, NULL));
	g_assert_cmpint(xb_opcode_get_kind(&op2_popped), ==, XB_OPCODE_KIND_INTEGER);
	g_assert_cmpuint(xb_opcode_get_val(&op2_popped), ==, 1);

	g_assert_true(xb_stack_pop(stack, &op3_popped, NULL));
	g_assert_cmpint(xb_opcode_get_kind(&op3_popped), ==, XB_OPCODE_KIND_TEXT);
	g_assert_cmpstr(xb_opcode_get_str(&op3_popped), ==, "dave");

	/* re-add one opcode */
	g_assert_true(xb_stack_push(stack, &op4, NULL));
	xb_opcode_text_init(op4, "dave again");
	g_assert_nonnull(op4);

	/* finish, cleaning up the stack properly... */
}

static void
xb_stack_peek_func(void)
{
	XbOpcode *op1, *op2, *op3;
	g_autoptr(XbStack) stack = xb_stack_new(3);

	/* push three opcodes */
	g_assert_true(xb_stack_push(stack, &op1, NULL));
	xb_opcode_func_init(op1, 0);
	g_assert_true(xb_stack_push(stack, &op2, NULL));
	xb_opcode_integer_init(op2, 1);
	g_assert_true(xb_stack_push(stack, &op3, NULL));
	xb_opcode_text_init(op3, "dave");

	/* peek the same opcodes */
	g_assert_true(xb_stack_peek_head(stack) == op1);
	g_assert_true(xb_stack_peek_tail(stack) == op3);
	g_assert_true(xb_stack_peek(stack, 0) == op1);
	g_assert_true(xb_stack_peek(stack, 1) == op2);
	g_assert_true(xb_stack_peek(stack, 2) == op3);
}

static void
xb_common_union_func(void)
{
	g_autoptr(GString) xpath = g_string_new(NULL);
	xb_string_append_union(xpath, "components/component");
	g_assert_cmpstr(xpath->str, ==, "components/component");
	xb_string_append_union(xpath, "applications/application");
	g_assert_cmpstr(xpath->str, ==, "components/component|applications/application");
}

static void
xb_common_func(void)
{
	g_assert_true(xb_string_search("gimp", "gimp"));
	g_assert_true(xb_string_search("GIMP", "gimp"));
	g_assert_true(xb_string_search("The GIMP", "gimp"));
	g_assert_true(xb_string_search("The GIMP Editor", "gimp"));
	g_assert_false(xb_string_search("gimp", ""));
	g_assert_false(xb_string_search("gimp", "imp"));
	g_assert_false(xb_string_search("the gimp editor", "imp"));
	g_assert_true(xb_string_token_valid("the"));
	g_assert_false(xb_string_token_valid(NULL));
	g_assert_false(xb_string_token_valid(""));
	g_assert_false(xb_string_token_valid("a"));
	g_assert_false(xb_string_token_valid("ab"));
}

static void
xb_common_locale_ensure_posix_func(void)
{
	struct {
		const gchar *locale;
		const gchar *posix;
	} items[] = {
	    /* already POSIX, left alone */
	    {"en_GB", "en_GB"},
	    {"fr", "fr"},
	    {"C", "C"},
	    /* BCP47 gets converted to POSIX */
	    {"en-GB", "en_GB"},
	    {"zh-TW", "zh_TW"},
	    /* the language and country get normalized, whatever the separator */
	    {"en-gb", "en_GB"},
	    {"EN-GB", "en_GB"},
	    {"en_gb", "en_GB"},
	    {"fil-ph", "fil_PH"},
	    {"en-gb.UTF-8", "en_GB.UTF-8"},
	    {"ca-es@valencia", "ca_ES@valencia"},
	    /* without a separator only a language gets normalized */
	    {"EN", "en"},
	    {"FIL", "fil"},
	    {"EN.UTF-8", "en.UTF-8"},
	    {"POSIX", "POSIX"},
	    {"C.UTF-8", "C.UTF-8"},
	    /* the `-` in a codeset or modifier suffix is not a separator */
	    {"fr_FR.UTF-8", "fr_FR.UTF-8"},
	    {"fr-FR.UTF-8", "fr_FR.UTF-8"},
	    {"ca-ES@valencia", "ca_ES@valencia"},
	    {"sr-RS.UTF-8@latin", "sr_RS.UTF-8@latin"},
	    /* only the first delimiter is converted, as POSIX locales have at
	     * most one `lang_COUNTRY` separator */
	    {"zh-Hans", "zh_Hans"},
	    /* a script implied by the country is dropped, in any case and with
	     * any suffix preserved */
	    {"zh-Hans-CN", "zh_CN"},
	    {"zh-Hant-TW", "zh_TW"},
	    {"zh-hans-cn", "zh_CN"},
	    {"zh-Hans-CN.UTF-8", "zh_CN.UTF-8"},
	    /* a script POSIX spells as a modifier is moved after any codeset */
	    {"sr-Latn-RS", "sr_RS@latin"},
	    {"SR-latn-rs", "sr_RS@latin"},
	    {"sr-Latn-RS.UTF-8", "sr_RS.UTF-8@latin"},
	    {"sr-Latn-ME", "sr_ME@latin"},
	    {"sr-Latn", "sr@latin"},
	    {"sr-latn", "sr@latin"},
	    {"sr-Latn.UTF-8", "sr.UTF-8@latin"},
	    /* but not if that would replace an existing modifier */
	    {"sr-Latn-RS@ijekavian", "sr_Latn-RS@ijekavian"},
	    /* any other script stays distinct */
	    {"zh-Hant-CN", "zh_Hant-CN"},
	    {"zh-Hans-TW", "zh_Hans-TW"},
	    {"zh-Hans-CNX", "zh_Hans-CNX"},
	    {"sr-Latn-BA", "sr_Latn-BA"},
	    {"sr-Latn-MEX", "sr_Latn-MEX"},
	    {"sr-Latn@ijekavian", "sr_Latn@ijekavian"},
	    {"zh-hant", "zh_hant"},
	    {"ZH-Hant-CN", "zh_Hant-CN"},
	    /* a form we already converted is not changed again */
	    {"zh_Hant-CN", "zh_Hant-CN"},
	    /* a separator with nothing on one side is not converted */
	    {"-", "-"},
	    {"_", "_"},
	    {"--", "--"},
	    {"-gb", "-gb"},
	    {"en-", "en-"},
	    {"EN-", "en-"},
	    {"en-.UTF-8", "en-.UTF-8"},
	    /* a short country, or a trailing delimiter after the first */
	    {"en-g", "en_g"},
	    {"en-gb-", "en_GB-"},
	    /* empty is not the same as unset */
	    {"", ""},
	    {NULL, NULL},
	};
	for (guint i = 0; i < G_N_ELEMENTS(items); i++) {
		g_autofree gchar *posix = xb_locale_ensure_posix(items[i].locale);
		g_assert_cmpstr(posix, ==, items[i].posix);
	}
}

static void
xb_common_content_type_func(void)
{
	struct {
		const gchar *fn;
		const gchar *ctype;
	} items[] = {{"test.desktop", "application/x-desktop"},
		     {"test.quirk", "text/plain"},
		     {"test.xml", "application/xml"},
		     {"test.xml.gz.gz.gz", "application/gzip"},
		     {"test.xml.xz", "application/x-xz"},
		     {"test.xml.zst", "application/zstd"},
		     {NULL, NULL}};
	for (guint i = 0; items[i].fn != NULL; i++) {
		gboolean ret;
		gsize bufsz = 0;
		g_autofree gchar *buf = NULL;
		g_autofree gchar *ctype = NULL;
		g_autofree gchar *fn = g_test_build_filename(G_TEST_DIST, items[i].fn, NULL);
		g_autoptr(GError) error = NULL;
		ret = g_file_get_contents(fn, &buf, &bufsz, &error);
		g_assert_no_error(error);
		g_assert_true(ret);
		ctype = xb_content_type_guess(fn, (const guchar *)buf, bufsz);
		g_assert_cmpstr(ctype, ==, items[i].ctype);
	}
}

static void
xb_common_searchv_func(void)
{
	const gchar *haystack[] = {"these", "words", "ready", NULL};
	const gchar *found[] = {"xxx", "wor", "yyy", NULL};
	const gchar *unfound1[] = {"xxx", "yyy", NULL};
	const gchar *unfound2[] = {"ords", NULL};
	g_assert_true(xb_string_searchv(haystack, found));
	g_assert_false(xb_string_searchv(haystack, unfound1));
	g_assert_false(xb_string_searchv(haystack, unfound2));
}

static void
xb_value_bindings_copy_func(void)
{
	g_auto(XbValueBindings) bindings = XB_VALUE_BINDINGS_INIT();
	g_autoptr(XbValueBindings) copy = NULL;

	/* leave index 0 unbound; a buggy copy would assert-abort on the first
	 * unbound slot instead of skipping it */
	xb_value_bindings_bind_str(&bindings, 1, "one", NULL);
	xb_value_bindings_bind_val(&bindings, 3, 3);

	copy = xb_value_bindings_copy(&bindings);
	g_assert_nonnull(copy);
	g_assert_false(xb_value_bindings_is_bound(copy, 0));
	g_assert_true(xb_value_bindings_is_bound(copy, 1));
	g_assert_false(xb_value_bindings_is_bound(copy, 2));
	g_assert_true(xb_value_bindings_is_bound(copy, 3));
}

static void
xb_query_context_copy_func(void)
{
	g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT();
	XbValueBindings *bindings = xb_query_context_get_bindings(&context);
	g_autoptr(XbQueryContext) copy = NULL;
	XbValueBindings *copy_bindings;

	/* leave index 0 unbound, but bind index 1 and 3; a buggy copy that stops
	 * at the first unbound slot would silently drop these */
	xb_value_bindings_bind_str(bindings, 1, "one", NULL);
	xb_value_bindings_bind_val(bindings, 3, 3);
	xb_query_context_set_limit(&context, 123);

	copy = xb_query_context_copy(&context);
	g_assert_nonnull(copy);
	g_assert_cmpuint(xb_query_context_get_limit(copy), ==, 123);

	copy_bindings = xb_query_context_get_bindings(copy);
	g_assert_false(xb_value_bindings_is_bound(copy_bindings, 0));
	g_assert_true(xb_value_bindings_is_bound(copy_bindings, 1));
	g_assert_false(xb_value_bindings_is_bound(copy_bindings, 2));
	g_assert_true(xb_value_bindings_is_bound(copy_bindings, 3));
}

void
xb_common_test_add_funcs(void)
{
	g_test_add_func("/libxmlb/common", xb_common_func);
	g_test_add_func("/libxmlb/common/content-type", xb_common_content_type_func);
	g_test_add_func("/libxmlb/common/locale-ensure-posix", xb_common_locale_ensure_posix_func);
	g_test_add_func("/libxmlb/common/searchv", xb_common_searchv_func);
	g_test_add_func("/libxmlb/common/union", xb_common_union_func);
	g_test_add_func("/libxmlb/value-bindings/copy", xb_value_bindings_copy_func);
	g_test_add_func("/libxmlb/query-context/copy", xb_query_context_copy_func);
	g_test_add_func("/libxmlb/stack", xb_stack_func);
	g_test_add_func("/libxmlb/stack/peek", xb_stack_peek_func);
}
