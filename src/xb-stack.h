/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-opcode.h"

G_BEGIN_DECLS

typedef struct _XbStack XbStack;

GType
xb_stack_get_type(void);
gchar *
xb_stack_to_string(XbStack *self) G_GNUC_NON_NULL(1);
gboolean
xb_stack_pop(XbStack *self, XbOpcode *opcode_out, GError **error) G_GNUC_NON_NULL(1);
gboolean
xb_stack_push(XbStack *self, XbOpcode **opcode_out, GError **error) G_GNUC_NON_NULL(1, 2);

G_END_DECLS
