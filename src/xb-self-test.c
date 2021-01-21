/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <gio/gio.h>
#include <locale.h>

#include "xb-builder.h"
#include "xb-builder-node.h"
#include "xb-machine.h"
#include "xb-node-query.h"
#include "xb-opcode.h"
#include "xb-opcode-private.h"
#include "xb-silo-export.h"
#include "xb-silo-private.h"
#include "xb-silo-query-private.h"
#include "xb-stack-private.h"
#include "xb-string-private.h"

static GMainLoop *_test_loop = NULL;
static guint _test_loop_timeout_id = 0;

#define XB_SELF_TEST_INOTIFY_TIMEOUT		10000 /* ms */

static gboolean
xb_test_hang_check_cb (gpointer user_data)
{
	g_main_loop_quit (_test_loop);
	_test_loop_timeout_id = 0;
	return G_SOURCE_REMOVE;
}

static void
xb_test_loop_run_with_timeout (guint timeout_ms)
{
	g_assert (_test_loop_timeout_id == 0);
	g_assert (_test_loop == NULL);
	_test_loop = g_main_loop_new (NULL, FALSE);
	_test_loop_timeout_id = g_timeout_add (timeout_ms, xb_test_hang_check_cb, NULL);
	g_main_loop_run (_test_loop);
}

static void
xb_test_loop_quit (void)
{
	if (_test_loop_timeout_id > 0) {
		g_source_remove (_test_loop_timeout_id);
		_test_loop_timeout_id = 0;
	}
	if (_test_loop != NULL) {
		g_main_loop_quit (_test_loop);
		g_main_loop_unref (_test_loop);
		_test_loop = NULL;
	}
}

static gboolean
xb_test_import_xml (XbBuilder *self, const gchar *xml, GError **error)
{
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();

	g_return_val_if_fail (XB_IS_BUILDER (self), FALSE);
	g_return_val_if_fail (xml != NULL, FALSE);

	/* add source */
	if (!xb_builder_source_load_xml (source, xml, XB_BUILDER_SOURCE_FLAG_NONE, error))
		return FALSE;

	/* success */
	xb_builder_import_source (self, source);
	return TRUE;
}

static void
xb_stack_func (void)
{
	XbOpcode *op1, *op2, *op3, *op4;
	g_auto(XbOpcode) op1_popped = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2_popped = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op3_popped = XB_OPCODE_INIT ();
	g_autoptr(XbStack) stack = xb_stack_new (3);

	/* push three opcodes */
	g_assert_true (xb_stack_push (stack, &op3, NULL));
	xb_opcode_text_init (op3, "dave");
	g_assert_true (xb_stack_push (stack, &op2, NULL));
	xb_opcode_integer_init (op2, 1);
	g_assert_true (xb_stack_push (stack, &op1, NULL));
	xb_opcode_func_init (op1, 0);
	g_assert_false (xb_stack_push (stack, &op4, NULL));
	g_assert_null (op4);

	/* pop the same opcodes */
	g_assert_true (xb_stack_pop (stack, &op1_popped, NULL));
	g_assert_cmpint (xb_opcode_get_kind (&op1_popped), ==, XB_OPCODE_KIND_FUNCTION);

	g_assert_true (xb_stack_pop (stack, &op2_popped, NULL));
	g_assert_cmpint (xb_opcode_get_kind (&op2_popped), ==, XB_OPCODE_KIND_INTEGER);
	g_assert_cmpuint (xb_opcode_get_val (&op2_popped), ==, 1);

	g_assert_true (xb_stack_pop (stack, &op3_popped, NULL));
	g_assert_cmpint (xb_opcode_get_kind (&op3_popped), ==, XB_OPCODE_KIND_TEXT);
	g_assert_cmpstr (xb_opcode_get_str (&op3_popped), ==, "dave");

	/* re-add one opcode */
	g_assert_true (xb_stack_push (stack, &op4, NULL));
	xb_opcode_text_init (op4, "dave again");
	g_assert_nonnull (op4);

	/* finish, cleaning up the stack properly... */
}

static void
xb_stack_peek_func (void)
{
	XbOpcode *op1, *op2, *op3;
	g_autoptr(XbStack) stack = xb_stack_new (3);

	/* push three opcodes */
	g_assert_true (xb_stack_push (stack, &op1, NULL));
	xb_opcode_func_init (op1, 0);
	g_assert_true (xb_stack_push (stack, &op2, NULL));
	xb_opcode_integer_init (op2, 1);
	g_assert_true (xb_stack_push (stack, &op3, NULL));
	xb_opcode_text_init (op3, "dave");

	/* peek the same opcodes */
	g_assert_true (xb_stack_peek_head (stack) == op1);
	g_assert_true (xb_stack_peek_tail (stack) == op3);
	g_assert_true (xb_stack_peek (stack, 0) == op1);
	g_assert_true (xb_stack_peek (stack, 1) == op2);
	g_assert_true (xb_stack_peek (stack, 2) == op3);
}

static void
xb_common_union_func (void)
{
	g_autoptr(GString) xpath = g_string_new (NULL);
	xb_string_append_union (xpath, "components/component");
	g_assert_cmpstr (xpath->str, ==, "components/component");
	xb_string_append_union (xpath, "applications/application");
	g_assert_cmpstr (xpath->str, ==, "components/component|applications/application");
}

static void
xb_common_func (void)
{
	g_assert_true (xb_string_search ("gimp", "gimp"));
	g_assert_true (xb_string_search ("GIMP", "gimp"));
	g_assert_true (xb_string_search ("The GIMP", "gimp"));
	g_assert_true (xb_string_search ("The GIMP Editor", "gimp"));
	g_assert_false (xb_string_search ("gimp", ""));
	g_assert_false (xb_string_search ("gimp", "imp"));
	g_assert_false (xb_string_search ("the gimp editor", "imp"));
	g_assert_true (xb_string_token_valid ("the"));
	g_assert_false (xb_string_token_valid (NULL));
	g_assert_false (xb_string_token_valid (""));
	g_assert_false (xb_string_token_valid ("a"));
	g_assert_false (xb_string_token_valid ("ab"));
}

static void
xb_common_searchv_func (void)
{
	const gchar *haystack[] = { "these", "words", "ready", NULL };
	const gchar *found[] = { "xxx", "wor", "yyy", NULL };
	const gchar *unfound1[] = { "xxx", "yyy", NULL };
	const gchar *unfound2[] = { "ords", NULL };
	g_assert_true (xb_string_searchv (haystack, found));
	g_assert_false (xb_string_searchv (haystack, unfound1));
	g_assert_false (xb_string_searchv (haystack, unfound2));
}

static void
xb_opcodes_kind_func (void)
{
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op3 = XB_OPCODE_INIT ();

	xb_opcode_func_init (&op1, 0);
	xb_opcode_integer_init (&op2, 1);
	xb_opcode_text_init (&op3, "dave");

	/* check kind */
	g_assert_cmpint (xb_opcode_get_kind (&op1), ==, XB_OPCODE_KIND_FUNCTION);
	g_assert_cmpint (xb_opcode_get_kind (&op2), ==, XB_OPCODE_KIND_INTEGER);
	g_assert_cmpint (xb_opcode_get_kind (&op3), ==, XB_OPCODE_KIND_TEXT);

	/* to and from string */
	g_assert_cmpint (xb_opcode_kind_from_string ("TEXT"), ==, XB_OPCODE_KIND_TEXT);
	g_assert_cmpint (xb_opcode_kind_from_string ("FUNC"), ==, XB_OPCODE_KIND_FUNCTION);
	g_assert_cmpint (xb_opcode_kind_from_string ("INTE"), ==, XB_OPCODE_KIND_INTEGER);
	g_assert_cmpint (xb_opcode_kind_from_string ("dave"), ==, XB_OPCODE_KIND_UNKNOWN);
	g_assert_cmpstr (xb_opcode_kind_to_string (XB_OPCODE_KIND_TEXT), ==, "TEXT");
	g_assert_cmpstr (xb_opcode_kind_to_string (XB_OPCODE_KIND_FUNCTION), ==, "FUNC");
	g_assert_cmpstr (xb_opcode_kind_to_string (XB_OPCODE_KIND_INTEGER), ==, "INTE");
	g_assert_cmpstr (xb_opcode_kind_to_string (XB_OPCODE_KIND_UNKNOWN), ==, NULL);

	/* integer compare */
	g_assert_false (xb_opcode_cmp_val (&op1));
	g_assert_true (xb_opcode_cmp_val (&op2));
	g_assert_false (xb_opcode_cmp_val (&op3));

	/* string compare */
	g_assert_false (xb_opcode_cmp_str (&op1));
	g_assert_false (xb_opcode_cmp_str (&op2));
	g_assert_true (xb_opcode_cmp_str (&op3));
}

static void
xb_predicate_func (void)
{
	g_autoptr(XbSilo) silo = xb_silo_new ();
	struct {
		const gchar	*pred;
		const gchar	*str;
	} tests[] = {
		{ "'a'='b'",
		  "'a','b',eq()" },
		{ "@a='b'",
		  "'a',attr(),'b',eq()" },
		{ "@a=='b'",
		  "'a',attr(),'b',eq()" },
		{ "'a'<'b'",
		  "'a','b',lt()" },
		{ "999>=123",
		  "999,123,ge()" },
		{ "not(0)",
		  "0,not()" },
		{ "@a",
		  "'a',attr(),'(null)',ne()" },
		{ "not(@a)",
		  "'a',attr(),not()" },
		{ "'a'=",
		  "'a',eq()" },
		{ "='b'",
		  "'b',eq()" },
		{ "999=\'b\'",
		  "999,'b',eq()" },
		{ "text()=\'b\'",
		  "text(),'b',eq()" },
		{ "last()",
		  "last()" },
		{ "text()~='beef'",
		  "text(),'beef'[beef],search()" },
		{ "@type~='dead'",
		  "'type',attr(),'dead',search()" },
		{ "2",
		  "2,position(),eq()" },
		{ "text()=lower-case('firefox')",
		  "text(),'firefox',lower-case(),eq()" },
		{ "$'a'=$'b'",
		  "$'a',$'b',eq()" },
		{ "('a'='b')&&('c'='d')",
		  "'a','b',eq(),'c','d',eq(),and()" },
		/* sentinel */
		{ NULL, NULL }
	};
	const gchar *invalid[] = {
		"text(",
		"text((((((((((((((((((((text()))))))))))))))))))))",
		NULL
	};
	xb_machine_set_debug_flags (xb_silo_get_machine (silo),
				    XB_MACHINE_DEBUG_FLAG_SHOW_STACK |
				    XB_MACHINE_DEBUG_FLAG_SHOW_PARSING);
	for (guint i = 0; tests[i].pred != NULL; i++) {
		g_autofree gchar *str = NULL;
		g_autoptr(GError) error = NULL;
		g_autoptr(XbStack) opcodes = NULL;

		g_debug ("testing %s", tests[i].pred);
		opcodes = xb_machine_parse_full (xb_silo_get_machine (silo),
						 tests[i].pred, -1,
						 XB_MACHINE_PARSE_FLAG_NONE,
						 &error);
		g_assert_no_error (error);
		g_assert_nonnull (opcodes);
		str = xb_stack_to_string (opcodes);
		g_assert_nonnull (str);
		g_assert_cmpstr (str, ==, tests[i].str);
	}
	for (guint i = 0; invalid[i] != NULL; i++) {
		g_autoptr(GError) error = NULL;
		g_autoptr(XbStack) opcodes = NULL;
		g_debug ("testing %s", invalid[i]);
		opcodes = xb_machine_parse_full (xb_silo_get_machine (silo),
						 invalid[i], -1,
						 XB_MACHINE_PARSE_FLAG_NONE,
						 &error);
		g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
		g_assert_null (opcodes);
	}
}

static void
xb_predicate_optimize_func (void)
{
	g_autoptr(XbSilo) silo = xb_silo_new ();
	struct {
		const gchar	*pred;
		const gchar	*str;
	} tests[] = {
		{ "@a='b'",		"'a',attr(),'b',eq()" },
		{ "'a'<'b'",		"True" },	/* success! */
		{ "999>=123",		"True" },	/* success! */
		{ "not(0)",		"True" },	/* success! */
		{ "lower-case('Fire')",	"'fire'" },
		{ "upper-case('Τάχιστη')", "'ΤΆΧΙΣΤΗ'" },
		{ "upper-case(lower-case('Fire'))",
					"'FIRE'" },	/* 2nd pass */
		/* sentinel */
		{ NULL, NULL }
	};
	const gchar *invalid[] = {
		"'a'='b'",
		"123>=999",
		"not(1)",
		NULL
	};
	xb_machine_set_debug_flags (xb_silo_get_machine (silo),
				    XB_MACHINE_DEBUG_FLAG_SHOW_STACK |
				    XB_MACHINE_DEBUG_FLAG_SHOW_OPTIMIZER);
	for (guint i = 0; tests[i].pred != NULL; i++) {
		g_autofree gchar *str = NULL;
		g_autoptr(GError) error = NULL;
		g_autoptr(XbStack) opcodes = NULL;

		g_debug ("testing %s", tests[i].pred);
		opcodes = xb_machine_parse_full (xb_silo_get_machine (silo),
						 tests[i].pred, -1,
						 XB_MACHINE_PARSE_FLAG_OPTIMIZE,
						 &error);
		g_assert_no_error (error);
		g_assert_nonnull (opcodes);
		str = xb_stack_to_string (opcodes);
		g_assert_nonnull (str);
		g_assert_cmpstr (str, ==, tests[i].str);
	}
	for (guint i = 0; invalid[i] != NULL; i++) {
		g_autoptr(GError) error = NULL;
		g_autoptr(XbStack) opcodes = NULL;
		opcodes = xb_machine_parse_full (xb_silo_get_machine (silo),
						 invalid[i], -1,
						 XB_MACHINE_PARSE_FLAG_OPTIMIZE,
						 &error);
		g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
		g_assert_null (opcodes);
	}
}

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
	silo = xb_silo_new_from_xml (xml, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);
	g_assert_true (xb_silo_is_valid (silo));

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
	g_assert_cmpint (g_bytes_get_size (bytes), ==, 620);
}

static void
xb_builder_ensure_invalidate_cb (XbSilo *silo, GParamSpec *pspec, gpointer user_data)
{
	guint *invalidate_cnt = (guint *) user_data;
	(*invalidate_cnt)++;
	xb_test_loop_quit ();
}

static GInputStream *
xb_builder_custom_mime_cb (XbBuilderSource *self,
			   XbBuilderSourceCtx *ctx,
			   gpointer user_data,
			   GCancellable *cancellable,
			   GError **error)
{
	gchar *xml = g_strdup_printf ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
				      "<component type=\"desktop\">"
				      "<id>%s</id></component>",
				      xb_builder_source_ctx_get_filename (ctx));
	return g_memory_input_stream_new_from_data (xml, -1, g_free);
}

static void
xb_builder_custom_mime_func (void)
{
	gboolean ret;
	g_autofree gchar *xml = NULL;
	g_autofree gchar *tmp_desktop = g_build_filename (g_get_tmp_dir (), "temp.desktop", NULL);
	g_autofree gchar *tmp_xmlb = g_build_filename (g_get_tmp_dir (), "temp.xmlb", NULL);
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file_desktop = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
	g_autoptr(XbSilo) silo = NULL;

	/* add support for desktop files */
	xb_builder_source_add_adapter (source, "application/x-desktop",
				       xb_builder_custom_mime_cb, NULL, NULL);

	/* import a source file */
	ret = g_file_set_contents (tmp_desktop, "[Desktop Entry]", -1, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	file_desktop = g_file_new_for_path (tmp_desktop);
	ret = xb_builder_source_load_file (source, file_desktop,
					   XB_BUILDER_SOURCE_FLAG_WATCH_FILE,
					   NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_import_source (builder, source);
	file = g_file_new_for_path (tmp_xmlb);
	silo = xb_builder_ensure (builder, file,
				  XB_BUILDER_COMPILE_FLAG_WATCH_BLOB,
				  NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* check contents */
	xml = xb_silo_export (silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml);
	g_print ("%s", xml);
	g_assert_cmpstr ("<component type=\"desktop\">"
			 "<id>temp.desktop</id>"
			 "</component>", ==, xml);

}

static void
xb_builder_chained_adapters_func (void)
{
	gboolean ret;
	g_autofree gchar *xml = NULL;
	g_autofree gchar *path = NULL;
	g_autofree gchar *tmp_xmlb = g_build_filename (g_get_tmp_dir (), "temp.xmlb", NULL);
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file_src = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
	g_autoptr(XbSilo) silo = NULL;

	/* import a source file */
	path = g_test_build_filename (G_TEST_DIST, "test.xml.gz.gz.gz", NULL);
	file_src = g_file_new_for_path (path);
	ret = xb_builder_source_load_file (source, file_src,
					   XB_BUILDER_SOURCE_FLAG_NONE,
					   NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_import_source (builder, source);
	file = g_file_new_for_path (tmp_xmlb);
	silo = xb_builder_ensure (builder, file,
				  XB_BUILDER_COMPILE_FLAG_NONE,
				  NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* check contents */
	xml = xb_silo_export (silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml);
	g_print ("%s", xml);
	g_assert_cmpstr ("<id>Hello world!</id>", ==, xml);

}

static void
xb_builder_ensure_watch_source_func (void)
{
	gboolean ret;
	guint invalidate_cnt = 0;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GFile) file_xml = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
	g_autoptr(XbSilo) silo = NULL;
	g_autofree gchar *tmp_xml = g_build_filename (g_get_tmp_dir (), "temp.xml", NULL);
	g_autofree gchar *tmp_xmlb = g_build_filename (g_get_tmp_dir (), "temp.xmlb", NULL);

#ifdef _WIN32
	/* no inotify */
	g_test_skip ("inotify does not work on mingw");
	return;
#endif

	/* import a source file */
	ret = g_file_set_contents (tmp_xml,
				   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				   "<id>gimp</id>", -1, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	file_xml = g_file_new_for_path (tmp_xml);
	ret = xb_builder_source_load_file (source, file_xml,
					   XB_BUILDER_SOURCE_FLAG_WATCH_FILE,
					   NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_import_source (builder, source);
	file = g_file_new_for_path (tmp_xmlb);
	g_file_delete (file, NULL, NULL);
	silo = xb_builder_ensure (builder, file,
				  XB_BUILDER_COMPILE_FLAG_WATCH_BLOB,
				  NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);
	g_assert_true (xb_silo_is_valid (silo));
	g_signal_connect (silo, "notify::valid",
			  G_CALLBACK (xb_builder_ensure_invalidate_cb),
			  &invalidate_cnt);

	/* change source file */
	ret = g_file_set_contents (tmp_xml,
				   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				   "<id>inkscape</id>", -1, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_test_loop_run_with_timeout (XB_SELF_TEST_INOTIFY_TIMEOUT);
	g_assert_false (xb_silo_is_valid (silo));
	g_assert_cmpint (invalidate_cnt, ==, 1);
	g_assert_false (xb_silo_is_valid (silo));
}

static void
xb_builder_ensure_func (void)
{
	gboolean ret;
	guint invalidate_cnt = 0;
	g_autofree gchar *tmp_xmlb = g_build_filename (g_get_tmp_dir (), "temp.xmlb", NULL);
	g_autoptr(GBytes) bytes1 = NULL;
	g_autoptr(GBytes) bytes2 = NULL;
	g_autoptr(GBytes) bytes3 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
		"<components origin=\"lvfs\">\n"
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
	g_test_skip ("inotify does not work on mingw");
	return;
#endif

	/* import some XML */
	xb_builder_set_profile_flags (builder, XB_SILO_PROFILE_FLAG_DEBUG);
	ret = xb_test_import_xml (builder, xml, &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	/* create file if it does not exist */
	file = g_file_new_for_path (tmp_xmlb);
	g_file_delete (file, NULL, NULL);
	silo = xb_builder_ensure (builder, file,
				  XB_BUILDER_COMPILE_FLAG_WATCH_BLOB,
				  NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);
	g_signal_connect (silo, "notify::valid",
			  G_CALLBACK (xb_builder_ensure_invalidate_cb),
			  &invalidate_cnt);
	g_assert_cmpint (invalidate_cnt, ==, 0);
	bytes1 = xb_silo_get_bytes (silo);

	/* recreate file if it is invalid */
	ret = g_file_replace_contents (file, "dave", 4, NULL, FALSE,
				       G_FILE_CREATE_NONE, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_test_loop_run_with_timeout (XB_SELF_TEST_INOTIFY_TIMEOUT);
	g_assert_false (xb_silo_is_valid (silo));
	g_assert_cmpint (invalidate_cnt, ==, 1);

	g_clear_object (&silo);
	silo = xb_builder_ensure (builder, file,
				  XB_BUILDER_COMPILE_FLAG_NONE,
				  NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);
	g_assert_true (xb_silo_is_valid (silo));
	bytes2 = xb_silo_get_bytes (silo);
	g_assert (bytes1 != bytes2);
	g_clear_object (&silo);

	/* don't recreate file if perfectly valid */
	silo = xb_builder_ensure (builder, file,
				  XB_BUILDER_COMPILE_FLAG_NONE,
				  NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);
	g_assert_true (xb_silo_is_valid (silo));
	bytes3 = xb_silo_get_bytes (silo);
	g_assert (bytes2 == bytes3);
	g_clear_object (&silo);
	g_clear_object (&builder);

	/* don't re-create for a new builder with the same XML added */
	builder = xb_builder_new ();
	ret = xb_test_import_xml (builder, xml, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	silo = xb_builder_ensure (builder, file,
				  XB_BUILDER_COMPILE_FLAG_NONE,
				  NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);
}

static gboolean
xb_builder_error_cb (XbBuilderFixup *self,
		     XbBuilderNode *bn,
		     gpointer user_data,
		     GError **error)
{
	g_set_error_literal (error,
			     G_IO_ERROR,
			     G_IO_ERROR_BUSY,
			     "engine was busy");
	return FALSE;
}

static void
xb_builder_node_vfunc_error_func (void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbBuilderFixup) fixup = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
	g_autoptr(XbSilo) silo = NULL;

	/* add fixup */
	fixup = xb_builder_fixup_new ("AlwaysError",
				      xb_builder_error_cb,
				      NULL, NULL);
	xb_builder_source_add_fixup (source, fixup);

	/* import some XML */
	ret = xb_builder_source_load_xml (source, "<id>gimp.desktop</id>",
					  XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_import_source (builder, source);
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NONE,
				   NULL, &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_BUSY);
	g_assert_null (silo);
}

static gboolean
xb_builder_upgrade_appstream_cb (XbBuilderFixup *self,
				 XbBuilderNode *bn,
				 gpointer user_data,
				 GError **error)
{
	if (g_strcmp0 (xb_builder_node_get_element (bn), "application") == 0) {
		g_autoptr(XbBuilderNode) id = xb_builder_node_get_child (bn, "id", NULL);
		g_autofree gchar *kind = NULL;
		if (id != NULL) {
			kind = g_strdup (xb_builder_node_get_attr (id, "type"));
			xb_builder_node_remove_attr (id, "type");
		}
		if (kind != NULL)
			xb_builder_node_set_attr (bn, "type", kind);
		xb_builder_node_set_element (bn, "component");
	} else if (g_strcmp0 (xb_builder_node_get_element (bn), "metadata") == 0) {
		xb_builder_node_set_element (bn, "custom");
	}
	return TRUE;
}

static void
xb_builder_node_vfunc_func (void)
{
	gboolean ret;
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbBuilderFixup) fixup = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
		"  <application>\n"
		"    <id type=\"desktop\">gimp.desktop</id>\n"
		"  </application>\n";

	/* add fixup */
	fixup = xb_builder_fixup_new ("AppStreamUpgrade",
				      xb_builder_upgrade_appstream_cb,
				      NULL, NULL);
	xb_builder_source_add_fixup (source, fixup);

	/* import some XML */
	ret = xb_builder_source_load_xml (source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_import_source (builder, source);
	silo = xb_builder_compile (builder,
				   XB_BUILDER_COMPILE_FLAG_NONE,
				   NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* check the XML */
	xml2 = xb_silo_export (silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml2);
	g_print ("%s\n", xml2);
	g_assert_cmpstr ("<component type=\"desktop\">"
			 "<id>gimp.desktop</id>"
			 "</component>", ==, xml2);
}

static gboolean
xb_builder_fixup_ignore_node_cb (XbBuilderFixup *self,
				 XbBuilderNode *bn,
				 gpointer user_data,
				 GError **error)
{
	if (g_strcmp0 (xb_builder_node_get_element (bn), "component") == 0) {
		g_autoptr(XbBuilderNode) id = xb_builder_node_get_child (bn, "id", NULL);
		if (g_strcmp0 (xb_builder_node_get_text (id), "gimp.desktop") == 0)
			xb_builder_node_add_flag (bn, XB_BUILDER_NODE_FLAG_IGNORE);
	} else {
		g_debug ("ignoring %s", xb_builder_node_get_element (bn));
	}
	return TRUE;
}

static void
xb_builder_node_vfunc_remove_func (void)
{
	gboolean ret;
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbBuilderFixup) fixup = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
		"  <components>\n"
		"    <component>\n"
		"      <id>gimp.desktop</id>\n"
		"    </component>\n"
		"    <component>\n"
		"      <id>inkscape.desktop</id>\n"
		"    </component>\n"
		"  </components>\n";

	/* add fixup */
	fixup = xb_builder_fixup_new ("RemoveGimp",
				      xb_builder_fixup_ignore_node_cb,
				      NULL, NULL);
	xb_builder_source_add_fixup (source, fixup);

	/* import some XML */
	ret = xb_builder_source_load_xml (source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_import_source (builder, source);
	silo = xb_builder_compile (builder,
				   XB_BUILDER_COMPILE_FLAG_NONE,
				   NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* check the XML */
	xml2 = xb_silo_export (silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml2);
	g_print ("%s\n", xml2);
	g_assert_cmpstr ("<components>"
			 "<component>"
			 "<id>inkscape.desktop</id>"
			 "</component>"
			 "</components>", ==, xml2);
}

static gboolean
xb_builder_fixup_root_node_only_cb (XbBuilderFixup *self,
				    XbBuilderNode *bn,
				    gpointer user_data,
				    GError **error)
{
	g_debug (">%s<", xb_builder_node_get_element (bn));
	g_assert_cmpstr (xb_builder_node_get_element (bn), ==, NULL);
	return TRUE;
}

static void
xb_builder_node_vfunc_depth_func (void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbBuilderFixup) fixup = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
		"  <components>\n"
		"    <component>\n"
		"      <id>gimp.desktop</id>\n"
		"    </component>\n"
		"  </components>\n";

	/* add fixup */
	fixup = xb_builder_fixup_new ("OnlyRoot",
				      xb_builder_fixup_root_node_only_cb,
				      NULL, NULL);
	xb_builder_fixup_set_max_depth (fixup, 0);
	xb_builder_source_add_fixup (source, fixup);

	/* import some XML */
	ret = xb_builder_source_load_xml (source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_import_source (builder, source);
	silo = xb_builder_compile (builder,
				   XB_BUILDER_COMPILE_FLAG_NONE,
				   NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);
}

static void
xb_builder_ignore_invalid_func (void)
{
	gboolean ret;
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbSilo) silo = NULL;

	/* import some correct XML */
	ret = xb_test_import_xml (builder, "<book><id>foobar</id></book>", &error);
	g_assert_no_error (error);
	g_assert_true (ret);

	/* import some incorrect XML */
	ret = xb_test_import_xml (builder, "<book><id>foobar</id>", &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	silo = xb_builder_compile (builder,
				   XB_BUILDER_COMPILE_FLAG_IGNORE_INVALID,
				   NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* check the XML */
	xml2 = xb_silo_export (silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml2);
	g_print ("%s\n", xml2);
	g_assert_cmpstr ("<book><id>foobar</id></book>", ==, xml2);
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
	g_assert_true (xb_silo_is_valid (silo));

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
	g_assert_null (xml);
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
xb_xpath_node_func (void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
	"<components origin=\"lvfs\">\n"
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

	/* get node */
	n = xb_silo_query_first (silo, "components/component", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_attr (n, "type"), ==, "desktop");

	/* query with text opcodes */
	results = xb_node_query (n, "id", 0, &error);
	g_assert_no_error (error);
	g_assert_nonnull (results);
	g_assert_cmpint (results->len, ==, 2);
}

static void
xb_node_data_func (void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	g_autoptr(GBytes) bytes = g_bytes_new ("foo", 4);

	/* import from XML */
	silo = xb_silo_new_from_xml ("<id>gimp.desktop</id>", &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* get node */
	n = xb_silo_query_first (silo, "id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	xb_node_set_data (n, "store", bytes);
	xb_node_set_data (n, "store", bytes);
	g_assert_nonnull (xb_node_get_data (n, "store"));
	g_assert_null (xb_node_get_data (n, "dave"));
}

static void
xb_node_export_func (void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	g_autofree gchar *xml_default = NULL;
	g_autofree gchar *xml_collapsed = NULL;

	/* import from XML */
	silo = xb_silo_new_from_xml ("<component attr1=\"val1\" attr2=\"val2\"/>", &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* get node */
	n = xb_silo_query_first (silo, "component", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);

	/* export default */
	xml_default = xb_node_export (n, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml_default);
	g_assert_cmpstr (xml_default, ==, "<component attr1=\"val1\" attr2=\"val2\"></component>");

	/* export collapsed */
	xml_collapsed = xb_node_export (n, XB_NODE_EXPORT_FLAG_COLLAPSE_EMPTY, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml_collapsed);
	g_assert_cmpstr (xml_collapsed, ==, "<component attr1=\"val1\" attr2=\"val2\" />");
}

static void
xb_xpath_parent_subnode_func (void)
{
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) children = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbNode) c = NULL;
	g_autoptr(XbNode) p = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
	"<components origin=\"lvfs\">\n"
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
	xb_silo_set_enable_node_cache (silo, TRUE);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* get node */
	n = xb_silo_query_first (silo, "components/component", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_attr (n, "type"), ==, "desktop");
	g_assert_cmpint (xb_node_get_depth (n), ==, 1);

	/* export a child */
	xml2 = xb_node_query_export (n, "id", &error);
	g_assert_cmpstr (xml2, ==, "<id>gimp.desktop</id>");

	/* get sibling */
	c = xb_node_get_next (n);
	g_assert_nonnull (c);
	g_assert_cmpstr (xb_node_get_attr (c, "type"), ==, "firmware");
	p = xb_node_get_next (c);
	g_assert_null (p);
	g_clear_object (&c);

	/* use the node to go back up the tree */
	c = xb_node_query_first (n, "..", &error);
	g_assert_no_error (error);
	g_assert_nonnull (c);
	g_assert_cmpstr (xb_node_get_attr (c, "origin"), ==, "lvfs");

	/* verify this is the parent */
	p = xb_node_get_root (n);
	g_assert_cmpint (xb_node_get_depth (p), ==, 0);
	g_assert (c == p);
	children = xb_node_get_children (p);
	g_assert_nonnull (children);
	g_assert_cmpint (children->len, ==, 2);
}

static void
xb_xpath_helpers_func (void)
{
	const gchar *tmp;
	guint64 val;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;

	/* import from XML */
	silo = xb_silo_new_from_xml ("<release><checksum size=\"123\">456</checksum></release>", &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* as char */
	n = xb_silo_get_root (silo);
	g_assert_nonnull (n);
	tmp = xb_node_query_text (n, "checksum", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (tmp, ==, "456");
	tmp = xb_node_query_attr (n, "checksum", "size", &error);
	g_assert_no_error (error);
	g_assert_cmpstr (tmp, ==, "123");

	/* as uint64 */
	val = xb_node_query_text_as_uint (n, "checksum", &error);
	g_assert_no_error (error);
	g_assert_cmpint (val, ==, 456);
	val = xb_node_query_attr_as_uint (n, "checksum", "size", &error);
	g_assert_no_error (error);
	g_assert_cmpint (val, ==, 123);
}

static void
xb_xpath_query_func (void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
	"<components>\n"
	"  <component>\n"
	"    <id>n/a</id>\n"
	"  </component>\n"
	"</components>\n";

	/* import from XML */
	silo = xb_silo_new_from_xml (xml, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* query with slash */
	n = xb_silo_query_first (silo, "components/component/id[text()='n\\/a']", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "n/a");
	g_clear_object (&n);

	/* query with an OR, where the first section contains an unknown element */
	n = xb_silo_query_first (silo, "components/dave|components/component/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "n/a");
	g_clear_object (&n);

	/* query with an OR, where the last section contains an unknown element */
	n = xb_silo_query_first (silo, "components/component/id|components/dave", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "n/a");
	g_clear_object (&n);

	/* query with an OR, all sections contains an unknown element */
	n = xb_silo_query_first (silo, "components/dave|components/mike", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null (n);
}

static void
xb_xpath_incomplete_func (void)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
	"<components>\n"
	"  <component>\n"
	"    <id>gimp.desktop</id>\n"
	"  </component>\n"
	"</components>\n";

	/* import from XML */
	silo = xb_silo_new_from_xml (xml, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* query with MISSING '[' */
	n = xb_silo_query_first (silo, "components/component/id[text()='dave'", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
	g_assert_null (n);
}

static gboolean
xb_builder_fixup_tokenize_cb (XbBuilderFixup *self,
				XbBuilderNode *bn,
				gpointer user_data,
				GError **error)
{
	if (g_strcmp0 (xb_builder_node_get_element (bn), "name") == 0)
		xb_builder_node_tokenize_text (bn);
	return TRUE;
}

static void
xb_xpath_func (void)
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
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbBuilderFixup) fixup = NULL;
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
	"<components origin=\"lvfs\">\n"
	"  <header>\n"
	"    <csum type=\"sha1\">dead</csum>\n"
	"  </header>\n"
	"  <component type=\"desktop\">\n"
	"    <id>gimp.desktop</id>\n"
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
	fixup = xb_builder_fixup_new ("TextTokenize", xb_builder_fixup_tokenize_cb, NULL, NULL);
	xb_builder_source_add_fixup (source, fixup);

	/* import from XML */
	ret = xb_builder_source_load_xml (source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_import_source (builder, source);
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* set up debugging */
	xb_machine_set_debug_flags (xb_silo_get_machine (silo),
				    XB_MACHINE_DEBUG_FLAG_SHOW_STACK);

	/* dump to screen */
	str = xb_silo_to_string (silo, &error);
	g_assert_no_error (error);
	g_assert_nonnull (str);
	g_debug ("\n%s", str);

	/* query with predicate logical and */
	n = xb_silo_query_first (silo, "components/component/custom/value[(@key='KEY') and (text()='TRUE')]/../../id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

	/* query with predicate logical and; failure */
	n = xb_silo_query_first (silo, "components/component/custom/value[(@key='KEY')&&(text()='FALSE')]/../../id", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null (n);
	g_clear_error (&error);
	g_clear_object (&n);

	/* query with predicate logical and, alternate form */
	n = xb_silo_query_first (silo, "components/component/custom/value[and((@key='KEY'),(text()='TRUE'))]/../../id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

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

	n = xb_silo_query_first (silo, "components/component[@type='dave']/id", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null (n);
	g_clear_error (&error);
	g_clear_object (&n);

	n = xb_silo_query_first (silo, "components/component[@percentage>=90]", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null (n);
	g_clear_error (&error);
	g_clear_object (&n);

	n = xb_silo_query_first (silo, "components/component/id[text()='dave']", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
	g_assert_null (n);
	g_clear_error (&error);
	g_clear_object (&n);

	/* query with attr opcodes */
	n = xb_silo_query_first (silo, "components/component[@type='firmware']/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object (&n);

	/* query with attr opcodes */
	n = xb_silo_query_first (silo, "components/component[@type!='firmware']/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

	/* query with attr opcodes with quotes */
	n = xb_silo_query_first (silo, "components/component[@type='firmware']/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object (&n);

	/* query with position */
	n = xb_silo_query_first (silo, "components/component[2]/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object (&n);

	/* last() with position */
	n = xb_silo_query_first (silo, "components/component[last()]/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object (&n);

	/* query with attr opcodes that exists */
	n = xb_silo_query_first (silo, "components/component[@type]/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

	/* query with attrs that do not exist */
	n = xb_silo_query_first (silo, "components/component[not(@dave)]/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

	/* query with wildcard with predicate */
	n = xb_silo_query_first (silo, "components/*[@type]/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

	/* query with text opcodes */
	n = xb_silo_query_first (silo, "components/header/csum[text()='dead']", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_attr (n, "type"), ==, "sha1");
	g_clear_object (&n);

	/* query with search */
	n = xb_silo_query_first (silo, "components/component/id[text()~='gimp']", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

	/* query no normalize */
	n = xb_silo_query_first (silo, "components/component/name", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "Mêẞ");
	g_clear_object (&n);

	/* query name not UTF-8 */
	n = xb_silo_query_first (silo, "components/component/name[text()~='mEss']/../id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

	/* query with stem */
#ifdef HAVE_LIBSTEMMER
	xb_machine_set_debug_flags (xb_silo_get_machine (silo),
				    XB_MACHINE_DEBUG_FLAG_SHOW_STACK |
				    XB_MACHINE_DEBUG_FLAG_SHOW_PARSING);
	n = xb_silo_query_first (silo, "components/component/id[text()~=stem('gimping')]", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);
	xb_machine_set_debug_flags (xb_silo_get_machine (silo),
				    XB_MACHINE_DEBUG_FLAG_SHOW_STACK);
#endif

	/* query with text:integer */
	n = xb_silo_query_first (silo, "components/component/id['123'=123]", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

	/* query with integer:text */
	n = xb_silo_query_first (silo, "components/component/id[123='123']", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

	/* query with prefix */
	n = xb_silo_query_first (silo, "components/component/id[starts-with(text(),'gimp')]", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

	/* query with suffix */
	n = xb_silo_query_first (silo, "components/component/id[ends-with(text(),'.desktop')]", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

	/* query with contains */
	n = xb_silo_query_first (silo, "components/component/id[contains(text(),'imp')]", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

	/* query with type-conversion */
	n = xb_silo_query_first (silo, "components/component[position()=number('2')]/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object (&n);

	/* query with another type-conversion */
	n = xb_silo_query_first (silo, "components/component['2'=string(2)]/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
	g_clear_object (&n);

	/* query with backtrack */
	g_debug ("\n%s", xml);
	n = xb_silo_query_first (silo, "components/component[@type='firmware']/id[text()='org.hughski.ColorHug2.firmware']", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object (&n);

	/* query with nesting */
	g_debug ("\n%s", xml);
	n = xb_silo_query_first (silo, "components/component/id[text()=lower-case(upper-case('Gimp.DESKTOP'))]", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "gimp.desktop");
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

	/* only children of parent */
	xml_sub3 = xb_node_export (n3, XB_NODE_EXPORT_FLAG_ONLY_CHILDREN, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml_sub3);
	g_assert_cmpstr (xml_sub3, ==, "<id>org.hughski.ColorHug2.firmware</id>");
}

static void
xb_builder_native_lang_func (void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *str = NULL;
	g_autofree gchar *tmp = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
	"<components>\n"
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
	ret = xb_test_import_xml (builder, xml, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_add_locale (builder, "fr_FR.UTF-8");
	xb_builder_add_locale (builder, "fr_FR");
	xb_builder_add_locale (builder, "fr_FR");
	xb_builder_add_locale (builder, "fr");
	xb_builder_add_locale (builder, "C");
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_SINGLE_LANG, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* test we removed other languages */
	str = xb_silo_to_string (silo, &error);
	g_assert_no_error (error);
	g_assert_nonnull (str);
	g_debug ("\n%s", str);
	g_assert_null (g_strstr_len (str, -1, "Wilcommen"));
	g_assert_null (g_strstr_len (str, -1, "Hello"));
	g_assert_nonnull (g_strstr_len (str, -1, "Salut"));
	g_assert_null (g_strstr_len (str, -1, "Goodbye"));
	g_assert_null (g_strstr_len (str, -1, "Auf Wiedersehen"));
	g_assert_nonnull (g_strstr_len (str, -1, "Au revoir"));

	/* test we traversed the tree correctly */
	n = xb_silo_query_first (silo, "components/component/*", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	tmp = xb_node_export (n, XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS, &error);
	g_assert_cmpstr (tmp, ==, "<p xml:lang=\"fr\">Salut</p><p xml:lang=\"fr\">Au revoir</p>");
}

static void
xb_builder_comments_func (void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *str = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
	"<?xml version=\"1.0\" ?>\n"
	"<components>\n"
	"  <!-- one -->\n"
	"  <!-- two -->\n"
	"</components>\n";

	/* import XML */
	ret = xb_test_import_xml (builder, xml, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* export */
	str = xb_silo_export (silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_cmpstr (str, ==, "<components></components>");
}

static void
xb_builder_native_lang2_func (void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *str = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
	"<components>\n"
	"  <component>\n"
	"    <description xml:lang=\"de_DE\"><p>Wilcommen</p></description>\n"
	"    <description><p>Hello</p></description>\n"
	"    <description xml:lang=\"fr\"><p>Salut</p></description>\n"
	"  </component>\n"
	"</components>\n";

	/* import from XML */
	ret = xb_test_import_xml (builder, xml, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_add_locale (builder, "fr_FR");
	xb_builder_add_locale (builder, "fr");
	xb_builder_add_locale (builder, "C");
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_SINGLE_LANG, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* test we removed other languages */
	str = xb_silo_to_string (silo, &error);
	g_assert_no_error (error);
	g_assert_nonnull (str);
	g_assert_null (g_strstr_len (str, -1, "Wilcommen"));
	g_assert_null (g_strstr_len (str, -1, "Hello"));
	g_assert_nonnull (g_strstr_len (str, -1, "Salut"));
	g_debug ("\n%s", str);
}

static void
xb_builder_native_lang_no_locales_func (void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbSilo) silo = NULL;

	/* import from XML */
	ret = xb_test_import_xml (builder, "<id>gimp.desktop</id>", &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_SINGLE_LANG, NULL, &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
	g_assert_null (silo);
}

static void
xb_xpath_parent_func (void)
{
	XbNode *n;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
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
	"    <pkgname>colorhug-client</pkgname>\n"
	"    <description xml:lang=\"de_DE\"><p>Wilcommen!</p></description>\n"
	"    <description><p>hello!</p></description>\n"
	"    <description xml:lang=\"fr_FR\"><p>Bonjour!</p></description>\n"
	"    <project_license>GPL-2.0</project_license>\n"
	"  </component>\n"
	"</components>\n";

	/* import from XML */
	ret = xb_test_import_xml (builder, xml, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_add_locale (builder, "C");
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* get node, no parent */
	n = xb_silo_query_first (silo, "components/component[@type='firmware']/id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "org.hughski.ColorHug2.firmware");
	g_assert_cmpstr (xb_node_get_element (n), ==, "id");
	g_clear_object (&n);

	/* get node, one parent */
	n = xb_silo_query_first (silo, "components/component[@type='firmware']/id/..", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_element (n), ==, "component");
	g_clear_object (&n);

	/* get node, multiple parents */
	n = xb_silo_query_first (silo, "components/component[@type='firmware']/id/../..", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_element (n), ==, "components");
	g_clear_object (&n);

	/* descend, ascend, descend */
	n = xb_silo_query_first (silo, "components/component[@type='firmware']/pkgname/../project_license", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "GPL-2.0");
	g_clear_object (&n);

	/* descend, ascend, descend */
	n = xb_silo_query_first (silo, "components/component/pkgname[text()~='colorhug']/../id", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "org.hughski.ColorHug2.firmware");
	g_clear_object (&n);

	/* get node, too many parents */
	n = xb_silo_query_first (silo, "components/component[@type='firmware']/id/../../..", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
	g_assert_null (n);
	g_clear_error (&error);

	/* can't go lower than root */
	n = xb_silo_query_first (silo, "..", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
	g_assert_null (n);
	g_clear_error (&error);

	/* fuzzy substring match */
	n = xb_silo_query_first (silo, "components/component/pkgname[text()~='colorhug']", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "colorhug-client");
	g_clear_object (&n);

	/* strlen match */
	n = xb_silo_query_first (silo, "components/component/pkgname[string-length(text())==15]", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "colorhug-client");
	g_clear_object (&n);

	/* fuzzy substring match */
	n = xb_silo_query_first (silo, "components/component[@type~='firm']/pkgname", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "colorhug-client");
	g_clear_object (&n);
}

static void
xb_xpath_prepared_func (void)
{
	XbNode *n;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbSilo) silo = NULL;
	g_autoptr(XbQuery) query = NULL;
	g_auto(XbQueryContext) context = XB_QUERY_CONTEXT_INIT ();
	g_autoptr(XbNode) component = NULL;
	g_autoptr(GPtrArray) components = NULL;
	const gchar *xml =
	"<components origin=\"lvfs\">\n"
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
	ret = xb_test_import_xml (builder, xml, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_add_locale (builder, "C");
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* get first component */
	component = xb_silo_query_first (silo, "components/component", &error);
	g_assert_no_error (error);
	g_assert_nonnull (component);

	/* prepared statement on node */
	query = xb_query_new (silo, "id[text()=?]/..", &error);
	g_assert_no_error (error);
	g_assert_nonnull (query);
	xb_value_bindings_bind_str (xb_query_context_get_bindings (&context), 0, "gimp.desktop", NULL);
	components = xb_node_query_with_context (component, query, &context, &error);
	g_assert_no_error (error);
	g_assert_nonnull (components);
	g_assert_cmpint (components->len, ==, 1);
	n = g_ptr_array_index (components, 0);
	g_assert_cmpstr (xb_node_get_attr (n, "type"), ==, "desktop");
}

static void
xb_xpath_query_reverse_func (void)
{
	XbNode *n;
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbSilo) silo = NULL;
	g_autoptr(XbQuery) query = NULL;
	g_autoptr(GPtrArray) names = NULL;
	const gchar *xml =
	"<names>\n"
	"  <name>foo</name>\n"
	"  <name>bar</name>\n"
	"  <name>baz</name>\n"
	"</names>\n";

	/* import from XML */
	ret = xb_test_import_xml (builder, xml, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* get first when reversed */
	query = xb_query_new_full (silo, "names/name", XB_QUERY_FLAG_REVERSE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (query);
	names = xb_silo_query_with_context (silo, query, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (names);
	g_assert_cmpint (names->len, ==, 3);
	n = g_ptr_array_index (names, 0);
	g_assert_cmpstr (xb_node_get_text (n), ==, "baz");
}

static void
xb_xpath_query_force_node_cache_func (void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbNode) n1 = NULL;
	g_autoptr(XbNode) n2 = NULL;
	g_autoptr(XbQuery) query = NULL;
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
	"<names>\n"
	"  <name>foo</name>\n"
	"</names>\n";

	/* import from XML */
	ret = xb_test_import_xml (builder, xml, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* use a cache for this specific result */
	query = xb_query_new_full (silo, "names/name",
				   XB_QUERY_FLAG_FORCE_NODE_CACHE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (query);
	n1 = xb_silo_query_first_with_context (silo, query, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (n1);
	n2 = xb_silo_query_first_with_context (silo, query, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (n2);
	g_assert (n1 == n2);
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
	n = xb_silo_query_first (silo, "components/component[@type='desktop']/*", &error);
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
	ret = xb_test_import_xml (builder, "<tag>value</tag>", &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	ret = xb_test_import_xml (builder, "<tag>value2</tag><tag>value3</tag>", &error);
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
	g_assert_cmpstr ("<tag>value</tag><tag>value2</tag><tag>value3</tag>", ==, xml_new);

	/* query for multiple results */
	results = xb_silo_query (silo, "tag", 5, &error);
	g_assert_no_error (error);
	g_assert_nonnull (results);
	g_assert_cmpint (results->len, ==, 3);
}

static void
xb_builder_node_func (void)
{
	g_autofree gchar *str = NULL;
	g_autofree gchar *xml = NULL;
	g_autofree gchar *xml_src = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbBuilderNode) child_by_element = NULL;
	g_autoptr(XbBuilderNode) child_by_text = NULL;
	g_autoptr(XbBuilderNode) component = NULL;
	g_autoptr(XbBuilderNode) components = NULL;
	g_autoptr(XbBuilderNode) id = NULL;
	g_autoptr(XbBuilderNode) root = xb_builder_node_new (NULL);
	g_autoptr(XbSilo) silo = NULL;

	/* create a simple document */
	components = xb_builder_node_insert (root, "components",
					     "origin", "lvfs",
					     NULL);
	g_assert_cmpint (xb_builder_node_depth (components), ==, 1);
	component = xb_builder_node_insert (components, "component", NULL);
	g_assert_cmpint (xb_builder_node_depth (component), ==, 2);
	xb_builder_node_set_attr (component, "type", "firmware");
	xb_builder_node_set_attr (component, "type", "desktop");
	g_assert_cmpstr (xb_builder_node_get_attr (component, "type"), ==, "desktop");
	g_assert_cmpstr (xb_builder_node_get_attr (component, "dave"), ==, NULL);
	id = xb_builder_node_new ("id");
	xb_builder_node_add_flag (id, XB_BUILDER_NODE_FLAG_TOKENIZE_TEXT);
	xb_builder_node_add_token (id, "foobarbaz");
	xb_builder_node_add_child (component, id);
	xb_builder_node_set_text (id, "gimp.desktop", -1);
	xb_builder_node_insert_text (component, "icon", "dave", "type", "stock", NULL);
	g_assert_cmpint (xb_builder_node_depth (id), ==, 3);

	/* get specific child */
	child_by_element = xb_builder_node_get_child (components, "component", NULL);
	g_assert_nonnull (child_by_element);
	g_assert_cmpstr (xb_builder_node_get_element (child_by_element), ==, "component");
	child_by_text = xb_builder_node_get_child (component, "id", "gimp.desktop");
	g_assert_nonnull (child_by_text);
	g_assert_cmpstr (xb_builder_node_get_element (child_by_text), ==, "id");

	/* check the source XML */
	xml_src = xb_builder_node_export (components,
					  XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE,
					  &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml_src);
	g_print ("%s", xml_src);
	g_assert_cmpstr ("<components origin=\"lvfs\">\n"
			 "<component type=\"desktop\">\n"
			 "<id>gimp.desktop</id>\n"
			 "<icon type=\"stock\">dave</icon>\n"
			 "</component>\n"
			 "</components>\n", ==, xml_src);

	/* import the doc */
	xb_builder_import_node (builder, root);
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* to console */
	str = xb_silo_to_string (silo, &error);
	g_assert_no_error (error);
	g_assert_nonnull (str);
	g_debug ("%s", str);

	/* check the XML */
	xml = xb_silo_export (silo, XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml);
	g_print ("%s", xml);
	g_assert_cmpstr ("<components origin=\"lvfs\">"
			 "<component type=\"desktop\">"
			 "<id>gimp.desktop</id>"
			 "<icon type=\"stock\">dave</icon>"
			 "</component>"
			 "</components>", ==, xml);
}

static void
xb_builder_node_literal_text_func (void)
{
	gboolean ret;
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
		"  <component>\n"
		"    <description>\n"
		"      <p>Really long content\n"
		"spanning multiple lines\n"
		"</p>\n"
		"    </description>\n"
		"  </component>\n";

	/* import some XML */
	ret = xb_builder_source_load_xml (source, xml, XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_import_source (builder, source);
	silo = xb_builder_compile (builder,
				   XB_BUILDER_COMPILE_FLAG_NONE,
				   NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* check the XML */
	xml2 = xb_silo_export (silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml2);
	g_print ("%s\n", xml2);
	g_assert_cmpstr ("<component>"
			 "<description><p>Really long content\nspanning multiple lines\n</p></description>"
			 "</component>", ==, xml2);
}

static void
xb_builder_node_source_text_func (void)
{
	gboolean ret;
	g_autofree gchar *xml2 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
	g_autoptr(XbSilo) silo = NULL;
	const gchar *xml =
		"  <component>\n"
		"    <description>\n"
		"      <p>Really long content\n"
		"spanning multiple lines\n"
		"</p>\n"
		"    </description>\n"
		"  </component>\n";

	/* import some XML */
	ret = xb_builder_source_load_xml (source, xml, XB_BUILDER_SOURCE_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_import_source (builder, source);
	silo = xb_builder_compile (builder,
				   XB_BUILDER_COMPILE_FLAG_NONE,
				   NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* check the XML */
	xml2 = xb_silo_export (silo, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml2);
	g_print ("%s\n", xml2);
	g_assert_cmpstr ("<component>"
			 "<description><p>Really long content spanning multiple lines</p></description>"
			 "</component>", ==, xml2);
}

static void
xb_builder_node_info_func (void)
{
	gboolean ret;
	g_autofree gchar *xml = NULL;
	g_autofree gchar *tmp_xml = g_build_filename (g_get_tmp_dir (), "temp.xml", NULL);
	g_autoptr(GError) error = NULL;
	g_autoptr(XbBuilderSource) import1 = xb_builder_source_new ();
	g_autoptr(XbBuilderSource) import2 = xb_builder_source_new ();
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbBuilderNode) info1 = NULL;
	g_autoptr(XbBuilderNode) info2 = NULL;
	g_autoptr(XbSilo) silo = NULL;
	g_autoptr(GFile) file = NULL;

	/* create a simple document with some info */
	ret = g_file_set_contents (tmp_xml,
				   "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				   "<component><id type=\"desktop\">dave</id></component>",
				   -1, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	info1 = xb_builder_node_insert (NULL, "info", NULL);
	xb_builder_node_insert_text (info1, "scope", "user", NULL);
	info2 = xb_builder_node_insert (NULL, "info", NULL);
	xb_builder_node_insert_text (info2, "scope", "system", NULL);

	/* import the doc */
	file = g_file_new_for_path (tmp_xml);
	ret = xb_builder_source_load_file (import1, file, XB_BUILDER_SOURCE_FLAG_NONE, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_source_set_info (import1, info1);
	xb_builder_source_set_prefix (import1, "local");
	xb_builder_import_source (builder, import1);
	ret = xb_builder_source_load_file (import2, file, XB_BUILDER_SOURCE_FLAG_NONE, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	xb_builder_source_set_info (import2, info2);
	xb_builder_source_set_prefix (import2, "local");
	xb_builder_import_source (builder, import2);
	silo = xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* get info */
	n = xb_silo_query_first (silo, "local/component/id[text()='dave']/../info/scope", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_assert_cmpstr (xb_node_get_text (n), ==, "user");

	/* check the XML */
	xml = xb_silo_export (silo, XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS, &error);
	g_assert_no_error (error);
	g_assert_nonnull (xml);
	g_assert_cmpstr ("<local>"
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
			 "</local>"
			 , ==, xml);
}

static void
xb_threading_cb (gpointer data, gpointer user_data)
{
	XbSilo *silo = XB_SILO (user_data);
	gint i = g_random_int_range (0, 50);
	g_autofree gchar *xpath = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) components = NULL;

	/* do query */
	xpath = g_strdup_printf ("components/component/id[text()='%06i.firmware']", i);
	components = xb_silo_query (silo, xpath, 0, &error);
	g_assert_no_error (error);
	g_assert_nonnull (components);
	g_assert_cmpint (components->len, ==, 1);
	g_print (".");
}

static void
xb_threading_func (void)
{
	GThreadPool *pool;
	gboolean ret;
	guint n_components = 10000;
	g_autoptr(GError) error = NULL;
	g_autoptr(GString) xml = g_string_new (NULL);
	g_autoptr(XbSilo) silo = NULL;

#ifdef __s390x__
	/* this is run with qemu and takes too much time */
	g_test_skip ("s390 too slow, skipping");
	return;
#endif

	/* create a huge document */
	g_string_append (xml, "<components>");
	for (guint i = 0; i < n_components; i++) {
		g_string_append (xml, "<component>");
		g_string_append_printf (xml, "  <id>%06u.firmware</id>", i);
		g_string_append (xml, "  <name>ColorHug2</name>");
		g_string_append (xml, "  <summary>Firmware</summary>");
		g_string_append (xml, "  <description><p>New features!</p></description>");
		g_string_append (xml, "</component>");
	}
	g_string_append (xml, "</components>");

	/* import from XML */
	silo = xb_silo_new_from_xml (xml->str, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* create thread pool */
	pool = g_thread_pool_new (xb_threading_cb, silo, 20, TRUE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (pool);

	/* run threads */
	for (guint i = 0; i < 100; i++) {
		ret = g_thread_pool_push (pool, &i, &error);
		g_assert_no_error (error);
		g_assert_true (ret);
	}
	g_thread_pool_free (pool, FALSE, TRUE);
}

typedef struct {
	guint		 cnt;
	GString		*str;
} XbMarkupHelper;

static gboolean
xb_markup_head_cb (XbNode *n, gpointer user_data)
{
	XbMarkupHelper *helper = (XbMarkupHelper *) user_data;
	helper->cnt++;

	if (xb_node_get_text (n) == NULL)
		return FALSE;

	/* start */
	if (g_strcmp0 (xb_node_get_element (n), "em") == 0) {
		g_string_append (helper->str, "*");
	} else if (g_strcmp0 (xb_node_get_element (n), "strong") == 0) {
		g_string_append (helper->str, "**");
	} else if (g_strcmp0 (xb_node_get_element (n), "code") == 0) {
		g_string_append (helper->str, "`");
	}

	/* text */
	if (xb_node_get_text (n) != NULL)
		g_string_append (helper->str, xb_node_get_text (n));

	return FALSE;
}

static gboolean
xb_markup_tail_cb (XbNode *n, gpointer user_data)
{
	XbMarkupHelper *helper = (XbMarkupHelper *) user_data;
	helper->cnt++;

	/* end */
	if (g_strcmp0 (xb_node_get_element (n), "em") == 0) {
		g_string_append (helper->str, "*");
	} else if (g_strcmp0 (xb_node_get_element (n), "strong") == 0) {
		g_string_append (helper->str, "**");
	} else if (g_strcmp0 (xb_node_get_element (n), "code") == 0) {
		g_string_append (helper->str, "`");
	} else if (g_strcmp0 (xb_node_get_element (n), "p") == 0) {
		g_string_append (helper->str, "\n\n");
	}

	/* tail */
	if (xb_node_get_tail (n) != NULL)
		g_string_append (helper->str, xb_node_get_tail (n));

	return FALSE;
}

static void
xb_markup_func (void)
{
	gboolean ret;
	g_autofree gchar *new = NULL;
	g_autofree gchar *tmp = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(XbNode) n = NULL;
	g_autoptr(XbSilo) silo = NULL;
	XbMarkupHelper helper = {
		.cnt = 0,
		.str = g_string_new (NULL),
	};
	const gchar *xml = "<description>"
			   "<p><code>Title</code>:</p>"
			   "<p>There is a <em>slight</em> risk of <strong>death</strong> here<a>!</a></p>"
			   "</description>";

	/* import from XML */
	silo = xb_silo_new_from_xml (xml, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);

	/* ensure we can round-trip */
	tmp = xb_silo_to_string (silo, &error);
	g_assert_no_error (error);
	g_assert_nonnull (silo);
	g_debug ("\n%s", tmp);
	n = xb_silo_get_root (silo);
	g_assert_nonnull (n);
	new = xb_node_export (n, XB_NODE_EXPORT_FLAG_NONE, &error);
	g_assert_no_error (error);
	g_assert_nonnull (new);
	g_assert_cmpstr (xml, ==, new);

	/* ensure we can convert this to another format */
	ret = xb_node_transmogrify (n, xb_markup_head_cb, xb_markup_tail_cb, &helper);
	g_assert_true (ret);
	g_assert_cmpstr (helper.str->str, ==,
			 "`Title`:\n\nThere is a *slight* risk of **death** here!\n\n");
	g_assert_cmpint (helper.cnt, ==, 14);
	g_string_free (helper.str, TRUE);
}

static void
xb_speed_func (void)
{
	XbNode *n;
	gboolean ret;
	guint n_components = 5000;
	g_autofree gchar *tmp_xmlb = g_build_filename (g_get_tmp_dir (), "test.xmlb", NULL);
	g_autofree gchar *xpath1 = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(GString) xml = g_string_new (NULL);
	g_autoptr(GTimer) timer = g_timer_new ();
	g_autoptr(XbSilo) silo = NULL;

#ifdef __s390x__
	/* this is run with qemu and takes too much time */
	g_test_skip ("s390 too slow, skipping");
	return;
#endif

	/* create a huge document */
	g_string_append (xml, "<components>");
	for (guint i = 0; i < n_components; i++) {
		g_string_append (xml, "<component type=\"firmware\">");
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
	file = g_file_new_for_path (tmp_xmlb);
	ret = xb_silo_save_to_file (silo, file, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_clear_object (&silo);
	g_print ("import+save: %.3fms\n", g_timer_elapsed (timer, NULL) * 1000);
	g_timer_reset (timer);

	/* load from file */
	silo = xb_silo_new ();
	ret = xb_silo_load_from_file (silo, file, XB_SILO_LOAD_FLAG_NONE, NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_print ("mmap load: %.3fms\n", g_timer_elapsed (timer, NULL) * 1000);
	g_timer_reset (timer);

	/* query best case */
	n = xb_silo_query_first (silo, "components/component/id[text()='000000.firmware']", &error);
	g_assert_no_error (error);
	g_assert_nonnull (n);
	g_print ("query[first]: %.3fms\n", g_timer_elapsed (timer, NULL) * 1000);
	g_timer_reset (timer);
	g_clear_object (&n);

	/* query worst case */
	xpath1 = g_strdup_printf ("components/component/id[text()='%06u.firmware']", n_components - 1);
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
		xpath2 = g_strdup_printf ("components/component[@type='firmware']/id[text()='%06u.firmware']", i);
		n = xb_silo_query_first (silo, xpath2, &error);
		g_assert_no_error (error);
		g_assert_nonnull (n);
		g_clear_object (&n);
	}
	g_print ("query[x%u]: %.3fms\n", n_components, g_timer_elapsed (timer, NULL) * 1000);
	g_timer_reset (timer);

	/* factorial search, again */
	for (guint i = 0; i < n_components; i += 20) {
		g_autofree gchar *xpath2 = NULL;
		xpath2 = g_strdup_printf ("components/component[@type='firmware']/id[text()='%06u.firmware']", i);
		n = xb_silo_query_first (silo, xpath2, &error);
		g_assert_no_error (error);
		g_assert_nonnull (n);
		g_clear_object (&n);
	}
	g_print ("query[x%u]: %.3fms\n", n_components, g_timer_elapsed (timer, NULL) * 1000);
	g_timer_reset (timer);

	/* create an index */
	ret = xb_silo_query_build_index (silo, "components/component/id", NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	ret = xb_silo_query_build_index (silo, "components/component/id[text()='dave']", NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	ret = xb_silo_query_build_index (silo, "components/component/DAVE", NULL, &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	ret = xb_silo_query_build_index (silo, "components/component", "type", &error);
	g_assert_no_error (error);
	g_assert_true (ret);
	g_print ("create index: %.3fms\n", g_timer_elapsed (timer, NULL) * 1000);
	g_timer_reset (timer);

	/* index not found */
	n = xb_silo_query_first (silo, "components[text()=$'dave']", &error);
	g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
	g_assert_null (n);
	g_clear_error (&error);

	/* do the search again, this time with an index */
	g_timer_reset (timer);
	for (guint i = 0; i < n_components; i += 20) {
		g_autofree gchar *xpath2 = NULL;
		xpath2 = g_strdup_printf ("components/component[attr($'type')=$'firmware']/id[text()=$'%06u.firmware']", i);
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

	setlocale (LC_ALL, "");

	/* tests go here */
	g_test_add_func ("/libxmlb/common", xb_common_func);
	g_test_add_func ("/libxmlb/common{searchv}", xb_common_searchv_func);
	g_test_add_func ("/libxmlb/common{union}", xb_common_union_func);
	g_test_add_func ("/libxmlb/opcodes", xb_predicate_func);
	g_test_add_func ("/libxmlb/opcodes{optimize}", xb_predicate_optimize_func);
	g_test_add_func ("/libxmlb/opcodes{kind}", xb_opcodes_kind_func);
	g_test_add_func ("/libxmlb/stack", xb_stack_func);
	g_test_add_func ("/libxmlb/stack{peek}", xb_stack_peek_func);
	g_test_add_func ("/libxmlb/node{data}", xb_node_data_func);
	g_test_add_func ("/libxmlb/node{export}", xb_node_export_func);
	g_test_add_func ("/libxmlb/builder", xb_builder_func);
	g_test_add_func ("/libxmlb/builder{comments}", xb_builder_comments_func);
	g_test_add_func ("/libxmlb/builder{native-lang}", xb_builder_native_lang_func);
	g_test_add_func ("/libxmlb/builder{native-lang-nested}", xb_builder_native_lang2_func);
	g_test_add_func ("/libxmlb/builder{native-lang-locale}", xb_builder_native_lang_no_locales_func);
	g_test_add_func ("/libxmlb/builder{empty}", xb_builder_empty_func);
	g_test_add_func ("/libxmlb/builder{ensure}", xb_builder_ensure_func);
	g_test_add_func ("/libxmlb/builder{ensure-watch-source}", xb_builder_ensure_watch_source_func);
	g_test_add_func ("/libxmlb/builder{node-vfunc}", xb_builder_node_vfunc_func);
	g_test_add_func ("/libxmlb/builder{node-vfunc-remove}", xb_builder_node_vfunc_remove_func);
	g_test_add_func ("/libxmlb/builder{node-vfunc-depth}", xb_builder_node_vfunc_depth_func);
	g_test_add_func ("/libxmlb/builder{node-vfunc-error}", xb_builder_node_vfunc_error_func);
	g_test_add_func ("/libxmlb/builder{ignore-invalid}", xb_builder_ignore_invalid_func);
	g_test_add_func ("/libxmlb/builder{custom-mime}", xb_builder_custom_mime_func);
	g_test_add_func ("/libxmlb/builder{chained-adapters}", xb_builder_chained_adapters_func);
	g_test_add_func ("/libxmlb/builder-node", xb_builder_node_func);
	g_test_add_func ("/libxmlb/builder-node{info}", xb_builder_node_info_func);
	g_test_add_func ("/libxmlb/builder-node{literal-text}", xb_builder_node_literal_text_func);
	g_test_add_func ("/libxmlb/builder-node{source-text}", xb_builder_node_source_text_func);
	g_test_add_func ("/libxmlb/markup", xb_markup_func);
	g_test_add_func ("/libxmlb/xpath", xb_xpath_func);
	g_test_add_func ("/libxmlb/xpath-query", xb_xpath_query_func);
	g_test_add_func ("/libxmlb/xpath-query{reverse}", xb_xpath_query_reverse_func);
	g_test_add_func ("/libxmlb/xpath-query{force-node-cache}", xb_xpath_query_force_node_cache_func);
	g_test_add_func ("/libxmlb/xpath{helpers}", xb_xpath_helpers_func);
	g_test_add_func ("/libxmlb/xpath{prepared}", xb_xpath_prepared_func);
	g_test_add_func ("/libxmlb/xpath{incomplete}", xb_xpath_incomplete_func);
	g_test_add_func ("/libxmlb/xpath-parent", xb_xpath_parent_func);
	g_test_add_func ("/libxmlb/xpath-glob", xb_xpath_glob_func);
	g_test_add_func ("/libxmlb/xpath-node", xb_xpath_node_func);
	g_test_add_func ("/libxmlb/xpath-parent-subnode", xb_xpath_parent_subnode_func);
	g_test_add_func ("/libxmlb/multiple-roots", xb_builder_multiple_roots_func);
	if (g_test_perf ())
		g_test_add_func ("/libxmlb/threading", xb_threading_func);
	if (g_test_perf ())
		g_test_add_func ("/libxmlb/speed", xb_speed_func);
	return g_test_run ();
}
