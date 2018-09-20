/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_NODE_H
#define __XB_NODE_H

G_BEGIN_DECLS

#include <glib-object.h>

#define XB_TYPE_NODE (xb_node_get_type ())
G_DECLARE_FINAL_TYPE (XbNode, xb_node, XB, NODE, GObject)

/**
 * XbNodeExportFlags:
 * @XB_NODE_EXPORT_FLAG_NONE:			No extra flags to use
 * @XB_NODE_EXPORT_FLAG_ADD_HEADER:		Add an XML header to the data
 * @XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE:	Split up children with a newline
 * @XB_NODE_EXPORT_FLAG_FORMAT_INDENT:		Indent the XML by child depth
 * @XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS:	Include the siblings when converting
 *
 * The flags for converting to XML.
 **/
typedef enum {
	XB_NODE_EXPORT_FLAG_NONE		= 0,
	XB_NODE_EXPORT_FLAG_ADD_HEADER		= 1 << 0,
	XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE	= 1 << 1,
	XB_NODE_EXPORT_FLAG_FORMAT_INDENT	= 1 << 2,
	XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS	= 1 << 3,
	/*< private >*/
	XB_NODE_EXPORT_FLAG_LAST
} XbNodeExportFlags;

GPtrArray	*xb_node_query			(XbNode		*self,
						 const gchar	*xpath,
						 guint		 limit,
						 GError		**error);
XbNode		*xb_node_query_first		(XbNode		*self,
						 const gchar	*xpath,
						 GError		**error);
const gchar	*xb_node_query_text		(XbNode		*self,
						 const gchar	*xpath,
						 GError		**error);
guint64		 xb_node_query_text_as_uint	(XbNode		*self,
						 const gchar	*xpath,
						 GError		**error);
const gchar	*xb_node_query_attr		(XbNode		*self,
						 const gchar	*xpath,
						 const gchar	*name,
						 GError		**error);
guint64		 xb_node_query_attr_as_uint	(XbNode		*self,
						 const gchar	*xpath,
						 const gchar	*name,
						 GError		**error);
gchar		*xb_node_query_export		(XbNode		*self,
						 const gchar	*xpath,
						 GError		**error);
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
const gchar	*xb_node_get_attr		(XbNode		*self,
						 const gchar	*name);
guint		 xb_node_get_depth		(XbNode		*self);


G_END_DECLS

#endif /* __XB_NODE_H */

