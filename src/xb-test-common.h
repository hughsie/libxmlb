/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib-object.h>

#include "xb-builder.h"

G_BEGIN_DECLS

#define XB_TEST_INOTIFY_TIMEOUT 10000 /* ms */

void
xb_test_loop_run_with_timeout(guint timeout_ms);
void
xb_test_loop_quit(void);
gboolean
xb_test_import_xml(XbBuilder *self, const gchar *xml, GError **error) G_GNUC_NON_NULL(1, 2);
gboolean
xb_test_fixup_tokenize_cb(XbBuilderFixup *self,
			  XbBuilderNode *bn,
			  gpointer user_data,
			  GError **error);

/* each group of tests registers itself */
void
xb_common_test_add_funcs(void);
void
xb_opcode_test_add_funcs(void);
void
xb_node_test_add_funcs(void);
void
xb_builder_test_add_funcs(void);
void
xb_builder_source_test_add_funcs(void);
void
xb_builder_node_test_add_funcs(void);
void
xb_xpath_test_add_funcs(void);
void
xb_xpath_predicate_test_add_funcs(void);
void
xb_xpath_limit_test_add_funcs(void);
void
xb_threading_test_add_funcs(void);
void
xb_speed_test_add_funcs(void);

G_END_DECLS
