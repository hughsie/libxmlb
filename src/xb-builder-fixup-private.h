/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <gio/gio.h>

#include "xb-builder-fixup.h"

G_BEGIN_DECLS

gboolean	 xb_builder_fixup_node		(XbBuilderFixup		*self,
						 XbBuilderNode		*bn,
						 GError			**error);
const gchar	*xb_builder_fixup_get_id	(XbBuilderFixup		*self);
gchar		*xb_builder_fixup_get_guid	(XbBuilderFixup		*self);

G_END_DECLS
