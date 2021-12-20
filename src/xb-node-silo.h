/*
 * Copyright (C) 2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-node.h"
#include "xb-silo.h"

G_BEGIN_DECLS

XbSilo *
xb_node_get_silo(XbNode *self);

G_END_DECLS
