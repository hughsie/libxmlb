/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-node.h"
#include "xb-silo-private.h"

G_BEGIN_DECLS

XbNode *
xb_node_new(XbSilo *silo, XbSiloNode *sn);
XbSiloNode *
xb_node_get_sn(XbNode *self);

G_END_DECLS
