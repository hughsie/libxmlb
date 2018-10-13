/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <gio/gio.h>

#include "xb-node-private.h"
#include "xb-silo-export.h"
#include "xb-silo-private.h"
#include "xb-string-private.h"

typedef struct {
	GString			*xml;
	XbNodeExportFlags	 flags;
	guint32			 off;
	guint			 level;
} XbSiloExportHelper;

static gchar *
xb_silo_export_escape (const gchar *val)
{
	GString *str = g_string_new (val);
	xb_string_replace (str, "&", "&amp;");
	xb_string_replace (str, "<", "&lt;");
	xb_string_replace (str, ">", "&gt;");
	xb_string_replace (str, "\"", "&quot;");
	xb_string_replace (str, "'", "&apos;");
	return g_string_free (str, FALSE);
}

static gboolean
xb_silo_export_node (XbSilo *self, XbSiloExportHelper *helper, XbSiloNode *sn, GError **error)
{
	XbSiloNode *sn2;

	helper->off = xb_silo_get_offset_for_node (self, sn);

	/* add start of opening tag */
	if (helper->flags & XB_NODE_EXPORT_FLAG_FORMAT_INDENT) {
		for (guint i = 0; i < helper->level; i++)
			g_string_append (helper->xml, "  ");
	}
	g_string_append_printf (helper->xml, "<%s",
				xb_silo_from_strtab (self, sn->element_name));

	/* add any attributes */
	for (guint8 i = 0; i < sn->nr_attrs; i++) {
		XbSiloAttr *a = xb_silo_get_attr (self, helper->off, i);
		g_autofree gchar *key = xb_silo_export_escape (xb_silo_from_strtab (self, a->attr_name));
		g_autofree gchar *val = xb_silo_export_escape (xb_silo_from_strtab (self, a->attr_value));
		g_string_append_printf (helper->xml, " %s=\"%s\"", key, val);
	}

	/* finish the opening tag and add any text if it exists */
	if (sn->has_text) {
		g_autofree gchar *text = xb_silo_export_escape (xb_silo_from_strtab (self, sn->text));
		g_string_append (helper->xml, ">");
		g_string_append (helper->xml, text);
	} else {
		g_string_append (helper->xml, ">");
		if (helper->flags & XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE)
			g_string_append (helper->xml, "\n");
	}
	helper->off += xb_silo_node_get_size (sn);

	/* recurse deeper */
	while (xb_silo_get_node(self, helper->off)->is_node) {
		XbSiloNode *child = xb_silo_get_node (self, helper->off);
		helper->level++;
		if (!xb_silo_export_node (self, helper, child, error))
			return FALSE;
		helper->level--;
	}

	/* check for the single byte sentinel */
	sn2 = xb_silo_get_node (self, helper->off);
	if (sn2->is_node) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "no seninel at %" G_GUINT32_FORMAT,
			     helper->off);
		return FALSE;
	}
	helper->off += xb_silo_node_get_size (sn2);

	/* add closing tag */
	if ((helper->flags & XB_NODE_EXPORT_FLAG_FORMAT_INDENT) > 0 &&
	    !sn->has_text) {
		for (guint i = 0; i < helper->level; i++)
			g_string_append (helper->xml, "  ");
	}
	g_string_append_printf (helper->xml, "</%s>",
				xb_silo_from_strtab (self, sn->element_name));
	if (helper->flags & XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE)
		g_string_append (helper->xml, "\n");

	return TRUE;
}

/* private */
gchar *
xb_silo_export_with_root (XbSilo *self, XbNode *root, XbNodeExportFlags flags, GError **error)
{
	XbSiloNode *sn;
	XbSiloExportHelper helper = {
		.flags		= flags,
		.level		= 0,
		.off		= sizeof(XbSiloHeader),
	};

	g_return_val_if_fail (XB_IS_SILO (self), NULL);

	/* this implies the other */
	if (flags & XB_NODE_EXPORT_FLAG_ONLY_CHILDREN)
		flags |= XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS;

	/* optional subtree export */
	if (root != NULL) {
		sn = xb_node_get_sn (root);
		if (sn != NULL && flags & XB_NODE_EXPORT_FLAG_ONLY_CHILDREN)
			sn = xb_silo_node_get_child (self, sn);
	} else {
		sn = xb_silo_get_sroot (self);
	}

	/* no root */
	if (sn == NULL) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_FOUND,
				     "no data to export");
		return NULL;
	}

	/* root node */
	helper.xml = g_string_new (NULL);
	if ((flags & XB_NODE_EXPORT_FLAG_ADD_HEADER) > 0)
		g_string_append (helper.xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	do {
		if (!xb_silo_export_node (self, &helper, sn, error)) {
			g_string_free (helper.xml, TRUE);
			return NULL;
		}
		if ((flags & XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS) == 0)
			break;
		sn = xb_silo_node_get_next (self, sn);
	} while (sn != NULL);

	/* success */
	return g_string_free (helper.xml, FALSE);
}

/**
 * xb_silo_export:
 * @self: a #XbSilo
 * @flags: some #XbNodeExportFlags, e.g. #XB_NODE_EXPORT_FLAG_NONE
 * @error: the #GError, or %NULL
 *
 * Exports the silo back to XML.
 *
 * Returns: XML data, or %NULL for an error
 *
 * Since: 0.1.0
 **/
gchar *
xb_silo_export (XbSilo *self, XbNodeExportFlags flags, GError **error)
{
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	return xb_silo_export_with_root (self, NULL, flags, error);
}
