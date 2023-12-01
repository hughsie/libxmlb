/*
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "xb-value-bindings.h"

#include "xb-silo.h"

gchar *
xb_value_bindings_to_string(XbValueBindings *self) G_GNUC_NON_NULL(1);
gboolean
xb_value_bindings_indexed_text_lookup(XbValueBindings *self, XbSilo *silo, GError **error)
    G_GNUC_NON_NULL(1, 2);
