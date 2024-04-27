/*
 * Copyright 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "xb-builder-source-ctx.h"

G_BEGIN_DECLS

XbBuilderSourceCtx *
xb_builder_source_ctx_new(GFile *file, GInputStream *istream) G_GNUC_NON_NULL(2);
void
xb_builder_source_ctx_set_filename(XbBuilderSourceCtx *self, const gchar *basename)
    G_GNUC_NON_NULL(1);
gchar *
xb_builder_source_ctx_get_content_type(XbBuilderSourceCtx *self,
				       GCancellable *cancellable,
				       GError **error) G_GNUC_NON_NULL(1);

G_END_DECLS
