/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "xb-node.h"
#include "xb-silo-private.h"

G_BEGIN_DECLS

XbNode *
xb_node_new(XbSilo *silo, XbSiloNode *sn) G_GNUC_NON_NULL(1);
XbSiloNode *
xb_node_get_sn(XbNode *self) G_GNUC_NON_NULL(1);

G_END_DECLS
