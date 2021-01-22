/*
 * Copyright (C) 2021 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include <string.h>

#include "xb-silo-node.h"

/* private */
guint32
xb_silo_node_get_size (XbSiloNode *self)
{
	/* sentinel */
	if ((self->flags & XB_SILO_NODE_FLAG_IS_ELEMENT) == 0)
		return sizeof(guint8);

	/* element */
	return sizeof(XbSiloNode) + sizeof(XbSiloNodeAttr) * self->attr_cnt;
}

/* private */
guint8
xb_silo_node_get_flags (XbSiloNode *self)
{
	return self->flags;
}

/* private */
gboolean
xb_silo_node_has_flag (XbSiloNode *self, XbSiloNodeFlag flag)
{
	return (self->flags & flag) > 0;
}

/* private */
guint32
xb_silo_node_get_text_idx (XbSiloNode *self)
{
	return self->text;
}

/* private */
guint32
xb_silo_node_get_tail_idx (XbSiloNode *self)
{
	return self->tail;
}

/* private */
guint8
xb_silo_node_get_attr_count (XbSiloNode *self)
{
	return self->attr_cnt;
}

/* private */
XbSiloNodeAttr *
xb_silo_node_get_attr (XbSiloNode *self, guint8 idx)
{
	guint32 off = sizeof(XbSiloNode);
	off += sizeof(XbSiloNodeAttr) * idx;
	return (XbSiloNodeAttr *) (((guint8 *) self) + off);
}
