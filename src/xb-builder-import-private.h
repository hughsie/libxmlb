/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_BUILDER_IMPORT_PRIVATE_H
#define __XB_BUILDER_IMPORT_PRIVATE_H

#include <gio/gio.h>

#include "xb-builder-import.h"
#include "xb-builder-node.h"

G_BEGIN_DECLS

XbBuilderNode	*xb_builder_import_get_info	(XbBuilderImport	*self);
const gchar	*xb_builder_import_get_guid	(XbBuilderImport	*self);
const gchar	*xb_builder_import_get_prefix	(XbBuilderImport	*self);
GInputStream	*xb_builder_import_get_istream	(XbBuilderImport	*self);
gboolean	 xb_builder_import_node_func_run(XbBuilderImport	*self,
						 XbBuilderNode		*bn,
						 GError			**error);

G_END_DECLS

#endif /* __XB_BUILDER_IMPORT_PRIVATE_H */
