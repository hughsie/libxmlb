/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-silo-query.h"
#include "xb-query.h"

G_BEGIN_DECLS

GPtrArray	*xb_silo_query_with_root	(XbSilo		*self,
						 XbNode		*n,
						 const gchar	*xpath,
						 guint		 limit,
						 GError		**error);
GPtrArray	*xb_silo_query_with_root_full	(XbSilo		*self,
						 XbNode		*n,
						 XbQuery	*query,
						 GError		**error);

G_END_DECLS
