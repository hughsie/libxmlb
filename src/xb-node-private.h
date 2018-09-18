/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_NODE_PRIVATE_H
#define __XB_NODE_PRIVATE_H

G_BEGIN_DECLS

#include <glib-object.h>

#include "xb-silo-private.h"
#include "xb-node.h"

XbNode		*xb_node_new				(XbSilo		*silo,
							 XbSiloNode	*sn);
XbSiloNode	*xb_node_get_sn				(XbNode		*self);
XbSilo		*xb_node_get_silo			(XbNode		*self);

G_END_DECLS

#endif /* __XB_NODE_PRIVATE_H */

