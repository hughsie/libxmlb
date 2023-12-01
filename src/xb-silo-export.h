/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-node.h"
#include "xb-silo.h"

G_BEGIN_DECLS

gchar *
xb_silo_export(XbSilo *self, XbNodeExportFlags flags, GError **error) G_GNUC_NON_NULL(1);
gboolean
xb_silo_export_file(XbSilo *self,
		    GFile *file,
		    XbNodeExportFlags flags,
		    GCancellable *cancellable,
		    GError **error) G_GNUC_NON_NULL(1, 2);

G_END_DECLS
