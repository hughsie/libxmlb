/*
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "xb-compile.h"

G_BEGIN_DECLS

#define XB_TYPE_BUILDER_SOURCE_CTX (xb_builder_source_ctx_get_type())
G_DECLARE_DERIVABLE_TYPE(XbBuilderSourceCtx, xb_builder_source_ctx, XB, BUILDER_SOURCE_CTX, GObject)

struct _XbBuilderSourceCtxClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_xb_reserved1)(void);
	void (*_xb_reserved2)(void);
	void (*_xb_reserved3)(void);
	void (*_xb_reserved4)(void);
	void (*_xb_reserved5)(void);
	void (*_xb_reserved6)(void);
	void (*_xb_reserved7)(void);
};

GInputStream *
xb_builder_source_ctx_get_stream(XbBuilderSourceCtx *self) G_GNUC_NON_NULL(1);
const gchar *
xb_builder_source_ctx_get_filename(XbBuilderSourceCtx *self) G_GNUC_NON_NULL(1);
GBytes *
xb_builder_source_ctx_get_bytes(XbBuilderSourceCtx *self, GCancellable *cancellable, GError **error)
    G_GNUC_NON_NULL(1);

G_END_DECLS
