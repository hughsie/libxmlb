/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "xb-builder-node.h"
#include "xb-builder-source.h"

G_BEGIN_DECLS

XbBuilderNode *
xb_builder_source_get_info(XbBuilderSource *self) G_GNUC_NON_NULL(1);
gchar *
xb_builder_source_get_guid(XbBuilderSource *self) G_GNUC_NON_NULL(1);
const gchar *
xb_builder_source_get_prefix(XbBuilderSource *self) G_GNUC_NON_NULL(1);
GInputStream *
xb_builder_source_get_istream(XbBuilderSource *self, GCancellable *cancellable, GError **error)
    G_GNUC_NON_NULL(1);
GFile *
xb_builder_source_get_file(XbBuilderSource *self) G_GNUC_NON_NULL(1);
gboolean
xb_builder_source_fixup(XbBuilderSource *self, XbBuilderNode *bn, GError **error)
    G_GNUC_NON_NULL(1, 2);
XbBuilderSourceFlags
xb_builder_source_get_flags(XbBuilderSource *self) G_GNUC_NON_NULL(1);

G_END_DECLS
