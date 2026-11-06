/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>

#include "xb-machine.h"
#include "xb-opcode-private.h"
#include "xb-silo-private.h"
#include "xb-stack-private.h"
#include "xb-test-common.h"

static void
xb_opcodes_kind_func(void)
{
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT();
	g_auto(XbOpcode) op3 = XB_OPCODE_INIT();
	const XbOpcodeKind roundtrip_kinds[] = {
	    XB_OPCODE_KIND_INTEGER,
	    XB_OPCODE_KIND_TEXT,
	    XB_OPCODE_KIND_FUNCTION,
	    XB_OPCODE_KIND_BOUND_UNSET,
	    XB_OPCODE_KIND_BOUND_INTEGER,
	    XB_OPCODE_KIND_BOUND_TEXT,
	    XB_OPCODE_KIND_BOUND_INDEXED_TEXT,
	    XB_OPCODE_KIND_INDEXED_TEXT,
	    XB_OPCODE_KIND_BOOLEAN,
	};

	xb_opcode_func_init(&op1, 0);
	xb_opcode_integer_init(&op2, 1);
	xb_opcode_text_init(&op3, "dave");

	/* check kind */
	g_assert_cmpint(xb_opcode_get_kind(&op1), ==, XB_OPCODE_KIND_FUNCTION);
	g_assert_cmpint(xb_opcode_get_kind(&op2), ==, XB_OPCODE_KIND_INTEGER);
	g_assert_cmpint(xb_opcode_get_kind(&op3), ==, XB_OPCODE_KIND_TEXT);

	/* to and from string */
	g_assert_cmpint(xb_opcode_kind_from_string("TEXT"), ==, XB_OPCODE_KIND_TEXT);
	g_assert_cmpint(xb_opcode_kind_from_string("FUNC"), ==, XB_OPCODE_KIND_FUNCTION);
	g_assert_cmpint(xb_opcode_kind_from_string("INTE"), ==, XB_OPCODE_KIND_INTEGER);
	g_assert_cmpint(xb_opcode_kind_from_string("dave"), ==, XB_OPCODE_KIND_UNKNOWN);
	g_assert_cmpstr(xb_opcode_kind_to_string(XB_OPCODE_KIND_TEXT), ==, "TEXT");
	g_assert_cmpstr(xb_opcode_kind_to_string(XB_OPCODE_KIND_FUNCTION), ==, "FUNC");
	g_assert_cmpint(xb_opcode_kind_from_string("BIND"), ==, XB_OPCODE_KIND_BOUND_UNSET);
	g_assert_cmpstr(xb_opcode_kind_to_string(XB_OPCODE_KIND_INTEGER), ==, "INTE");
	g_assert_cmpstr(xb_opcode_kind_to_string(XB_OPCODE_KIND_UNKNOWN), ==, NULL);

	/* the kind<->string mapping must round-trip for every real kind */
	for (gsize i = 0; i < G_N_ELEMENTS(roundtrip_kinds); i++) {
		const gchar *str = xb_opcode_kind_to_string(roundtrip_kinds[i]);
		g_assert_nonnull(str);
		g_assert_cmpint(xb_opcode_kind_from_string(str), ==, roundtrip_kinds[i]);
	}

	/* integer compare */
	g_assert_false(xb_opcode_cmp_val(&op1));
	g_assert_true(xb_opcode_cmp_val(&op2));
	g_assert_false(xb_opcode_cmp_val(&op3));

	/* string compare */
	g_assert_false(xb_opcode_cmp_str(&op1));
	g_assert_false(xb_opcode_cmp_str(&op2));
	g_assert_true(xb_opcode_cmp_str(&op3));
}

static void
xb_predicate_func(void)
{
	g_autoptr(XbSilo) silo = xb_silo_new();
	struct {
		const gchar *pred;
		const gchar *str;
	} tests[] = {{"'a'='b'", "'a','b',eq()"},
		     {"@a='b'", "'a',attr(),'b',eq()"},
		     {"@a=='b'", "'a',attr(),'b',eq()"},
		     {"'a'<'b'", "'a','b',lt()"},
		     {"999>=123", "999,123,ge()"},
		     {"not(0)", "0^1,not()"},
		     {"@a", "'a',attr(),'(null)',ne()"},
		     {"not(@a)", "'a'^1,attr()^1,not()"},
		     {"'a'=", "'a',eq()"},
		     {"='b'", "'b',eq()"},
		     {"999=\'b\'", "999,'b',eq()"},
		     {"text()=\'b\'", "text(),'b',eq()"},
		     {"last()", "last()"},
		     {"text()~='beef'", "text(),'beef'[beef],search()"},
		     {"@type~='dead'", "'type',attr(),'dead',search()"},
		     {"2", "2,position(),eq()"},
		     {"text()=lower-case('firefox')", "text(),'firefox'^1,lower-case(),eq()"},
		     {"$'a'=$'b'", "$'a',$'b',eq()"},
		     {"('a'='b')&&('c'='d')", "'a'^1,'b'^1,eq()^1,'c'^1,'d'^1,eq()^1,and()"},
		     {"text()==('a','b','c')", "text(),'a'^1,'b'^1,'c'^1,in()"},
		     /* sentinel */
		     {NULL, NULL}};
	const gchar *invalid[] = {"text(",
				  "text((((((((((((((((((((text()))))))))))))))))))))",
				  NULL};
	xb_machine_set_debug_flags(xb_silo_get_machine(silo),
				   XB_MACHINE_DEBUG_FLAG_SHOW_STACK |
				       XB_MACHINE_DEBUG_FLAG_SHOW_PARSING);
	for (guint i = 0; tests[i].pred != NULL; i++) {
		g_autofree gchar *str = NULL;
		g_autoptr(GError) error = NULL;
		g_autoptr(XbStack) opcodes = NULL;

		g_debug("testing %s", tests[i].pred);
		opcodes = xb_machine_parse_full(xb_silo_get_machine(silo),
						tests[i].pred,
						-1,
						XB_MACHINE_PARSE_FLAG_NONE,
						&error);
		g_assert_no_error(error);
		g_assert_nonnull(opcodes);
		str = xb_stack_to_string(opcodes);
		g_assert_nonnull(str);
		g_assert_cmpstr(str, ==, tests[i].str);
	}
	for (guint i = 0; invalid[i] != NULL; i++) {
		g_autoptr(GError) error = NULL;
		g_autoptr(XbStack) opcodes = NULL;
		g_debug("testing %s", invalid[i]);
		opcodes = xb_machine_parse_full(xb_silo_get_machine(silo),
						invalid[i],
						-1,
						XB_MACHINE_PARSE_FLAG_NONE,
						&error);
		g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
		g_assert_null(opcodes);
	}
}

static void
xb_predicate_optimize_func(void)
{
	g_autoptr(XbSilo) silo = xb_silo_new();
	struct {
		const gchar *pred;
		const gchar *str;
	} tests[] = {{"@a='b'", "'a',attr(),'b',eq()"},
		     {"'a'<'b'", "True"},  /* success! */
		     {"999>=123", "True"}, /* success! */
		     {"not(0)", "True"},   /* success! */
		     {"lower-case('Fire')", "'fire'"},
		     {"upper-case('Τάχιστη')", "'ΤΆΧΙΣΤΗ'"},
		     {"upper-case(lower-case('Fire'))", "'FIRE'"}, /* 2nd pass */
		     {"text()==('a','b','c')", "text(),'a'^1,'b'^1,'c'^1,in()"},
		     /* sentinel */
		     {NULL, NULL}};
	const gchar *invalid[] = {"'a'='b'", "123>=999", "not(1)", NULL};
	xb_machine_set_debug_flags(xb_silo_get_machine(silo),
				   XB_MACHINE_DEBUG_FLAG_SHOW_STACK |
				       XB_MACHINE_DEBUG_FLAG_SHOW_OPTIMIZER);
	for (guint i = 0; tests[i].pred != NULL; i++) {
		g_autofree gchar *str = NULL;
		g_autoptr(GError) error = NULL;
		g_autoptr(XbStack) opcodes = NULL;

		g_debug("testing %s", tests[i].pred);
		opcodes = xb_machine_parse_full(xb_silo_get_machine(silo),
						tests[i].pred,
						-1,
						XB_MACHINE_PARSE_FLAG_OPTIMIZE,
						&error);
		g_assert_no_error(error);
		g_assert_nonnull(opcodes);
		str = xb_stack_to_string(opcodes);
		g_assert_nonnull(str);
		g_assert_cmpstr(str, ==, tests[i].str);
	}
	for (guint i = 0; invalid[i] != NULL; i++) {
		g_autoptr(GError) error = NULL;
		g_autoptr(XbStack) opcodes = NULL;
		opcodes = xb_machine_parse_full(xb_silo_get_machine(silo),
						invalid[i],
						-1,
						XB_MACHINE_PARSE_FLAG_OPTIMIZE,
						&error);
		g_assert_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
		g_assert_null(opcodes);
	}
}

void
xb_opcode_test_add_funcs(void)
{
	g_test_add_func("/libxmlb/opcodes", xb_predicate_func);
	g_test_add_func("/libxmlb/opcodes/optimize", xb_predicate_optimize_func);
	g_test_add_func("/libxmlb/opcodes/kind", xb_opcodes_kind_func);
}
