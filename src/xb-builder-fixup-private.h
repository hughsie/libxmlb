/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "xb-builder-fixup.h"

G_BEGIN_DECLS

gboolean
xb_builder_fixup_node(XbBuilderFixup *self, XbBuilderNode *bn, GError **error)
    G_GNUC_NON_NULL(1, 2);
const gchar *
xb_builder_fixup_get_id(XbBuilderFixup *self) G_GNUC_NON_NULL(1);
gchar *
xb_builder_fixup_get_guid(XbBuilderFixup *self) G_GNUC_NON_NULL(1);

G_END_DECLS
