/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <string.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "xb-builder.h"
#include "xb-node-private.h"
#include "xb-opcode.h"
#include "xb-silo-private.h"
#include "xb-string-private.h"

typedef struct {
	GObject			 parent_instance;
	GMappedFile		*mmap;
	gchar			*guid;
	gboolean		 valid;
	GBytes			*blob;
	const guint8		*data;	/* pointers into ->blob */
	guint32			 datasz;
	guint32			 strtab;
	GHashTable		*strtab_tags;
	GHashTable		*nodes;
	GMutex			 nodes_mutex;
	GHashTable		*file_monitors;	/* of fn:XbSiloFileMonitorItem */
	XbMachine		*machine;
	XbSiloProfileFlags	 profile_flags;
	GString			*profile_str;
} XbSiloPrivate;

typedef struct {
	GFileMonitor		*file_monitor;
	gulong			 file_monitor_id;
} XbSiloFileMonitorItem;

G_DEFINE_TYPE_WITH_PRIVATE (XbSilo, xb_silo, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (xb_silo_get_instance_private (o))

enum {
	PROP_0,
	PROP_GUID,
	PROP_VALID,
	PROP_LAST
};

/* private */
void
xb_silo_add_profile (XbSilo *self, GTimer *timer, const gchar *fmt, ...)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	va_list args;
	g_autoptr(GString) str = g_string_new (NULL);

	/* nothing to do */
	if (!priv->profile_flags)
		return;

	/* add duration */
	if (timer != NULL) {
		g_string_append_printf (str, "%.2fms",
					g_timer_elapsed (timer, NULL) * 1000);
		for (guint i = str->len; i < 12; i++)
			g_string_append (str, " ");
	}

	/* add varargs */
	va_start (args, fmt);
	g_string_append_vprintf (str, fmt, args);
	va_end (args);

	/* do the right thing */
	if (priv->profile_flags & XB_SILO_PROFILE_FLAG_DEBUG)
		g_debug ("%s", str->str);
	if (priv->profile_flags & XB_SILO_PROFILE_FLAG_APPEND)
		g_string_append_printf (priv->profile_str, "%s\n", str->str);

	/* reset automatically */
	if (timer != NULL)
		g_timer_reset (timer);
}

/* private */
const gchar *
xb_silo_from_strtab (XbSilo *self, guint32 offset)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	if (offset == XB_SILO_UNSET)
		return NULL;
	if (offset >= priv->datasz - priv->strtab) {
		g_critical ("strtab+offset is outside the data range for %u", offset);
		return NULL;
	}
	return (const gchar *) (priv->data + priv->strtab + offset);
}

/* private */
inline XbSiloNode *
xb_silo_get_node (XbSilo *self, guint32 off)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	return (XbSiloNode *) (priv->data + off);
}

/* private */
XbSiloAttr *
xb_silo_get_attr (XbSilo *self, guint32 off, guint8 idx)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	XbSiloNode *n = xb_silo_get_node (self, off);
	off += sizeof(XbSiloNode);
	off += sizeof(XbSiloAttr) * idx;
	if (!n->has_text)
		off -= sizeof(guint32);
	return (XbSiloAttr *) (priv->data + off);
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
	XbSiloPrivate *priv = GET_PRIVATE (self);
	return ((const guint8 *) n) - priv->data;
}

/* private */
guint32
xb_silo_get_strtab (XbSilo *self)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	return priv->strtab;
}

/* private */
XbSiloNode *
xb_silo_get_sroot (XbSilo *self)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	if (priv->blob == NULL)
		return NULL;
	if (g_bytes_get_size (priv->blob) <= sizeof(XbSiloHeader))
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

/* private */
guint32
xb_silo_get_strtab_idx (XbSilo *self, const gchar *element)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	gpointer value = NULL;
	if (!g_hash_table_lookup_extended (priv->strtab_tags, element, NULL, &value))
		return XB_SILO_UNSET;
	return GPOINTER_TO_UINT (value);
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
	XbSiloPrivate *priv = GET_PRIVATE (self);
	XbSiloHeader *hdr = (XbSiloHeader *) priv->data;
	g_autoptr(GString) str = g_string_new (NULL);

	g_return_val_if_fail (XB_IS_SILO (self), NULL);

	g_string_append_printf (str, "magic:        %08x\n", (guint) hdr->magic);
	g_string_append_printf (str, "guid:         %s\n", priv->guid);
	g_string_append_printf (str, "strtab:       @%" G_GUINT32_FORMAT "\n", hdr->strtab);
	g_string_append_printf (str, "strtab_ntags: %" G_GUINT16_FORMAT "\n", hdr->strtab_ntags);
	while (off < priv->strtab) {
		XbSiloNode *n = xb_silo_get_node (self, off);
		if (n->is_node) {
			g_string_append_printf (str, "NODE @%" G_GUINT32_FORMAT "\n", off);
			g_string_append_printf (str, "element_name: %s [%03u]\n",
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
				g_string_append_printf (str, "attr_name:    %s [%03u]\n",
							xb_silo_from_strtab (self, a->attr_name),
							a->attr_name);
				g_string_append_printf (str, "attr_value:   %s [%03u]\n",
							xb_silo_from_strtab (self, a->attr_value),
							a->attr_value);
			}
		} else {
			g_string_append_printf (str, "SENT @%" G_GUINT32_FORMAT "\n", off);
		}
		off += xb_silo_node_get_size (n);
	}

	/* add strtab */
	g_string_append_printf (str, "STRTAB @%" G_GUINT32_FORMAT "\n", hdr->strtab);
	for (off = 0; off < priv->datasz - hdr->strtab;) {
		const gchar *tmp = xb_silo_from_strtab (self, off);
		if (tmp == NULL)
			break;
		g_string_append_printf (str, "[%03u]: %s\n", off, tmp);
		off += strlen (tmp) + 1;
	}

	/* success */
	return g_string_free (g_steal_pointer (&str), FALSE);
}

/* private */
const gchar *
xb_silo_node_get_text (XbSilo *self, XbSiloNode *n)
{
	if (!n->has_text)
		return NULL;
	return xb_silo_from_strtab (self, n->text);
}

/* private */
const gchar *
xb_silo_node_get_element (XbSilo *self, XbSiloNode *n)
{
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
	XbSiloPrivate *priv = GET_PRIVATE (self);
	guint32 off = sizeof(XbSiloHeader);
	guint nodes_cnt = 0;

	g_return_val_if_fail (XB_IS_SILO (self), 0);

	while (off < priv->strtab) {
		XbSiloNode *n = xb_silo_get_node (self, off);
		if (n->is_node)
			nodes_cnt += 1;
		off += xb_silo_node_get_size (n);
	}

	/* success */
	return nodes_cnt;
}

/**
 * xb_silo_is_valid:
 * @self: a #XbSilo
 *
 * Checks is the silo is valid. The usual reason the silo is invalidated is
 * when the backing mmapped file has changed, or one of the imported files have
 * been modified.
 *
 * Returns: %TRUE if valid
 *
 * Since: 0.1.0
 **/
gboolean
xb_silo_is_valid (XbSilo *self)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	return priv->valid;
}

/* private */
gboolean
xb_silo_is_empty (XbSilo *self)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	return priv->strtab == sizeof(XbSiloHeader);
}

/**
 * xb_silo_invalidate:
 * @self: a #XbSilo
 *
 * Invalidates a silo. Future calls xb_silo_is_valid() will return %FALSE.
 *
 * Since: 0.1.1
 **/
void
xb_silo_invalidate (XbSilo *self)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	if (!priv->valid)
		return;
	priv->valid = FALSE;
	g_object_notify (G_OBJECT (self), "valid");
}

/* private */
void
xb_silo_uninvalidate (XbSilo *self)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	if (priv->valid)
		return;
	priv->valid = TRUE;
	g_object_notify (G_OBJECT (self), "valid");
}

/* private */
guint
xb_silo_node_get_depth (XbSilo *self, XbSiloNode *n)
{
	guint depth = 0;
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
	XbSiloPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	if (priv->blob == NULL)
		return NULL;
	return g_bytes_ref (priv->blob);
}

/**
 * xb_silo_get_guid:
 * @self: a #XbSilo
 *
 * Gets the GUID used to identify this silo.
 *
 * Returns: a string, otherwise %NULL
 *
 * Since: 0.1.0
 **/
const gchar *
xb_silo_get_guid (XbSilo *self)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	return priv->guid;
}

/* private */
XbMachine *
xb_silo_get_machine (XbSilo *self)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	return priv->machine;
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
	XbSiloHeader *hdr;
	XbSiloPrivate *priv = GET_PRIVATE (self);
	gsize sz = 0;
	gchar guid[UUID_STR_LEN] = { '\0' };
	guint32 off = 0;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->nodes_mutex);
	g_autoptr(GTimer) timer = g_timer_new ();

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (blob != NULL, FALSE);
	g_return_val_if_fail (locker != NULL, FALSE);

	/* no longer valid */
	g_hash_table_remove_all (priv->nodes);
	g_hash_table_remove_all (priv->strtab_tags);
	g_clear_pointer (&priv->guid, g_free);

	/* refcount internally */
	if (priv->blob != NULL)
		g_bytes_unref (priv->blob);
	priv->blob = g_bytes_ref (blob);

	/* update pointers into blob */
	priv->data = g_bytes_get_data (priv->blob, &sz);
	priv->datasz = (guint32) sz;

	/* check size */
	if (sz < sizeof(XbSiloHeader)) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "blob too small");
		return FALSE;
	}

	/* check header magic */
	hdr = (XbSiloHeader *) priv->data;
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

	/* get GUID */
	uuid_unparse (hdr->guid, guid);
	priv->guid = g_strdup (guid);

	/* check strtab */
	priv->strtab = hdr->strtab;
	if (priv->strtab > priv->datasz) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "strtab incorrect");
		return FALSE;
	}

	/* load strtab_tags */
	for (guint16 i = 0; i < hdr->strtab_ntags; i++) {
		const gchar *tmp = xb_silo_from_strtab (self, off);
		if (tmp == NULL) {
			g_set_error_literal (error,
					     G_IO_ERROR,
					     G_IO_ERROR_INVALID_DATA,
					     "strtab_ntags incorrect");
			return FALSE;
		}
		g_hash_table_insert (priv->strtab_tags,
				     (gpointer) tmp,
				     GUINT_TO_POINTER (off));
		off += strlen (tmp) + 1;
	}

	/* profile */
	xb_silo_add_profile (self, timer, "parse blob");

	/* success */
	priv->valid = TRUE;
	return TRUE;
}

/**
 * xb_silo_get_profile_string:
 * @self: a #XbSilo
 *
 * Returns the profiling data. This will only return profiling text if
 * xb_silo_set_profile_flags() was used with %XB_SILO_PROFILE_FLAG_APPEND.
 *
 * Returns: text profiling data
 *
 * Since: 0.1.1
 **/
const gchar *
xb_silo_get_profile_string (XbSilo *self)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	return priv->profile_str->str;
}

/**
 * xb_silo_set_profile_flags:
 * @self: a #XbSilo
 * @profile_flags: some #XbSiloProfileFlags, e.g. %XB_SILO_PROFILE_FLAG_DEBUG
 *
 * Enables or disables the collection of profiling data.
 *
 * Since: 0.1.1
 **/
void
xb_silo_set_profile_flags (XbSilo *self, XbSiloProfileFlags profile_flags)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_SILO (self));
	priv->profile_flags = profile_flags;
}

/* private */
XbSiloProfileFlags
xb_silo_get_profile_flags (XbSilo *self)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	return priv->profile_flags;
}

static void
xb_silo_watch_file_cb (GFileMonitor *monitor,
		       GFile *file,
		       GFile *other_file,
		       GFileMonitorEvent event_type,
		       gpointer user_data)
{
	XbSilo *silo = XB_SILO (user_data);
	g_autofree gchar *fn = g_file_get_path (file);
	g_debug ("%s changed, invalidating", fn);
	xb_silo_invalidate (silo);
}

/**
 * xb_silo_watch_file:
 * @self: a #XbSilo
 * @file: a #GFile
 * @cancellable: a #GCancellable, or %NULL
 * @error: the #GError, or %NULL
 *
 * Adds a file monitor to the silo. If the file or directory for @file changes
 * then the silo will be invalidated.
 *
 * Returns: %TRUE for success, otherwise @error is set.
 *
 * Since: 0.1.0
 **/
gboolean
xb_silo_watch_file (XbSilo *self,
		    GFile *file,
		    GCancellable *cancellable,
		    GError **error)
{
	XbSiloFileMonitorItem *item;
	XbSiloPrivate *priv = GET_PRIVATE (self);
	g_autofree gchar *fn = g_file_get_path (file);
	g_autoptr(GFileMonitor) file_monitor = NULL;

	/* already exists */
	item = g_hash_table_lookup (priv->file_monitors, fn);
	if (item != NULL)
		return TRUE;

	/* try to create */
	file_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE,
					    cancellable, error);
	if (file_monitor == NULL)
		return FALSE;
	g_file_monitor_set_rate_limit (file_monitor, 20);

	/* add */
	item = g_slice_new0 (XbSiloFileMonitorItem);
	item->file_monitor = g_object_ref (file_monitor);
	item->file_monitor_id = g_signal_connect (file_monitor, "changed",
						  G_CALLBACK (xb_silo_watch_file_cb), self);
	g_hash_table_insert (priv->file_monitors, g_steal_pointer (&fn), item);
	return TRUE;
}

/**
 * xb_silo_load_from_file:
 * @self: a #XbSilo
 * @file: a #GFile
 * @flags: #XbSiloLoadFlags, e.g. %XB_SILO_LOAD_FLAG_NONE
 * @cancellable: a #GCancellable, or %NULL
 * @error: the #GError, or %NULL
 *
 * Loads a silo from file.
 *
 * Returns: %TRUE for success, otherwise @error is set.
 *
 * Since: 0.1.0
 **/
gboolean
xb_silo_load_from_file (XbSilo *self,
			GFile *file,
			XbSiloLoadFlags flags,
			GCancellable *cancellable,
			GError **error)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	g_autofree gchar *fn = NULL;
	g_autoptr(GBytes) blob = NULL;
	g_autoptr(GTimer) timer = g_timer_new ();

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	/* no longer valid */
	g_hash_table_remove_all (priv->file_monitors);
	g_hash_table_remove_all (priv->nodes);
	g_hash_table_remove_all (priv->strtab_tags);
	g_clear_pointer (&priv->guid, g_free);
	if (priv->mmap != NULL)
		g_mapped_file_unref (priv->mmap);

	fn = g_file_get_path (file);
	priv->mmap = g_mapped_file_new (fn, FALSE, error);
	if (priv->mmap == NULL)
		return FALSE;
	blob = g_mapped_file_get_bytes (priv->mmap);
	if (!xb_silo_load_from_bytes (self, blob, flags, error))
		return FALSE;

	/* watch file for changes */
	if (flags & XB_SILO_LOAD_FLAG_WATCH_BLOB) {
		if (!xb_silo_watch_file (self, file, cancellable, error))
			return FALSE;
	}

	/* success */
	xb_silo_add_profile (self, timer, "loaded file");
	return TRUE;
}

/**
 * xb_silo_save_to_file:
 * @self: a #XbSilo
 * @file: a #GFile
 * @cancellable: a #GCancellable, or %NULL
 * @error: the #GError, or %NULL
 *
 * Saves a silo to a file.
 *
 * Returns: %TRUE for success, otherwise @error is set.
 *
 * Since: 0.1.0
 **/
gboolean
xb_silo_save_to_file (XbSilo *self,
		      GFile *file,
		      GCancellable *cancellable,
		      GError **error)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	g_autoptr(GFile) file_parent = NULL;
	g_autoptr(GTimer) timer = g_timer_new ();

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	/* invalid */
	if (priv->data == NULL) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_INITIALIZED,
				     "no data to save");
		return FALSE;
	}

	/* ensure parent directories exist */
	file_parent = g_file_get_parent (file);
	if (file_parent != NULL &&
	    !g_file_query_exists (file_parent, cancellable)) {
		if (!g_file_make_directory_with_parents (file_parent,
							 cancellable,
							 error))
			return FALSE;
	}

	/* save and then rename */
	if (!g_file_replace_contents (file,
				      (const gchar *) priv->data,
				      (gsize) priv->datasz, NULL, FALSE,
				      G_FILE_CREATE_NONE, NULL,
				      cancellable, error)) {
		return FALSE;
	}
	xb_silo_add_profile (self, timer, "save file");
	return TRUE;
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
	g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
	g_return_val_if_fail (xml != NULL, NULL);
	if (!xb_builder_source_load_xml (source, xml, XB_BUILDER_SOURCE_FLAG_NONE, error))
		return NULL;
	xb_builder_import_source (builder, source);
	return xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, error);
}

/* private */
XbNode *
xb_silo_node_create (XbSilo *self, XbSiloNode *sn)
{
	XbNode *n;
	XbSiloPrivate *priv = GET_PRIVATE (self);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->nodes_mutex);

	g_return_val_if_fail (locker != NULL, NULL);

	/* does already exist */
	n = g_hash_table_lookup (priv->nodes, sn);
	if (n != NULL)
		return g_object_ref (n);

	/* create and add */
	n = xb_node_new (self, sn);
	g_hash_table_insert (priv->nodes, sn, g_object_ref (n));
	return n;
}

/* convert [2] to position()=2 */
static gboolean
xb_silo_machine_fixup_position_cb (XbMachine *self,
				   GPtrArray *opcodes,
				   gpointer user_data,
				   GError **error)
{
	g_ptr_array_add (opcodes, xb_machine_opcode_func_new (self, "position"));
	g_ptr_array_add (opcodes, xb_machine_opcode_func_new (self, "eq"));
	return TRUE;
}

/* convert "'type' attr()" -> "'type' attr() '(null)' ne()" */
static gboolean
xb_silo_machine_fixup_attr_exists_cb (XbMachine *self,
				      GPtrArray *opcodes,
				      gpointer user_data,
				      GError **error)
{
	g_ptr_array_add (opcodes, xb_opcode_text_new_static (NULL));
	g_ptr_array_add (opcodes, xb_machine_opcode_func_new (self, "ne"));
	return TRUE;
}

static gboolean
xb_silo_machine_func_attr_cb (XbMachine *self,
			      GPtrArray *stack,
			      gboolean *result,
			      gpointer user_data,
			      gpointer exec_data,
			      GError **error)
{
	XbSilo *silo = XB_SILO (user_data);
	XbSiloQueryData *query_data = (XbSiloQueryData *) exec_data;
	g_autoptr(XbOpcode) op = xb_machine_stack_pop (self, stack);
	const gchar *tmp = xb_opcode_get_str (op);
	xb_machine_stack_push_text_static (self, stack,
					   xb_silo_node_get_attr (silo, query_data->sn, tmp));
	return TRUE;
}

static gboolean
xb_silo_machine_func_text_cb (XbMachine *self,
			      GPtrArray *stack,
			      gboolean *result,
			      gpointer user_data,
			      gpointer exec_data,
			      GError **error)
{
	XbSilo *silo = XB_SILO (user_data);
	XbSiloQueryData *query_data = (XbSiloQueryData *) exec_data;
	xb_machine_stack_push_text_static (self, stack,
					   xb_silo_node_get_text (silo, query_data->sn));
	return TRUE;
}

static gboolean
xb_silo_machine_func_first_cb (XbMachine *self,
			       GPtrArray *stack,
			       gboolean *result,
			       gpointer user_data,
			       gpointer exec_data,
			       GError **error)
{
	XbSiloQueryData *query_data = (XbSiloQueryData *) exec_data;
	*result = query_data->position == 1;
	return TRUE;
}

static gboolean
xb_silo_machine_func_last_cb (XbMachine *self,
			      GPtrArray *stack,
			      gboolean *result,
			      gpointer user_data,
			      gpointer exec_data,
			      GError **error)
{
	XbSiloQueryData *query_data = (XbSiloQueryData *) exec_data;
	*result = query_data->sn->next == 0;
	return TRUE;
}

static gboolean
xb_silo_machine_func_position_cb (XbMachine *self,
				  GPtrArray *stack,
				  gboolean *result,
				  gpointer user_data,
				  gpointer exec_data,
				  GError **error)
{
	XbSiloQueryData *query_data = (XbSiloQueryData *) exec_data;
	xb_machine_stack_push_integer (self, stack, query_data->position);
	return TRUE;
}

static gboolean
xb_silo_machine_func_search_cb (XbMachine *self,
				GPtrArray *stack,
				gboolean *result,
				gpointer user_data,
				gpointer exec_data,
				GError **error)
{
	g_autoptr(XbOpcode) op1 = xb_machine_stack_pop (self, stack);
	g_autoptr(XbOpcode) op2 = xb_machine_stack_pop (self, stack);

	/* TEXT:TEXT */
	if (xb_opcode_cmp_str (op1) && xb_opcode_cmp_str (op2)) {
		*result = xb_string_search (xb_opcode_get_str (op2),
					    xb_opcode_get_str (op1));
		return TRUE;
	}

	/* fail */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "%s:%s types not supported",
		     xb_opcode_kind_to_string (xb_opcode_get_kind (op1)),
		     xb_opcode_kind_to_string (xb_opcode_get_kind (op2)));
	return FALSE;
}

static gboolean
xb_silo_machine_fixup_attr_text_cb (XbMachine *self,
				    GPtrArray *opcodes,
				    const gchar *text,
				    gboolean *handled,
				    gpointer user_data,
				    GError **error)
{
	/* @foo -> attr(foo) */
	if (g_str_has_prefix (text, "@")) {
		XbOpcode *opcode;
		opcode = xb_machine_opcode_func_new (self, "attr");
		if (opcode == NULL) {
			g_set_error_literal (error,
					     G_IO_ERROR,
					     G_IO_ERROR_NOT_SUPPORTED,
					     "no attr opcode");
			return FALSE;
		}
		g_ptr_array_add (opcodes, xb_opcode_text_new (text + 1));
		g_ptr_array_add (opcodes, opcode);
		*handled = TRUE;
		return TRUE;
	}

	/* not us */
	return TRUE;
}

static void
xb_silo_file_monitor_item_free (XbSiloFileMonitorItem *item)
{
	g_signal_handler_disconnect (item->file_monitor, item->file_monitor_id);
	g_object_unref (item->file_monitor);
	g_slice_free (XbSiloFileMonitorItem, item);
}

static void
xb_silo_get_property (GObject *obj, guint prop_id, GValue *value, GParamSpec *pspec)
{
	XbSilo *self = XB_SILO (obj);
	XbSiloPrivate *priv = GET_PRIVATE (self);
	switch (prop_id) {
	case PROP_GUID:
		g_value_set_string (value, priv->guid);
		break;
	case PROP_VALID:
		g_value_set_boolean (value, priv->valid);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
xb_silo_set_property (GObject *obj, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	XbSilo *self = XB_SILO (obj);
	XbSiloPrivate *priv = GET_PRIVATE (self);
	switch (prop_id) {
	case PROP_GUID:
		g_free (priv->guid);
		priv->guid = g_value_dup_string (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
		break;
	}
}

static void
xb_silo_init (XbSilo *self)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	priv->file_monitors = g_hash_table_new_full (g_str_hash, g_str_equal,
						     g_free, (GDestroyNotify) xb_silo_file_monitor_item_free);
	priv->nodes = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					     NULL, (GDestroyNotify) g_object_unref);
	priv->strtab_tags = g_hash_table_new (g_str_hash, g_str_equal);
	priv->profile_str = g_string_new (NULL);

	g_mutex_init (&priv->nodes_mutex);

	priv->machine = xb_machine_new ();
	xb_machine_add_method (priv->machine, "attr", 1,
			       xb_silo_machine_func_attr_cb, self, NULL);
	xb_machine_add_method (priv->machine, "text", 0,
			       xb_silo_machine_func_text_cb, self, NULL);
	xb_machine_add_method (priv->machine, "first", 0,
			       xb_silo_machine_func_first_cb, self, NULL);
	xb_machine_add_method (priv->machine, "last", 0,
			       xb_silo_machine_func_last_cb, self, NULL);
	xb_machine_add_method (priv->machine, "position", 0,
			       xb_silo_machine_func_position_cb, self, NULL);
	xb_machine_add_method (priv->machine, "search", 2,
			       xb_silo_machine_func_search_cb, self, NULL);
	xb_machine_add_operator (priv->machine, "~=", "search");
	xb_machine_add_opcode_fixup (priv->machine, "INTE",
				     xb_silo_machine_fixup_position_cb, self, NULL);
	xb_machine_add_opcode_fixup (priv->machine, "TEXT,FUNC:attr",
				     xb_silo_machine_fixup_attr_exists_cb, self, NULL);
	xb_machine_add_text_handler (priv->machine,
				     xb_silo_machine_fixup_attr_text_cb, self, NULL);
}

static void
xb_silo_finalize (GObject *obj)
{
	XbSilo *self = XB_SILO (obj);
	XbSiloPrivate *priv = GET_PRIVATE (self);

	g_mutex_clear (&priv->nodes_mutex);

	g_free (priv->guid);
	g_string_free (priv->profile_str, TRUE);
	g_object_unref (priv->machine);
	g_hash_table_unref (priv->file_monitors);
	g_hash_table_unref (priv->nodes);
	g_hash_table_unref (priv->strtab_tags);
	if (priv->mmap != NULL)
		g_mapped_file_unref (priv->mmap);
	if (priv->blob != NULL)
		g_bytes_unref (priv->blob);
	G_OBJECT_CLASS (xb_silo_parent_class)->finalize (obj);
}

static void
xb_silo_class_init (XbSiloClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = xb_silo_finalize;
	object_class->get_property = xb_silo_get_property;
	object_class->set_property = xb_silo_set_property;

	/**
	 * XbSilo:guid:
	 */
	pspec = g_param_spec_string ("guid", NULL, NULL, NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_GUID, pspec);

	/**
	 * XbSilo:allow-cancel:
	 */
	pspec = g_param_spec_boolean ("valid", NULL, NULL, TRUE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_VALID, pspec);
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
