/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "xb-machine.h"
#include "xb-node.h"
#include "xb-query.h"
#include "xb-silo-node.h"
#include "xb-silo.h"
#include "xb-string-private.h"

G_BEGIN_DECLS

/* 32 bytes, native byte order */
typedef struct __attribute__((packed)) {
	guint32 magic;
	guint32 version;
	XbGuid guid;
	guint16 strtab_ntags;
	guint8 padding[2];
	guint32 strtab;
	guint64 filesz;
} XbSiloHeader;

#define XB_SILO_MAGIC_BYTES 0x624c4d58
#define XB_SILO_VERSION	    0x00000009

typedef struct {
	/*< private >*/
	XbSiloNode *sn;
	guint position;
} XbSiloQueryData;

const gchar *
xb_silo_from_strtab(XbSilo *self, guint32 offset, GError **error) G_GNUC_NON_NULL(1);
gboolean
xb_silo_strtab_index_insert(XbSilo *self, guint32 offset, GError **error) G_GNUC_NON_NULL(1);
guint32
xb_silo_strtab_index_lookup(XbSilo *self, const gchar *str) G_GNUC_NON_NULL(1);
XbSiloNode *
xb_silo_get_node(XbSilo *self, guint32 off, GError **error) G_GNUC_NON_NULL(1);
XbMachine *
xb_silo_get_machine(XbSilo *self) G_GNUC_NON_NULL(1);
guint32
xb_silo_get_strtab(XbSilo *self) G_GNUC_NON_NULL(1);
guint32
xb_silo_get_strtab_idx(XbSilo *self, const gchar *element) G_GNUC_NON_NULL(1);
guint32
xb_silo_get_offset_for_node(XbSilo *self, XbSiloNode *n) G_GNUC_NON_NULL(1, 2);
XbSiloNode *
xb_silo_get_root_node(XbSilo *self, GError **error) G_GNUC_NON_NULL(1);
XbSiloNode *
xb_silo_get_parent_node(XbSilo *self, XbSiloNode *n, GError **error) G_GNUC_NON_NULL(1, 2);
XbSiloNode *
xb_silo_get_next_node(XbSilo *self, XbSiloNode *n, GError **error) G_GNUC_NON_NULL(1, 2);
XbSiloNode *
xb_silo_get_child_node(XbSilo *self, XbSiloNode *n, GError **error) G_GNUC_NON_NULL(1, 2);
const gchar *
xb_silo_get_node_element(XbSilo *self, XbSiloNode *n, GError **error) G_GNUC_NON_NULL(1, 2);
XbSiloNodeAttr *
xb_silo_get_node_attr_by_str(XbSilo *self, XbSiloNode *n, const gchar *name)
    G_GNUC_NON_NULL(1, 2, 3);
guint
xb_silo_get_node_depth(XbSilo *self, XbSiloNode *n) G_GNUC_NON_NULL(1, 2);
XbNode *
xb_silo_create_node(XbSilo *self, XbSiloNode *sn, gboolean force_node_cache) G_GNUC_NON_NULL(1);
GTimer *
xb_silo_start_profile(XbSilo *self) G_GNUC_NON_NULL(1);
void
xb_silo_add_profile(XbSilo *self, GTimer *timer, const gchar *fmt, ...) G_GNUC_PRINTF(3, 4)
    G_GNUC_NON_NULL(1);
gboolean
xb_silo_is_empty(XbSilo *self) G_GNUC_NON_NULL(1);
void
xb_silo_uninvalidate(XbSilo *self) G_GNUC_NON_NULL(1);
XbSiloProfileFlags
xb_silo_get_profile_flags(XbSilo *self) G_GNUC_NON_NULL(1);

G_END_DECLS
