/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>

#include "xb-builder.h"
#include "xb-node-private.h"
#include "xb-silo-private.h"

struct _XbSilo
{
	GObject			 parent_instance;
	GMappedFile		*mmap;
	GBytes			*blob;
	const guint8		*data;	/* pointers into ->blob */
	guint32			 datasz;
	guint32			 strtab;
	GHashTable		*nodes;
};

G_DEFINE_TYPE (XbSilo, xb_silo, G_TYPE_OBJECT)

/* private */
const gchar *
xb_silo_from_strtab (XbSilo *self, guint32 offset)
{
	if (offset == XB_SILO_UNSET)
		return NULL;
	if (offset >= self->datasz - self->strtab) {
		g_critical ("strtab+offset is outside the data range for %u", offset);
		return NULL;
	}
	return (const gchar *) (self->data + self->strtab + offset);
}

/* private */
XbSiloNode *
xb_silo_get_node (XbSilo *self, guint32 off)
{
	return (XbSiloNode *) (self->data + off);
}

/* private */
XbSiloAttr *
xb_silo_get_attr (XbSilo *self, guint32 off, guint8 idx)
{
	XbSiloNode *n = xb_silo_get_node (self, off);
	off += sizeof(XbSiloNode);
	off += sizeof(XbSiloAttr) * idx;
	if (!n->has_text)
		off -= sizeof(guint32);
	return (XbSiloAttr *) (self->data + off);
}

/* private */
guint8
xb_silo_node_get_size (XbSiloNode *n)
{
	if (n->is_node) {
		guint8 sz = sizeof(XbSiloNode);
		sz += n->nr_attrs * sizeof(XbSiloAttr);
		if (!n->has_text)
			sz -= sizeof(guint32);
		return sz;
	}
	/* sentinel */
	return 1;
}

/* private */
guint32
xb_silo_get_offset_for_node (XbSilo *self, XbSiloNode *n)
{
	return ((const guint8 *) n) - self->data;
}

/* private */
guint32
xb_silo_get_strtab (XbSilo *self)
{
	return self->strtab;
}

/* private */
XbSiloNode *
xb_silo_get_sroot (XbSilo *self)
{
	if (g_bytes_get_size (self->blob) <= sizeof(XbSiloHeader))
		return NULL;
	return xb_silo_get_node (self, sizeof(XbSiloHeader));
}

/* private */
XbSiloNode *
xb_silo_node_get_parent (XbSilo *self, XbSiloNode *n)
{
	if (n->parent == 0x0)
		return NULL;
	return xb_silo_get_node (self, n->parent);
}

/* private */
XbSiloNode *
xb_silo_node_get_next (XbSilo *self, XbSiloNode *n)
{
	if (n->next == 0x0)
		return NULL;
	return xb_silo_get_node (self, n->next);
}

/* private */
XbSiloNode *
xb_silo_node_get_child (XbSilo *self, XbSiloNode *n)
{
	XbSiloNode *c;
	guint32 off = xb_silo_get_offset_for_node (self, n);
	off += xb_silo_node_get_size (n);

	/* check for sentinel */
	c = xb_silo_get_node (self, off);
	if (!c->is_node)
		return NULL;
	return c;
}

/**
 * xb_silo_get_root:
 * @self: a #XbSilo
 *
 * Gets the root node for the silo. (MIGHT BE MORE).
 *
 * Returns: (transfer full): A #XbNode, or %NULL for an error
 *
 * Since: 0.1.0
 **/
XbNode *
xb_silo_get_root (XbSilo *self)
{
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	return xb_silo_node_create (self, xb_silo_get_sroot (self));
}

/**
 * xb_silo_to_string:
 * @self: a #XbSilo
 * @error: the #GError, or %NULL
 *
 * Converts the silo to an internal string representation. This is only
 * really useful for debugging #XbSilo itself.
 *
 * Returns: A string, or %NULL for an error
 *
 * Since: 0.1.0
 **/
gchar *
xb_silo_to_string (XbSilo *self, GError **error)
{
	guint32 off = sizeof(XbSiloHeader);
	XbSiloHeader *hdr = (XbSiloHeader *) self->data;
	g_autoptr(GString) str = g_string_new (NULL);

	g_return_val_if_fail (XB_IS_SILO (self), NULL);

	g_string_append_printf (str, "magic:        %08x\n", (guint) hdr->magic);
	g_string_append_printf (str, "guid:         %u\n", hdr->guid[0]);
	g_string_append_printf (str, "strtab:       @%" G_GUINT32_FORMAT "\n", self->strtab);
	while (off < self->strtab) {
		XbSiloNode *n = xb_silo_get_node (self, off);
		if (n->is_node) {
			g_string_append_printf (str, "NODE @%" G_GUINT32_FORMAT " (%p)\n", off, n);
			g_string_append_printf (str, "element_name: %s (%u)\n",
						xb_silo_from_strtab (self, n->element_name),
						n->element_name);
			g_string_append_printf (str, "next:         %" G_GUINT32_FORMAT "\n", n->next);
			g_string_append_printf (str, "parent:       %" G_GUINT32_FORMAT "\n", n->parent);
			if (n->has_text) {
				g_string_append_printf (str, "text:         %s\n",
							xb_silo_from_strtab (self, n->text));
			}
			for (guint8 i = 0; i < n->nr_attrs; i++) {
				XbSiloAttr *a = xb_silo_get_attr (self, off, i);
				g_string_append_printf (str, "attr_name:    %s (%u)\n",
							xb_silo_from_strtab (self, a->attr_name),
							a->attr_name);
				g_string_append_printf (str, "attr_value:   %s (%u)\n",
							xb_silo_from_strtab (self, a->attr_value),
							a->attr_value);
			}
		} else {
			g_string_append_printf (str, "SENT @%" G_GUINT32_FORMAT "\n", off);
		}
		off += xb_silo_node_get_size (n);
	}

	/* success */
	return g_string_free (g_steal_pointer (&str), FALSE);
}

/* private */
const gchar *
xb_silo_node_get_text (XbSilo *self, XbSiloNode *n)
{
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	g_return_val_if_fail (n != NULL, NULL);
	g_return_val_if_fail (n->is_node, NULL);
	if (!n->has_text)
		return NULL;
	return xb_silo_from_strtab (self, n->text);
}

/* private */
const gchar *
xb_silo_node_get_element (XbSilo *self, XbSiloNode *n)
{
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	g_return_val_if_fail (n != NULL, NULL);
	g_return_val_if_fail (n->is_node, NULL);
	return xb_silo_from_strtab (self, n->element_name);
}

/* private */
const gchar *
xb_silo_node_get_attr (XbSilo *self, XbSiloNode *n, const gchar *name)
{
	guint32 off;

	/* calculate offset to fist attribute */
	off = xb_silo_get_offset_for_node (self, n);
	for (guint8 i = 0; i < n->nr_attrs; i++) {
		XbSiloAttr *a = xb_silo_get_attr (self, off, i);
		if (g_strcmp0 (xb_silo_from_strtab (self, a->attr_name), name) == 0)
			return xb_silo_from_strtab (self, a->attr_value);
	}

	/* nothing matched */
	return NULL;
}

/**
 * xb_silo_get_size:
 * @self: a #XbSilo
 *
 * Gets the number of nodes in the silo.
 *
 * Returns: a integer, or 0 is an empty blob
 *
 * Since: 0.1.0
 **/
guint
xb_silo_get_size (XbSilo *self)
{
	guint32 off = sizeof(XbSiloHeader);
	guint nodes_cnt = 0;

	g_return_val_if_fail (XB_IS_SILO (self), 0);

	while (off < self->strtab) {
		XbSiloNode *n = xb_silo_get_node (self, off);
		if (n->is_node)
			nodes_cnt += 1;
		off += xb_silo_node_get_size (n);
	}

	/* success */
	return nodes_cnt;
}

/* private */
guint
xb_silo_node_get_depth (XbSilo *self, XbSiloNode *n)
{
	guint depth = 0;

	g_return_val_if_fail (n != NULL, 0);

	while (n->parent != 0) {
		depth++;
		n = xb_silo_get_node (self, n->parent);
	}
	return depth;
}

/**
 * xb_silo_get_bytes:
 * @self: a #XbSilo
 *
 * Gets the backing object that created the blob.
 *
 * You should never *ever* modify this data.
 *
 * Returns: (transfer full): A #GBytes, or %NULL if never set
 *
 * Since: 0.1.0
 **/
GBytes *
xb_silo_get_bytes (XbSilo *self)
{
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	if (self->blob == NULL)
		return NULL;
	return g_bytes_ref (self->blob);
}

/**
 * xb_silo_load_from_bytes:
 * @self: a #XbSilo
 * @blob: a #GBytes
 * @flags: #XbSiloLoadFlags, e.g. %XB_SILO_LOAD_FLAG_NONE
 * @error: the #GError, or %NULL
 *
 * Loads a silo from memory location.
 *
 * Returns: %TRUE for success, otherwise @error is set.
 *
 * Since: 0.1.0
 **/
gboolean
xb_silo_load_from_bytes (XbSilo *self, GBytes *blob, XbSiloLoadFlags flags, GError **error)
{
	XbSiloHeader *hdr = (XbSiloHeader *) self->data;
	gsize sz = 0;

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (blob != NULL, FALSE);

	/* no longer valid */
	g_hash_table_remove_all (self->nodes);

	/* refcount internally */
	if (self->blob != NULL)
		g_bytes_unref (self->blob);
	self->blob = g_bytes_ref (blob);

	/* update pointers into blob */
	self->data = g_bytes_get_data (self->blob, &sz);
	self->datasz = (guint32) sz;

	/* check size */
	if (sz < sizeof(XbSiloHeader)) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "blob too small");
		return FALSE;
	}

	/* check header magic */
	hdr = (XbSiloHeader *) self->data;
	if ((flags & XB_SILO_LOAD_FLAG_NO_MAGIC) == 0) {
		if (hdr->magic != XB_SILO_MAGIC_BYTES) {
			g_set_error_literal (error,
					     G_IO_ERROR,
					     G_IO_ERROR_INVALID_DATA,
					     "magic incorrect");
			return FALSE;
		}
		if (hdr->version != XB_SILO_VERSION) {
			g_set_error_literal (error,
					     G_IO_ERROR,
					     G_IO_ERROR_INVALID_DATA,
					     "version incorrect");
			return FALSE;
		}
	}

	/* check strtab */
	self->strtab = hdr->strtab;
	if (self->strtab > self->datasz) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "strtab incorrect");
		return FALSE;
	}

	/* success */
	return TRUE;
}

/**
 * xb_silo_load_from_file:
 * @self: a #XbSilo
 * @fn: an absolute filename
 * @flags: #XbSiloLoadFlags, e.g. %XB_SILO_LOAD_FLAG_NONE
 * @error: the #GError, or %NULL
 *
 * Loads a silo from file.
 *
 * Returns: %TRUE for success, otherwise @error is set.
 *
 * Since: 0.1.0
 **/
gboolean
xb_silo_load_from_file (XbSilo *self, const gchar *fn, XbSiloLoadFlags flags, GError **error)
{
	g_autoptr(GBytes) blob = NULL;

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (fn != NULL, FALSE);

	self->mmap = g_mapped_file_new (fn, FALSE, error);
	if (self->mmap == NULL)
		return FALSE;
	blob = g_mapped_file_get_bytes (self->mmap);
	return xb_silo_load_from_bytes (self, blob, flags, error);
}

/**
 * xb_silo_save_to_file:
 * @self: a #XbSilo
 * @fn: a filename
 * @error: the #GError, or %NULL
 *
 * Saves a silo to a file.
 *
 * Returns: %TRUE for success, otherwise @error is set.
 *
 * Since: 0.1.0
 **/
gboolean
xb_silo_save_to_file (XbSilo *self, const gchar *fn, GError **error)
{
	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (fn != NULL, FALSE);

	/* invalid */
	if (self->data == NULL) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_INITIALIZED,
				     "no data to save");
		return FALSE;
	}

	/* save and then rename */
	return g_file_set_contents (fn, (const gchar *) self->data, (gsize) self->datasz, error);
}

/**
 * xb_silo_new_from_xml:
 * @xml: XML string
 * @error: the #GError, or %NULL
 *
 * Creates a new silo from an XML string.
 *
 * Returns: a new #XbSilo, or %NULL
 *
 * Since: 0.1.0
 **/
XbSilo *
xb_silo_new_from_xml (const gchar *xml, GError **error)
{
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_return_val_if_fail (xml != NULL, NULL);
	if (!xb_builder_import_xml (builder, xml, error))
		return NULL;
	return xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, error);
}

/* private */
XbNode *
xb_silo_node_create (XbSilo *self, XbSiloNode *sn)
{
	XbNode *n;

	/* does already exist */
	n = g_hash_table_lookup (self->nodes, sn);
	if (n != NULL)
		return g_object_ref (n);

	/* create and add */
	n = xb_node_new (self, sn);
	g_hash_table_insert (self->nodes, sn, g_object_ref (n));
	return n;
}

static void
xb_silo_init (XbSilo *self)
{
	self->nodes = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					     NULL, (GDestroyNotify) g_object_unref);
}

static void
xb_silo_finalize (GObject *obj)
{
	XbSilo *self = XB_SILO (obj);
	g_hash_table_unref (self->nodes);
	if (self->mmap != NULL)
		g_mapped_file_unref (self->mmap);
	if (self->blob != NULL)
		g_bytes_unref (self->blob);
	G_OBJECT_CLASS (xb_silo_parent_class)->finalize (obj);
}

static void
xb_silo_class_init (XbSiloClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = xb_silo_finalize;
}

/**
 * xb_silo_new:
 *
 * Creates a new silo.
 *
 * Returns: a new #XbSilo
 *
 * Since: 0.1.0
 **/
XbSilo *
xb_silo_new (void)
{
	return g_object_new (XB_TYPE_SILO, NULL);
}
