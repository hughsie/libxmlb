/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-node.h"

G_BEGIN_DECLS

#define XB_TYPE_BUILDER_NODE (xb_builder_node_get_type ())
G_DECLARE_DERIVABLE_TYPE (XbBuilderNode, xb_builder_node, XB, BUILDER_NODE, GObject)

struct _XbBuilderNodeClass {
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
 * XbBuilderNodeFlags:
 * @XB_BUILDER_NODE_FLAG_NONE:			No extra flags to use
 * @XB_BUILDER_NODE_FLAG_IGNORE:		Do not include this node in the silo
 * @XB_BUILDER_NODE_FLAG_LITERAL_TEXT:		Assume the node CDATA is already valid
 * @XB_BUILDER_NODE_FLAG_HAS_TEXT:		If the node has leading text
 * @XB_BUILDER_NODE_FLAG_HAS_TAIL:		If the node has trailing text
 * @XB_BUILDER_NODE_FLAG_TOKENIZE_TEXT:		Tokenize and fold text to ASCII
 *
 * The flags used when building a node.
 **/
typedef enum {
	XB_BUILDER_NODE_FLAG_NONE		= 0,		/* Since: 0.1.0 */
	XB_BUILDER_NODE_FLAG_IGNORE		= 1 << 0,	/* Since: 0.1.0 */
	XB_BUILDER_NODE_FLAG_LITERAL_TEXT	= 1 << 1,	/* Since: 0.1.0 */
	XB_BUILDER_NODE_FLAG_HAS_TEXT		= 1 << 2,	/* Since: 0.1.12 */
	XB_BUILDER_NODE_FLAG_HAS_TAIL		= 1 << 3,	/* Since: 0.1.12 */
	XB_BUILDER_NODE_FLAG_TOKENIZE_TEXT	= 1 << 4,	/* Since: 0.3.1 */
	/*< private >*/
	XB_BUILDER_NODE_FLAG_LAST
} XbBuilderNodeFlags;

typedef gboolean (*XbBuilderNodeTraverseFunc)	(XbBuilderNode		*bn,
						 gpointer		 user_data);
typedef gint	 (*XbBuilderNodeSortFunc)	(XbBuilderNode		*bn1,
						 XbBuilderNode		*bn2,
						 gpointer		 user_data);

XbBuilderNode	*xb_builder_node_new		(const gchar		*element);
XbBuilderNode	*xb_builder_node_insert		(XbBuilderNode		*parent,
						 const gchar		*element,
						 ...) G_GNUC_NULL_TERMINATED;
void		 xb_builder_node_insert_text	(XbBuilderNode		*parent,
						 const gchar		*element,
						 const gchar		*text,
						 ...) G_GNUC_NULL_TERMINATED;

gboolean	 xb_builder_node_has_flag	(XbBuilderNode		*self,
						 XbBuilderNodeFlags	 flag);
void		 xb_builder_node_add_flag	(XbBuilderNode		*self,
						 XbBuilderNodeFlags	 flag);
const gchar	*xb_builder_node_get_element	(XbBuilderNode		*self);
void		 xb_builder_node_set_element	(XbBuilderNode		*self,
						 const gchar		*element);
const gchar	*xb_builder_node_get_text	(XbBuilderNode		*self);
guint64		 xb_builder_node_get_text_as_uint(XbBuilderNode		*self);
void		 xb_builder_node_set_text	(XbBuilderNode		*self,
						 const gchar		*text,
						 gssize			 text_len);
void		 xb_builder_node_tokenize_text	(XbBuilderNode		*self);
const gchar	*xb_builder_node_get_tail	(XbBuilderNode		*self);
void		 xb_builder_node_set_tail	(XbBuilderNode		*self,
						 const gchar		*tail,
						 gssize			 tail_len);
const gchar	*xb_builder_node_get_attr	(XbBuilderNode		*self,
						 const gchar		*name);
guint64		 xb_builder_node_get_attr_as_uint(XbBuilderNode		*self,
						 const gchar		*name);
void		 xb_builder_node_set_attr	(XbBuilderNode		*self,
						 const gchar		*name,
						 const gchar		*value);
void		 xb_builder_node_remove_attr	(XbBuilderNode		*self,
						 const gchar		*name);
void		 xb_builder_node_add_child	(XbBuilderNode		*self,
						 XbBuilderNode		*child);
void		 xb_builder_node_remove_child	(XbBuilderNode		*self,
						 XbBuilderNode		*child);
GPtrArray	*xb_builder_node_get_children	(XbBuilderNode		*self);
XbBuilderNode	*xb_builder_node_get_first_child(XbBuilderNode		*self);
XbBuilderNode	*xb_builder_node_get_last_child	(XbBuilderNode		*self);
XbBuilderNode	*xb_builder_node_get_child	(XbBuilderNode		*self,
						 const gchar		*element,
						 const gchar		*text);
void		 xb_builder_node_unlink		(XbBuilderNode		*self);
XbBuilderNode	*xb_builder_node_get_parent	(XbBuilderNode		*self);
guint		 xb_builder_node_depth		(XbBuilderNode		*self);
void		 xb_builder_node_traverse	(XbBuilderNode		*self,
						 GTraverseType		 order,
						 GTraverseFlags		 flags,
						 gint			 max_depth,
						 XbBuilderNodeTraverseFunc func,
						 gpointer		 user_data);
void		 xb_builder_node_sort_children	(XbBuilderNode		*self,
						 XbBuilderNodeSortFunc	 func,
						 gpointer		 user_data);
gchar		*xb_builder_node_export		(XbBuilderNode		*self,
						 XbNodeExportFlags	 flags,
						 GError			**error);
GPtrArray	*xb_builder_node_get_tokens	(XbBuilderNode		*self);
void		 xb_builder_node_add_token	(XbBuilderNode		*self,
						 const gchar		*token);

G_END_DECLS
