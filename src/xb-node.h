/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define XB_TYPE_NODE (xb_node_get_type ())
G_DECLARE_DERIVABLE_TYPE (XbNode, xb_node, XB, NODE, GObject)

struct _XbNodeClass {
	GObjectClass			 parent_class;
	/*< private >*/
	void (*_xb_reserved1)		(void);
	void (*_xb_reserved2)		(void);
	void (*_xb_reserved3)		(void);
	void (*_xb_reserved4)		(void);
	void (*_xb_reserved5)		(void);
	void (*_xb_reserved6)		(void);
	void (*_xb_reserved7)		(void);
};

/**
 * XbNodeExportFlags:
 * @XB_NODE_EXPORT_FLAG_NONE:			No extra flags to use
 * @XB_NODE_EXPORT_FLAG_ADD_HEADER:		Add an XML header to the data
 * @XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE:	Split up children with a newline
 * @XB_NODE_EXPORT_FLAG_FORMAT_INDENT:		Indent the XML by child depth
 * @XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS:	Include the siblings when converting
 * @XB_NODE_EXPORT_FLAG_ONLY_CHILDREN:		Only export the children of the node
 *
 * The flags for converting to XML.
 **/
typedef enum {
	XB_NODE_EXPORT_FLAG_NONE		= 0,		/* Since: 0.1.0 */
	XB_NODE_EXPORT_FLAG_ADD_HEADER		= 1 << 0,	/* Since: 0.1.0 */
	XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE	= 1 << 1,	/* Since: 0.1.0 */
	XB_NODE_EXPORT_FLAG_FORMAT_INDENT	= 1 << 2,	/* Since: 0.1.0 */
	XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS	= 1 << 3,	/* Since: 0.1.0 */
	XB_NODE_EXPORT_FLAG_ONLY_CHILDREN	= 1 << 4,	/* Since: 0.1.0 */
	/*< private >*/
	XB_NODE_EXPORT_FLAG_LAST
} XbNodeExportFlags;

gchar		*xb_node_export			(XbNode		*self,
						 XbNodeExportFlags flags,
						 GError		**error);
GBytes		*xb_node_get_data		(XbNode		*self,
						 const gchar	*key);
void		 xb_node_set_data		(XbNode		*self,
						 const gchar	*key,
						 GBytes		*data);

XbNode		*xb_node_get_root		(XbNode		*self);
XbNode		*xb_node_get_parent		(XbNode		*self);
XbNode		*xb_node_get_next		(XbNode		*self);
XbNode		*xb_node_get_child		(XbNode		*self);
GPtrArray	*xb_node_get_children		(XbNode		*self);
const gchar	*xb_node_get_element		(XbNode		*self);
const gchar	*xb_node_get_text		(XbNode		*self);
guint64		 xb_node_get_text_as_uint	(XbNode		*self);
const gchar	*xb_node_get_attr		(XbNode		*self,
						 const gchar	*name);
guint64		 xb_node_get_attr_as_uint	(XbNode		*self,
						 const gchar	*name);
guint		 xb_node_get_depth		(XbNode		*self);


G_END_DECLS
