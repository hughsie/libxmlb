/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "xb-compile.h"

G_BEGIN_DECLS

#define XB_TYPE_NODE (xb_node_get_type())
G_DECLARE_DERIVABLE_TYPE(XbNode, xb_node, XB, NODE, GObject)

struct _XbNodeClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_xb_reserved1)(void);
	void (*_xb_reserved2)(void);
	void (*_xb_reserved3)(void);
	void (*_xb_reserved4)(void);
	void (*_xb_reserved5)(void);
	void (*_xb_reserved6)(void);
	void (*_xb_reserved7)(void);
};

/**
 * XbNodeExportFlags:
 * @XB_NODE_EXPORT_FLAG_NONE:			No extra flags to use
 * @XB_NODE_EXPORT_FLAG_ADD_HEADER:		Add an XML header to the data
 * @XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE:	Split up children with a newline
 * @XB_NODE_EXPORT_FLAG_FORMAT_INDENT:		Indent the XML by child depth
 * @XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS:	Include the siblings when converting
 * @XB_NODE_EXPORT_FLAG_ONLY_CHILDREN:		Only export the children of the node
 * @XB_NODE_EXPORT_FLAG_COLLAPSE_EMPTY:		If node has no children, collapse open and close
 *tags
 *
 * The flags for converting to XML.
 **/
typedef enum {
	XB_NODE_EXPORT_FLAG_NONE = 0,		       /* Since: 0.1.0 */
	XB_NODE_EXPORT_FLAG_ADD_HEADER = 1 << 0,       /* Since: 0.1.0 */
	XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE = 1 << 1, /* Since: 0.1.0 */
	XB_NODE_EXPORT_FLAG_FORMAT_INDENT = 1 << 2,    /* Since: 0.1.0 */
	XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS = 1 << 3, /* Since: 0.1.0 */
	XB_NODE_EXPORT_FLAG_ONLY_CHILDREN = 1 << 4,    /* Since: 0.1.0 */
	XB_NODE_EXPORT_FLAG_COLLAPSE_EMPTY = 1 << 5,   /* Since: 0.2.2 */
	/*< private >*/
	XB_NODE_EXPORT_FLAG_LAST
} XbNodeExportFlags;

typedef struct {
	/*< private >*/
	gpointer dummy1;
	guint8 dummy2;
	gpointer dummy3;
	gpointer dummy4;
	gpointer dummy5;
	gpointer dummy6;
} XbNodeAttrIter;

typedef struct {
	/*< private >*/
	gpointer dummy1;
	gpointer dummy2;
	gboolean dummy3;
	gpointer dummy4;
	gpointer dummy5;
	gpointer dummy6;
} XbNodeChildIter;

typedef gboolean (*XbNodeTransmogrifyFunc)(XbNode *self, gpointer user_data);
gboolean
xb_node_transmogrify(XbNode *self,
		     XbNodeTransmogrifyFunc func_text,
		     XbNodeTransmogrifyFunc func_tail,
		     gpointer user_data) G_GNUC_NON_NULL(1);

gchar *
xb_node_export(XbNode *self, XbNodeExportFlags flags, GError **error) G_GNUC_NON_NULL(1);
GBytes *
xb_node_get_data(XbNode *self, const gchar *key) G_GNUC_NON_NULL(1, 2);
void
xb_node_set_data(XbNode *self, const gchar *key, GBytes *data) G_GNUC_NON_NULL(1, 2);

XbNode *
xb_node_get_root(XbNode *self) G_GNUC_NON_NULL(1);
XbNode *
xb_node_get_parent(XbNode *self) G_GNUC_NON_NULL(1);
XbNode *
xb_node_get_next(XbNode *self) G_GNUC_NON_NULL(1);
XbNode *
xb_node_get_child(XbNode *self) G_GNUC_NON_NULL(1);
GPtrArray *
xb_node_get_children(XbNode *self) G_GNUC_NON_NULL(1);
const gchar *
xb_node_get_element(XbNode *self) G_GNUC_NON_NULL(1);
const gchar *
xb_node_get_text(XbNode *self) G_GNUC_NON_NULL(1);
guint64
xb_node_get_text_as_uint(XbNode *self) G_GNUC_NON_NULL(1);
const gchar *
xb_node_get_tail(XbNode *self) G_GNUC_NON_NULL(1);
const gchar *
xb_node_get_attr(XbNode *self, const gchar *name) G_GNUC_NON_NULL(1, 2);
guint64
xb_node_get_attr_as_uint(XbNode *self, const gchar *name) G_GNUC_NON_NULL(1, 2);
guint
xb_node_get_depth(XbNode *self) G_GNUC_NON_NULL(1);

void
xb_node_attr_iter_init(XbNodeAttrIter *iter, XbNode *self) G_GNUC_NON_NULL(1, 2);
gboolean
xb_node_attr_iter_next(XbNodeAttrIter *iter, const gchar **name, const gchar **value)
    G_GNUC_NON_NULL(1);

void
xb_node_child_iter_init(XbNodeChildIter *iter, XbNode *self) G_GNUC_NON_NULL(1, 2);
gboolean
xb_node_child_iter_next(XbNodeChildIter *iter, XbNode **child) G_GNUC_NON_NULL(1, 2);
gboolean
xb_node_child_iter_loop(XbNodeChildIter *iter, XbNode **child) G_GNUC_NON_NULL(1, 2);

G_END_DECLS
