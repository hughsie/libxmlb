/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_SILO_EXPORT_PRIVATE_H
#define __XB_SILO_EXPORT_PRIVATE_H

#include "xb-silo-export.h"

G_BEGIN_DECLS

GString		*xb_silo_export_with_root	(XbSilo		*self,
						 XbNode		*root,
						 XbNodeExportFlags flags,
						 GError		**error);

G_END_DECLS

#endif /* __XB_SILO_EXPORT_PRIVATE_H */
