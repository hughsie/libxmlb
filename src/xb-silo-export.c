/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <gio/gio.h>

#include "xb-node-private.h"
#include "xb-silo-export-private.h"
#include "xb-silo-node.h"
#include "xb-string-private.h"

typedef struct {
	GString			*xml;
	XbNodeExportFlags	 flags;
	guint32			 off;
	guint			 level;
} XbSiloExportHelper;

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
	for (guint8 i = 0; i < xb_silo_node_get_attr_count (sn); i++) {
		XbSiloNodeAttr *a = xb_silo_node_get_attr (sn, i);
		g_autofree gchar *key = xb_string_xml_escape (xb_silo_from_strtab (self, a->attr_name));
		g_autofree gchar *val = xb_string_xml_escape (xb_silo_from_strtab (self, a->attr_value));
		g_string_append_printf (helper->xml, " %s=\"%s\"", key, val);
	}

	/* collapse open/close tags together if no text or children */
	if (helper->flags & XB_NODE_EXPORT_FLAG_COLLAPSE_EMPTY &&
	    xb_silo_node_get_text_idx (sn) == XB_SILO_UNSET &&
	    xb_silo_get_child_node (self, sn) == NULL) {
		g_string_append (helper->xml, " />");
	} else {
		/* finish the opening tag and add any text if it exists */
		if (xb_silo_node_get_text_idx (sn) != XB_SILO_UNSET) {
			g_autofree gchar *text = xb_string_xml_escape (xb_silo_get_node_text (self, sn));
			g_string_append (helper->xml, ">");
			g_string_append (helper->xml, text);
		} else {
			g_string_append (helper->xml, ">");
			if (helper->flags & XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE)
				g_string_append (helper->xml, "\n");
		}
		helper->off += xb_silo_node_get_size (sn);

		/* recurse deeper */
		while (xb_silo_node_has_flag (xb_silo_get_node (self, helper->off),
					      XB_SILO_NODE_FLAG_IS_ELEMENT)) {
			XbSiloNode *child = xb_silo_get_node (self, helper->off);
			helper->level++;
			if (!xb_silo_export_node (self, helper, child, error))
				return FALSE;
			helper->level--;
		}

		/* check for the single byte sentinel */
		sn2 = xb_silo_get_node (self, helper->off);
		if (xb_silo_node_has_flag (sn2, XB_SILO_NODE_FLAG_IS_ELEMENT)) {
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
		    xb_silo_node_get_text_idx (sn) == XB_SILO_UNSET) {
			for (guint i = 0; i < helper->level; i++)
				g_string_append (helper->xml, "  ");
		}
		g_string_append_printf (helper->xml, "</%s>",
					xb_silo_from_strtab (self, sn->element_name));
	}

	/* add any optional tail */
	if (xb_silo_node_get_tail_idx (sn) != XB_SILO_UNSET) {
		g_autofree gchar *tail = xb_string_xml_escape (xb_silo_get_node_tail (self, sn));
		g_string_append (helper->xml, tail);
	}

	if (helper->flags & XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE)
		g_string_append (helper->xml, "\n");

	return TRUE;
}

/* private */
GString *
xb_silo_export_with_root (XbSilo *self, XbSiloNode *sroot, XbNodeExportFlags flags, GError **error)
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
	if (sroot != NULL) {
		sn = sroot;
		if (sn != NULL && flags & XB_NODE_EXPORT_FLAG_ONLY_CHILDREN)
			sn = xb_silo_get_child_node (self, sn);
	} else {
		sn = xb_silo_get_root_node (self);
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
		sn = xb_silo_get_next_node (self, sn);
	} while (sn != NULL);

	/* success */
	return helper.xml;
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
	GString *xml;
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	xml = xb_silo_export_with_root (self, NULL, flags, error);
	if (xml == NULL)
		return NULL;
	return g_string_free (xml, FALSE);
}

/**
 * xb_silo_export_file:
 * @self: a #XbSilo
 * @file: a #GFile
 * @flags: some #XbNodeExportFlags, e.g. #XB_NODE_EXPORT_FLAG_NONE
 * @cancellable: a #GCancellable, or %NULL
 * @error: the #GError, or %NULL
 *
 * Exports the silo back to an XML file.
 *
 * Returns: %TRUE on success
 *
 * Since: 0.1.2
 **/
gboolean
xb_silo_export_file (XbSilo *self,
		     GFile *file,
		     XbNodeExportFlags flags,
		     GCancellable *cancellable,
		     GError **error)
{
	g_autoptr(GString) xml = NULL;

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	xml = xb_silo_export_with_root (self, NULL, flags, error);
	if (xml == NULL)
		return FALSE;
	return g_file_replace_contents (file,
					xml->str,
					xml->len,
					NULL, /* etag */
					FALSE, /* make-backup */
					G_FILE_CREATE_NONE,
					NULL, /* new etag */
					cancellable,
					error);
}
