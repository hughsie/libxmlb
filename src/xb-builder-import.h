/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_BUILDER_IMPORT_H
#define __XB_BUILDER_IMPORT_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define XB_TYPE_BUILDER_IMPORT (xb_builder_import_get_type ())

G_DECLARE_FINAL_TYPE (XbBuilderImport, xb_builder_import, XB, BUILDER_IMPORT, GObject)

XbBuilderImport	*xb_builder_import_new_file	(GFile			*file,
						 GCancellable		*cancellable,
						 GError			**error);
XbBuilderImport	*xb_builder_import_new_xml	(const gchar		*xml,
						 GError			**error);
const gchar	*xb_builder_import_get_guid	(XbBuilderImport	*self);
GInputStream	*xb_builder_import_get_istream	(XbBuilderImport	*self);

G_END_DECLS

#endif /* __XB_BUILDER_IMPORT_H */
