/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-node.h"
#include "xb-query.h"
#include "xb-silo.h"

G_BEGIN_DECLS

GPtrArray	*xb_silo_query			(XbSilo		*self,
						 const gchar	*xpath,
						 guint		 limit,
						 GError		**error);
GPtrArray	*xb_silo_query_full		(XbSilo		*self,
						 XbQuery	*query,
						 GError		**error);
XbNode		*xb_silo_query_first		(XbSilo		*self,
						 const gchar	*xpath,
						 GError		**error);
XbNode		*xb_silo_query_first_full	(XbSilo		*self,
						 XbQuery	*query,
						 GError		**error);
gboolean	 xb_silo_query_build_index	(XbSilo		*self,
						 const gchar	*xpath,
						 const gchar	*attr,
						 GError		**error);

G_END_DECLS
