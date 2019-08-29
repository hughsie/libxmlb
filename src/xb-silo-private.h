/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <uuid.h>

#include "xb-machine.h"
#include "xb-node.h"
#include "xb-silo.h"

G_BEGIN_DECLS

/* for old versions of libuuid */
#ifndef UUID_STR_LEN
#define UUID_STR_LEN	37
#endif

/* 32 bytes, native byte order */
typedef struct __attribute__ ((packed)) {
	guint32		magic;
	guint32		version;
	uuid_t		guid;
	guint16		strtab_ntags;
	guint8		padding[2];
	guint32		strtab;
} XbSiloHeader;

#define XB_SILO_MAGIC_BYTES		0x624c4d58
#define XB_SILO_VERSION			0x00000007
#define XB_SILO_UNSET			0xffffffff

typedef struct __attribute__ ((packed)) {
	guint8		is_node:1;
	guint8		nr_attrs:7;
	guint32		element_name;	/* ONLY when is_node: from strtab */
	guint32		parent;		/* ONLY when is_node: from 0 */
	guint32		next;		/* ONLY when is_node: from 0 */
	guint32		text;		/* ONLY when is_node: from strtab */
	guint32		tail;		/* ONLY when is_node: from strtab */
} XbSiloNode;

typedef struct __attribute__ ((packed)) {
	guint32		attr_name;	/* from strtab */
	guint32		attr_value;	/* from strtab */
} XbSiloAttr;

typedef struct {
	/*< private >*/
	XbSiloNode	*sn;
	guint		 position;
} XbSiloQueryData;

const gchar	*xb_silo_from_strtab		(XbSilo		*self,
						 guint32	 offset);
void		 xb_silo_strtab_index_insert	(XbSilo		*self,
						 guint32	 offset);
guint32		 xb_silo_strtab_index_lookup	(XbSilo		*self,
						 const gchar	*str);
XbSiloNode	*xb_silo_get_node		(XbSilo		*self,
						 guint32	 off);
XbSiloAttr	*xb_silo_get_attr		(XbSilo		*self,
						 guint32	 off,
						 guint8		 idx);
XbMachine	*xb_silo_get_machine		(XbSilo		*self);
guint32		 xb_silo_get_strtab		(XbSilo		*self);
guint32		 xb_silo_get_strtab_idx		(XbSilo		*self,
						 const gchar	*element);
guint32		 xb_silo_get_offset_for_node	(XbSilo		*self,
						 XbSiloNode	*n);
guint8		 xb_silo_node_get_size		(XbSiloNode	*n);
XbSiloNode	*xb_silo_get_sroot		(XbSilo		*self);
XbSiloNode	*xb_silo_node_get_parent	(XbSilo		*self,
						 XbSiloNode	*n);
XbSiloNode	*xb_silo_node_get_next		(XbSilo		*self,
						 XbSiloNode	*n);
XbSiloNode	*xb_silo_node_get_child		(XbSilo		*self,
						 XbSiloNode	*n);
const gchar	*xb_silo_node_get_element	(XbSilo		*self,
						 XbSiloNode	*n);
const gchar	*xb_silo_node_get_text		(XbSilo		*self,
						 XbSiloNode	*n);
const gchar	*xb_silo_node_get_tail		(XbSilo		*self,
						 XbSiloNode	*n);
XbSiloAttr	*xb_silo_node_get_attr_by_str	(XbSilo		*self,
						 XbSiloNode	*n,
						 const gchar	*name);
guint		 xb_silo_node_get_depth		(XbSilo		*self,
						 XbSiloNode	*n);
XbNode		*xb_silo_node_create		(XbSilo		*self,
						 XbSiloNode	*sn);
void		 xb_silo_add_profile		(XbSilo		*self,
						 GTimer		*timer,
						 const gchar	*fmt,
						 ...) G_GNUC_PRINTF (3, 4);
gboolean	 xb_silo_is_empty		(XbSilo		*self);
void		 xb_silo_uninvalidate		(XbSilo		*self);
XbSiloProfileFlags xb_silo_get_profile_flags	(XbSilo		*self);

G_END_DECLS
