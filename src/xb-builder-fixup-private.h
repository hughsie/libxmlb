/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_BUILDER_FIXUP_PRIVATE_H
#define __XB_BUILDER_FIXUP_PRIVATE_H

#include <gio/gio.h>

#include "xb-builder-fixup.h"

G_BEGIN_DECLS

gboolean	 xb_builder_fixup_node		(XbBuilderFixup		*self,
						 XbBuilderNode		*bn,
						 GError			**error);
const gchar	*xb_builder_fixup_get_id	(XbBuilderFixup		*self);
gchar		*xb_builder_fixup_get_guid	(XbBuilderFixup		*self);

G_END_DECLS

#endif /* __XB_BUILDER_FIXUP_PRIVATE_H */
