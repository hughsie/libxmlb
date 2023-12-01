/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 * vi:set noexpandtab tabstop=8 shiftwidth=8:
 *
 * Copyright (C) 2020 Endless OS Foundation LLC
 *
 * Author: Philip Withnall <withnall@endlessm.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib.h>

#include "xb-opcode.h"

G_BEGIN_DECLS

/**
 * XbValueBindings:
 *
 * An opaque struct which contains values bound to a query.
 *
 * Since: 0.3.0
 */
typedef struct {
	/*< private >*/
	guint8 dummy0;
	guint32 dummy1;
	gpointer dummy2[2];
	guint8 dummy3;
	guint32 dummy4;
	gpointer dummy5[2];
	guint8 dummy6;
	guint32 dummy7;
	gpointer dummy8[2];
	guint8 dummy9;
	guint32 dummy10;
	gpointer dummy11[2];
	gpointer dummy12[3];
} XbValueBindings;

GType
xb_value_bindings_get_type(void);

/**
 * XB_VALUE_BINDINGS_INIT:
 *
 * Static initialiser for #XbValueBindings so it can be used on the stack.
 *
 * Use it in association with g_auto(), to ensure the bindings are freed once
 * finished with:
 * |[
 * g_auto(XbValueBindings) bindings = XB_VALUE_BINDINGS_INIT ();
 *
 * xb_value_bindings_bind_str (&bindings, 0, "test", NULL);
 * ]|
 *
 * Since: 0.3.0
 */
#define XB_VALUE_BINDINGS_INIT()                                                                   \
	{                                                                                          \
		0, 0, {NULL, NULL}, 0, 0, {NULL, NULL}, 0, 0, {NULL, NULL}, 0, 0, {NULL, NULL},    \
		{                                                                                  \
			NULL,                                                                      \
		}                                                                                  \
	}

void
xb_value_bindings_init(XbValueBindings *self) G_GNUC_NON_NULL(1);
void
xb_value_bindings_clear(XbValueBindings *self) G_GNUC_NON_NULL(1);

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC(XbValueBindings, xb_value_bindings_clear)

XbValueBindings *
xb_value_bindings_copy(XbValueBindings *self) G_GNUC_NON_NULL(1);
void
xb_value_bindings_free(XbValueBindings *self) G_GNUC_NON_NULL(1);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XbValueBindings, xb_value_bindings_free)

gboolean
xb_value_bindings_is_bound(XbValueBindings *self, guint idx) G_GNUC_NON_NULL(1);
void
xb_value_bindings_bind_str(XbValueBindings *self,
			   guint idx,
			   const gchar *str,
			   GDestroyNotify destroy_func) G_GNUC_NON_NULL(1);
void
xb_value_bindings_bind_val(XbValueBindings *self, guint idx, guint32 val) G_GNUC_NON_NULL(1);

gboolean
xb_value_bindings_lookup_opcode(XbValueBindings *self, guint idx, XbOpcode *opcode_out)
    G_GNUC_NON_NULL(1, 3);

gboolean
xb_value_bindings_copy_binding(XbValueBindings *self,
			       guint idx,
			       XbValueBindings *dest,
			       guint dest_idx) G_GNUC_NON_NULL(1, 3);

G_END_DECLS
