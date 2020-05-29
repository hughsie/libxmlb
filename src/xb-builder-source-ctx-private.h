/*
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-builder-source-ctx.h"

G_BEGIN_DECLS

XbBuilderSourceCtx *xb_builder_source_ctx_new		(GFile			*file,
							 GInputStream		*istream);
void		 xb_builder_source_ctx_set_filename	(XbBuilderSourceCtx	*self,
							 const gchar		*filename);
gchar		*xb_builder_source_ctx_get_content_type	(XbBuilderSourceCtx	*self,
							 GCancellable		*cancellable,
							 GError			**error);

G_END_DECLS
