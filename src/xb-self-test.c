/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>
#include <locale.h>

#include "xb-test-common.h"

int
main(int argc, char **argv)
{
	g_setenv("G_TEST_SRCDIR", SRCDIR, FALSE);

	g_test_init(&argc, &argv, NULL);

	/* only critical and error are fatal */
	g_log_set_fatal_mask(NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
	g_setenv("G_MESSAGES_DEBUG", "all", TRUE);

	setlocale(LC_ALL, "");

	/* tests go here */
	xb_common_test_add_funcs();
	xb_opcode_test_add_funcs();
	xb_node_test_add_funcs();
	xb_builder_test_add_funcs();
	xb_builder_source_test_add_funcs();
	xb_builder_node_test_add_funcs();
	xb_xpath_test_add_funcs();
	xb_xpath_predicate_test_add_funcs();
	xb_xpath_limit_test_add_funcs();
	xb_threading_test_add_funcs();
	xb_speed_test_add_funcs();
	return g_test_run();
}
