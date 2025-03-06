/*
 * Copyright 2021 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "xb-compile.h"

#define XB_SILO_UNSET 0xffffffff

typedef enum {
	XB_SILO_NODE_FLAG_NONE = 0,
	XB_SILO_NODE_FLAG_IS_ELEMENT = 1 << 0,
	XB_SILO_NODE_FLAG_IS_TOKENIZED = 1 << 1,
} XbSiloNodeFlag;

typedef struct __attribute__((packed)) {
	guint8 flags : 2;
	guint8 attr_count : 6;
	guint8 token_count;   /* ONLY when is_element */
	guint32 element_name; /* ONLY when is_element: from strtab */
	guint32 parent;	      /* ONLY when is_element: from 0 */
	guint32 next;	      /* ONLY when is_element: from 0 */
	guint32 text;	      /* ONLY when is_element: from strtab */
	guint32 tail;	      /* ONLY when is_element: from strtab */
			      /*
			      guint32		attrs[attr_count];
			      guint32		tokens[token_count];
			      */
} XbSiloNode;

typedef struct __attribute__((packed)) {
	guint32 attr_name;  /* from strtab */
	guint32 attr_value; /* from strtab */
} XbSiloNodeAttr;

gchar *
xb_silo_node_to_string(const XbSiloNode *self) G_GNUC_NON_NULL(1);

/* private */
static inline gboolean
xb_silo_node_has_flag(const XbSiloNode *self, XbSiloNodeFlag flag)
{
	return (self->flags & flag) > 0;
}

static inline guint32
xb_silo_node_get_size(const XbSiloNode *self)
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
static inline guint8
xb_silo_node_get_flags(const XbSiloNode *self)
{
	return self->flags;
}

/* private */
static inline guint32
xb_silo_node_get_text_idx(const XbSiloNode *self)
{
	return self->text;
}

/* private */
static inline guint32
xb_silo_node_get_tail_idx(const XbSiloNode *self)
{
	return self->tail;
}

/* private */
static inline guint8
xb_silo_node_get_attr_count(const XbSiloNode *self)
{
	return self->attr_count;
}

/* private */
static inline XbSiloNodeAttr *
xb_silo_node_get_attr(const XbSiloNode *self, guint8 idx)
{
	guint32 off = sizeof(XbSiloNode);
	off += sizeof(XbSiloNodeAttr) * idx;
	return (XbSiloNodeAttr *)(((guint8 *)self) + off);
}

/* private */
static inline guint8
xb_silo_node_get_token_count(const XbSiloNode *self)
{
	return self->token_count;
}

/* private */
static inline guint32
xb_silo_node_get_token_idx(const XbSiloNode *self, guint idx)
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
