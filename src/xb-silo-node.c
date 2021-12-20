/*
 * Copyright (C) 2021 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN "XbSilo"

#include "xb-silo-node.h"

#include <string.h>

/* private */
guint32
xb_silo_node_get_size(XbSiloNode *self)
{
	if (xb_silo_node_has_flag(self, XB_SILO_NODE_FLAG_IS_ELEMENT)) {
		guint8 sz = sizeof(XbSiloNode);
		sz += self->attr_count * sizeof(XbSiloNodeAttr);
		sz += self->token_count * sizeof(guint32);
		return sz;
	}

	/* sentinel */
	return sizeof(guint8);
}

/* private */
guint8
xb_silo_node_get_flags(XbSiloNode *self)
{
	return self->flags;
}

/* private */
gboolean
xb_silo_node_has_flag(XbSiloNode *self, XbSiloNodeFlag flag)
{
	return (self->flags & flag) > 0;
}

/* private */
guint32
xb_silo_node_get_text_idx(XbSiloNode *self)
{
	return self->text;
}

/* private */
guint32
xb_silo_node_get_tail_idx(XbSiloNode *self)
{
	return self->tail;
}

/* private */
guint8
xb_silo_node_get_attr_count(XbSiloNode *self)
{
	return self->attr_count;
}

/* private */
XbSiloNodeAttr *
xb_silo_node_get_attr(XbSiloNode *self, guint8 idx)
{
	guint32 off = sizeof(XbSiloNode);
	off += sizeof(XbSiloNodeAttr) * idx;
	return (XbSiloNodeAttr *)(((guint8 *)self) + off);
}

/* private */
guint8
xb_silo_node_get_token_count(XbSiloNode *self)
{
	return self->token_count;
}

/* private */
guint32
xb_silo_node_get_token_idx(XbSiloNode *self, guint idx)
{
	guint32 off = 0;
	guint32 stridx;

	/* not valid */
	if (!xb_silo_node_has_flag(self, XB_SILO_NODE_FLAG_IS_ELEMENT))
		return XB_SILO_UNSET;
	if (!xb_silo_node_has_flag(self, XB_SILO_NODE_FLAG_IS_TOKENIZED))
		return XB_SILO_UNSET;

	/* calculate offset to token */
	off += sizeof(XbSiloNode);
	off += self->attr_count * sizeof(XbSiloNodeAttr);
	off += idx * sizeof(guint32);
	memcpy(&stridx, (guint8 *)self + off, sizeof(stridx));
	return stridx;
}
