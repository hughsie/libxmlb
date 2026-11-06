/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>

#include "xb-test-common.h"

static GMainLoop *_test_loop = NULL;
static guint _test_loop_timeout_id = 0;

static gboolean
xb_test_hang_check_cb(gpointer user_data)
{
	g_main_loop_quit(_test_loop);
	_test_loop_timeout_id = 0;
	return G_SOURCE_REMOVE;
}

void
xb_test_loop_run_with_timeout(guint timeout_ms)
{
	g_assert(_test_loop_timeout_id == 0);
	g_assert(_test_loop == NULL);
	_test_loop = g_main_loop_new(NULL, FALSE);
	_test_loop_timeout_id = g_timeout_add(timeout_ms, xb_test_hang_check_cb, NULL);
	g_main_loop_run(_test_loop);
}

void
xb_test_loop_quit(void)
{
	g_clear_handle_id(&_test_loop_timeout_id, g_source_remove);
	if (_test_loop != NULL) {
		g_main_loop_quit(_test_loop);
		g_main_loop_unref(_test_loop);
		_test_loop = NULL;
	}
}

gboolean
xb_test_import_xml(XbBuilder *self, const gchar *xml, GError **error)
{
	g_autoptr(XbBuilderSource) source = xb_builder_source_new();

	g_return_val_if_fail(XB_IS_BUILDER(self), FALSE);
	g_return_val_if_fail(xml != NULL, FALSE);

	/* add source */
	if (!xb_builder_source_load_xml(source, xml, XB_BUILDER_SOURCE_FLAG_NONE, error))
		return FALSE;

	/* success */
	xb_builder_import_source(self, source);
	return TRUE;
}

gboolean
xb_test_fixup_tokenize_cb(XbBuilderFixup *self,
			  XbBuilderNode *bn,
			  gpointer user_data,
			  GError **error)
{
	if (g_strcmp0(xb_builder_node_get_element(bn), "name") == 0)
		xb_builder_node_tokenize_text(bn);
	return TRUE;
}
