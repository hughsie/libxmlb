/*
 * Copyright (C) 2021 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#define XB_SILO_UNSET			0xffffffff

typedef enum {
	XB_SILO_NODE_FLAG_NONE		=	0,
	XB_SILO_NODE_FLAG_IS_ELEMENT	=	1 << 0,
} XbSiloNodeFlag;

typedef struct __attribute__ ((packed)) {
	guint8		flags:1;
	guint8		attr_count:7;
	guint32		element_name;	/* ONLY when is_node: from strtab */
	guint32		parent;		/* ONLY when is_node: from 0 */
	guint32		next;		/* ONLY when is_node: from 0 */
	guint32		text;		/* ONLY when is_node: from strtab */
	guint32		tail;		/* ONLY when is_node: from strtab */
} XbSiloNode;

typedef struct __attribute__ ((packed)) {
	guint32		attr_name;	/* from strtab */
	guint32		attr_value;	/* from strtab */
} XbSiloNodeAttr;

guint32		 xb_silo_node_get_size		(XbSiloNode	*self);
guint32		 xb_silo_node_get_text_idx	(XbSiloNode	*self);
guint32		 xb_silo_node_get_tail_idx	(XbSiloNode	*self);
guint8		 xb_silo_node_get_flags		(XbSiloNode	*self);
gboolean	 xb_silo_node_has_flag		(XbSiloNode	*self,
						 XbSiloNodeFlag	 flags);
guint8		 xb_silo_node_get_attr_count	(XbSiloNode	*self);
XbSiloNodeAttr	*xb_silo_node_get_attr		(XbSiloNode	*self,
						 guint8		 idx);
