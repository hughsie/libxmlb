/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "xb-silo-export.h"

G_BEGIN_DECLS

GString		*xb_silo_export_with_root	(XbSilo		*self,
						 XbNode		*root,
						 XbNodeExportFlags flags,
						 GError		**error);

G_END_DECLS
