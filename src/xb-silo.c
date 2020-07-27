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

#ifdef HAVE_LIBSTEMMER
#include <libstemmer.h>
#endif

#include "xb-builder.h"
#include "xb-machine-private.h"
#include "xb-node-private.h"
#include "xb-opcode-private.h"
#include "xb-silo-private.h"
#include "xb-stack-private.h"
#include "xb-string-private.h"

typedef struct {
	GMappedFile		*mmap;
	gchar			*guid;
	gboolean		 valid;
	GBytes			*blob;
	const guint8		*data;	/* pointers into ->blob */
	guint32			 datasz;
	guint32			 strtab;
	GHashTable		*strtab_tags;
	GHashTable		*strindex;
	gboolean		 enable_node_cache;
	GHashTable		*nodes;	/* (mutex nodes_mutex) */
	GMutex			 nodes_mutex;
	GHashTable		*file_monitors;	/* of GFile:XbSiloFileMonitorItem */
	XbMachine		*machine;
	XbSiloProfileFlags	 profile_flags;
	GString			*profile_str;
#ifdef HAVE_LIBSTEMMER
	struct sb_stemmer	*stemmer_ctx;	/* lazy loaded */
	GMutex			 stemmer_mutex;
#endif
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
	PROP_ENABLE_NODE_CACHE,
	PROP_LAST
};

/* private */
GTimer *
xb_silo_start_profile (XbSilo *self)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);

	/* nothing to do; g_timer_new() does a syscall to clock_gettime() which
	 * is best avoided if not needed */
	if (!priv->profile_flags)
		return NULL;

	return g_timer_new ();
}

/* private */
void
xb_silo_add_profile (XbSilo *self, GTimer *timer, const gchar *fmt, ...)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	va_list args;
	g_autoptr(GString) str = NULL;

	/* nothing to do */
	if (!priv->profile_flags)
		return;

	str = g_string_new ("");

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
static gchar *
xb_silo_stem (XbSilo *self, const gchar *value)
{
#ifdef HAVE_LIBSTEMMER
	XbSiloPrivate *priv = GET_PRIVATE (self);
	const gchar *tmp;
	gsize len_dst;
	gsize len_src;
	g_autofree gchar *value_casefold = NULL;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&priv->stemmer_mutex);

	/* not enabled */
	value_casefold = g_utf8_casefold (value, -1);
	if (priv->stemmer_ctx == NULL)
		priv->stemmer_ctx = sb_stemmer_new ("en", NULL);

	/* stem */
	len_src = strlen (value_casefold);
	tmp = (const gchar *) sb_stemmer_stem (priv->stemmer_ctx,
					       (guchar *) value_casefold,
					       (gint) len_src);
	len_dst = (gsize) sb_stemmer_length (priv->stemmer_ctx);
	if (len_src == len_dst)
		return g_steal_pointer (&value_casefold);
	return g_strndup (tmp, len_dst);
#else
	return g_utf8_casefold (value, -1);
#endif
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
void
xb_silo_strtab_index_insert (XbSilo *self, guint32 offset)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	const gchar *tmp;

	/* get the string version */
	tmp = xb_silo_from_strtab (self, offset);
	if (tmp == NULL)
		return;
	if (g_hash_table_lookup (priv->strindex, tmp) != NULL)
		return;
	g_hash_table_insert (priv->strindex,
			     (gpointer) tmp,
			     GUINT_TO_POINTER (offset));
}

/* private */
guint32
xb_silo_strtab_index_lookup (XbSilo *self, const gchar *str)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	gpointer val = NULL;
	if (!g_hash_table_lookup_extended (priv->strindex, str, NULL, &val))
		return XB_SILO_UNSET;
	return GPOINTER_TO_INT (val);
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
	off += sizeof(XbSiloNode);
	off += sizeof(XbSiloAttr) * idx;
	return (XbSiloAttr *) (priv->data + off);
}

/* private */
guint8
xb_silo_node_get_size (XbSiloNode *n)
{
	if (n->is_node) {
		guint8 sz = sizeof(XbSiloNode);
		sz += n->nr_attrs * sizeof(XbSiloAttr);
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
	return xb_silo_node_create (self, xb_silo_get_sroot (self), FALSE);
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
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

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
			if (n->text != XB_SILO_UNSET) {
				g_string_append_printf (str, "text:         %s\n",
							xb_silo_from_strtab (self, n->text));
			}
			if (n->tail != XB_SILO_UNSET) {
				g_string_append_printf (str, "tail:         %s\n",
							xb_silo_from_strtab (self, n->tail));
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
	if (n->text == XB_SILO_UNSET)
		return NULL;
	return xb_silo_from_strtab (self, n->text);
}

/* private */
const gchar *
xb_silo_node_get_tail (XbSilo *self, XbSiloNode *n)
{
	if (n->tail == XB_SILO_UNSET)
		return NULL;
	return xb_silo_from_strtab (self, n->tail);
}

/* private */
const gchar *
xb_silo_node_get_element (XbSilo *self, XbSiloNode *n)
{
	return xb_silo_from_strtab (self, n->element_name);
}

/* private */
XbSiloAttr *
xb_silo_node_get_attr_by_str (XbSilo *self, XbSiloNode *n, const gchar *name)
{
	guint32 off;

	/* calculate offset to first attribute */
	off = xb_silo_get_offset_for_node (self, n);
	for (guint8 i = 0; i < n->nr_attrs; i++) {
		XbSiloAttr *a = xb_silo_get_attr (self, off, i);
		if (g_strcmp0 (xb_silo_from_strtab (self, a->attr_name), name) == 0)
			return a;
	}

	/* nothing matched */
	return NULL;
}

static XbSiloAttr *
xb_silo_node_get_attr_by_val (XbSilo *self, XbSiloNode *n, guint32 name)
{
	guint32 off;

	/* calculate offset to first attribute */
	off = xb_silo_get_offset_for_node (self, n);
	for (guint8 i = 0; i < n->nr_attrs; i++) {
		XbSiloAttr *a = xb_silo_get_attr (self, off, i);
		if (a->attr_name == name)
			return a;
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
	XbGuid guid_tmp;
	XbSiloHeader *hdr;
	XbSiloPrivate *priv = GET_PRIVATE (self);
	gsize sz = 0;
	guint32 off = 0;
	g_autoptr(GMutexLocker) locker = NULL;
	g_autoptr(GTimer) timer = xb_silo_start_profile (self);

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (blob != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* no longer valid */
	if (priv->enable_node_cache) {
		locker = g_mutex_locker_new (&priv->nodes_mutex);
		if (priv->nodes != NULL)
			g_hash_table_remove_all (priv->nodes);
	}

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
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "version incorrect, got %u, expected %d", hdr->version, XB_SILO_VERSION);
			return FALSE;
		}
	}

	/* get GUID */
	memcpy (&guid_tmp, &hdr->guid, sizeof(guid_tmp));
	priv->guid = xb_guid_to_string (&guid_tmp);

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

/**
 * xb_silo_get_enable_node_cache:
 * @self: an #XbSilo
 *
 * Get #XbSilo:enable-node-cache.
 *
 * Since: 0.2.0
 */
gboolean
xb_silo_get_enable_node_cache (XbSilo *self)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	return priv->enable_node_cache;
}

/**
 * xb_silo_set_enable_node_cache:
 * @self: an #XbSilo
 * @enable_node_cache: %TRUE to enable the node cache, %FALSE otherwise
 *
 * Set #XbSilo:enable-node-cache.
 *
 * This is not thread-safe, and can only be called before the #XbSilo is passed
 * between threads.
 *
 * Since: 0.2.0
 */
void
xb_silo_set_enable_node_cache (XbSilo *self, gboolean enable_node_cache)
{
	XbSiloPrivate *priv = GET_PRIVATE (self);

	g_return_if_fail (XB_IS_SILO (self));

	if (priv->enable_node_cache == enable_node_cache)
		return;

	priv->enable_node_cache = enable_node_cache;

	/* if disabling the cache, destroy any existing data structures;
	 * if enabling it, create them lazily when the first entry is cached
	 * (see xb_silo_node_create()) */
	if (!enable_node_cache) {
		g_clear_pointer (&priv->nodes, g_hash_table_unref);
	}

	g_object_notify (G_OBJECT (self), "enable-node-cache");
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
	g_autofree gchar *basename = g_file_get_basename (file);
	if (g_str_has_prefix (basename, ".goutputstream"))
		return;
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
	g_autoptr(GFileMonitor) file_monitor = NULL;

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* already exists */
	item = g_hash_table_lookup (priv->file_monitors, file);
	if (item != NULL)
		return TRUE;

	/* try to create */
	file_monitor = g_file_monitor (file, G_FILE_MONITOR_NONE,
				       cancellable, error);
	if (file_monitor == NULL)
		return FALSE;
	g_file_monitor_set_rate_limit (file_monitor, 20);

	/* add */
	item = g_slice_new0 (XbSiloFileMonitorItem);
	item->file_monitor = g_object_ref (file_monitor);
	item->file_monitor_id = g_signal_connect (file_monitor, "changed",
						  G_CALLBACK (xb_silo_watch_file_cb), self);
	g_hash_table_insert (priv->file_monitors, g_object_ref (file), item);
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
	g_autoptr(GTimer) timer = xb_silo_start_profile (self);

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* no longer valid (@nodes is cleared by xb_silo_load_from_bytes()) */
	g_hash_table_remove_all (priv->file_monitors);
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
	g_autoptr(GTimer) timer = xb_silo_start_profile (self);

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);
	g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

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
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);
	if (!xb_builder_source_load_xml (source, xml, XB_BUILDER_SOURCE_FLAG_NONE, error))
		return NULL;
	xb_builder_import_source (builder, source);
	return xb_builder_compile (builder, XB_BUILDER_COMPILE_FLAG_NONE, NULL, error);
}

/* private */
XbNode *
xb_silo_node_create (XbSilo *self, XbSiloNode *sn, gboolean force_node_cache)
{
	XbNode *n;
	XbSiloPrivate *priv = GET_PRIVATE (self);
	g_autoptr(GMutexLocker) locker = NULL;

	/* the cache should only be enabled/disabled before threads are
	 * spawned, so `priv->enable_node_cache` can be accessed unlocked */
	if (!priv->enable_node_cache && !force_node_cache)
		return xb_node_new (self, sn);

	locker = g_mutex_locker_new (&priv->nodes_mutex);

	/* ensure the cache exists */
	if (priv->nodes == NULL)
		priv->nodes = g_hash_table_new_full (g_direct_hash, g_direct_equal,
						     NULL, (GDestroyNotify) g_object_unref);

	/* does already exist */
	n = g_hash_table_lookup (priv->nodes, sn);
	if (n != NULL)
		return g_object_ref (n);

	/* create and add */
	n = xb_node_new (self, sn);
	g_hash_table_insert (priv->nodes, sn, g_object_ref (n));
	return n;
}

/* Push two opcodes onto the stack with appropriate rollback on failure. */
static gboolean
_xb_stack_push_two (XbStack *opcodes, XbOpcode **op1, XbOpcode **op2, GError **error)
{
	if (!xb_stack_push (opcodes, op1, error))
		return FALSE;
	if (!xb_stack_push (opcodes, op2, error)) {
		xb_stack_pop (opcodes, NULL, NULL);
		return FALSE;
	}
	return TRUE;
}

/* convert [2] to position()=2 */
static gboolean
xb_silo_machine_fixup_position_cb (XbMachine *self,
				   XbStack *opcodes,
				   gpointer user_data,
				   GError **error)
{
	XbOpcode *op1;
	XbOpcode *op2;

	if (!_xb_stack_push_two (opcodes, &op1, &op2, error))
		return FALSE;

	xb_machine_opcode_func_init (self, op1, "position");
	xb_machine_opcode_func_init (self, op2, "eq");

	return TRUE;
}

/* convert "'type' attr()" -> "'type' attr() '(null)' ne()" */
static gboolean
xb_silo_machine_fixup_attr_exists_cb (XbMachine *self,
				      XbStack *opcodes,
				      gpointer user_data,
				      GError **error)
{
	XbOpcode *op1;
	XbOpcode *op2;

	if (!_xb_stack_push_two (opcodes, &op1, &op2, error))
		return FALSE;

	xb_opcode_text_init_static (op1, NULL);
	xb_machine_opcode_func_init (self, op2, "ne");

	return TRUE;
}

static gboolean
xb_silo_machine_func_attr_cb (XbMachine *self,
			      XbStack *stack,
			      gboolean *result,
			      gpointer user_data,
			      gpointer exec_data,
			      GError **error)
{
	XbOpcode *op2;
	XbSiloAttr *a;
	XbSilo *silo = XB_SILO (user_data);
	XbSiloQueryData *query_data = (XbSiloQueryData *) exec_data;
	g_auto(XbOpcode) op = XB_OPCODE_INIT ();

	/* optimize pass */
	if (query_data == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED,
				     "cannot optimize: no silo to query");
		return FALSE;
	}

	if (!xb_machine_stack_pop (self, stack, &op, error))
		return FALSE;

	/* indexed string */
	if (xb_opcode_get_kind (&op) == XB_OPCODE_KIND_INDEXED_TEXT) {
		guint32 val = xb_opcode_get_val (&op);
		a = xb_silo_node_get_attr_by_val (silo, query_data->sn, val);
	} else {
		const gchar *str = xb_opcode_get_str (&op);
		a = xb_silo_node_get_attr_by_str (silo, query_data->sn, str);
	}
	if (a == NULL) {
		return xb_machine_stack_push_text_static (self, stack, NULL, error);
	}
	if (!xb_machine_stack_push (self, stack, &op2, error))
		return FALSE;
	xb_opcode_init (op2, XB_OPCODE_KIND_INDEXED_TEXT,
			xb_silo_from_strtab (silo, a->attr_value),
			a->attr_value,
			NULL);
	return TRUE;
}

static gboolean
xb_silo_machine_func_stem_cb (XbMachine *self,
			      XbStack *stack,
			      gboolean *result,
			      gpointer user_data,
			      gpointer exec_data,
			      GError **error)
{
	XbSilo *silo = XB_SILO (user_data);
	XbOpcode *head;
	const gchar *str;
	g_auto(XbOpcode) op = XB_OPCODE_INIT ();

	head = xb_stack_peek_head (stack);
	if (head == NULL || !xb_opcode_cmp_str (head)) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "%s type not supported",
			     (head != NULL) ? xb_opcode_kind_to_string (xb_opcode_get_kind (head)) : "(null)");
		return FALSE;
	}

	if (!xb_machine_stack_pop (self, stack, &op, error))
		return FALSE;

	/* TEXT */
	str = xb_opcode_get_str (&op);
	return xb_machine_stack_push_text_steal (self, stack, xb_silo_stem (silo, str), error);
}

static gboolean
xb_silo_machine_func_text_cb (XbMachine *self,
			      XbStack *stack,
			      gboolean *result,
			      gpointer user_data,
			      gpointer exec_data,
			      GError **error)
{
	XbSilo *silo = XB_SILO (user_data);
	XbSiloQueryData *query_data = (XbSiloQueryData *) exec_data;
	XbOpcode *op;

	/* optimize pass */
	if (query_data == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED,
				     "cannot optimize: no silo to query");
		return FALSE;
	}

	if (!xb_machine_stack_push (self, stack, &op, error))
		return FALSE;
	xb_opcode_init (op, XB_OPCODE_KIND_INDEXED_TEXT,
			xb_silo_node_get_text (silo, query_data->sn),
			query_data->sn->text,
			NULL);
	return TRUE;
}

static gboolean
xb_silo_machine_func_tail_cb (XbMachine *self,
			      XbStack *stack,
			      gboolean *result,
			      gpointer user_data,
			      gpointer exec_data,
			      GError **error)
{
	XbSilo *silo = XB_SILO (user_data);
	XbSiloQueryData *query_data = (XbSiloQueryData *) exec_data;
	XbOpcode *op;

	/* optimize pass */
	if (query_data == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED,
				     "cannot optimize: no silo to query");
		return FALSE;
	}

	if (!xb_machine_stack_push (self, stack, &op, error))
		return FALSE;
	xb_opcode_init (op, XB_OPCODE_KIND_INDEXED_TEXT,
			xb_silo_node_get_tail (silo, query_data->sn),
			query_data->sn->tail,
			NULL);
	return TRUE;
}

static gboolean
xb_silo_machine_func_first_cb (XbMachine *self,
			       XbStack *stack,
			       gboolean *result,
			       gpointer user_data,
			       gpointer exec_data,
			       GError **error)
{
	XbSiloQueryData *query_data = (XbSiloQueryData *) exec_data;

	/* optimize pass */
	if (query_data == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED,
				     "cannot optimize: no silo to query");
		return FALSE;
	}
	return xb_stack_push_bool (stack, query_data->position == 1, error);
}

static gboolean
xb_silo_machine_func_last_cb (XbMachine *self,
			      XbStack *stack,
			      gboolean *result,
			      gpointer user_data,
			      gpointer exec_data,
			      GError **error)
{
	XbSiloQueryData *query_data = (XbSiloQueryData *) exec_data;

	/* optimize pass */
	if (query_data == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED,
				     "cannot optimize: no silo to query");
		return FALSE;
	}
	return xb_stack_push_bool (stack, query_data->sn->next == 0, error);
}

static gboolean
xb_silo_machine_func_position_cb (XbMachine *self,
				  XbStack *stack,
				  gboolean *result,
				  gpointer user_data,
				  gpointer exec_data,
				  GError **error)
{
	XbSiloQueryData *query_data = (XbSiloQueryData *) exec_data;

	/* optimize pass */
	if (query_data == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED,
				     "cannot optimize: no silo to query");
		return FALSE;
	}
	return xb_machine_stack_push_integer (self, stack, query_data->position, error);
}

static gboolean
xb_silo_machine_func_search_cb (XbMachine *self,
				XbStack *stack,
				gboolean *result,
				gpointer user_data,
				gpointer exec_data,
				GError **error)
{
	XbOpcode *head1 = NULL;
	XbOpcode *head2 = NULL;
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT ();

	if (xb_stack_get_size (stack) >= 2) {
		head1 = xb_stack_peek (stack, xb_stack_get_size (stack) - 1);
		head2 = xb_stack_peek (stack, xb_stack_get_size (stack) - 2);
	}
	if (head1 == NULL || !xb_opcode_cmp_str (head1) ||
	    head2 == NULL || !xb_opcode_cmp_str (head2)) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "%s:%s types not supported",
			     (head1 != NULL) ? xb_opcode_kind_to_string (xb_opcode_get_kind (head1)) : "(null)",
			     (head2 != NULL) ? xb_opcode_kind_to_string (xb_opcode_get_kind (head2)) : "(null)");
		return FALSE;
	}

	if (!xb_machine_stack_pop_two (self, stack, &op1, &op2, error))
		return FALSE;

	/* TEXT:TEXT */
	return xb_stack_push_bool (stack, xb_string_search (xb_opcode_get_str (&op2),
							    xb_opcode_get_str (&op1)), error);
}

static gboolean
xb_silo_machine_fixup_attr_text_cb (XbMachine *self,
				    XbStack *opcodes,
				    const gchar *text,
				    gboolean *handled,
				    gpointer user_data,
				    GError **error)
{
	/* @foo -> attr(foo) */
	if (g_str_has_prefix (text, "@")) {
		XbOpcode *op1;
		XbOpcode *op2;

		if (!_xb_stack_push_two (opcodes, &op1, &op2, error))
			return FALSE;

		xb_opcode_text_init (op1, text + 1);
		if (!xb_machine_opcode_func_init (self, op2, "attr")) {
			g_set_error_literal (error,
					     G_IO_ERROR,
					     G_IO_ERROR_NOT_SUPPORTED,
					     "no attr opcode");
			xb_stack_pop (opcodes, NULL, NULL);
			xb_stack_pop (opcodes, NULL, NULL);
			return FALSE;
		}

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
	case PROP_ENABLE_NODE_CACHE:
		g_value_set_boolean (value, priv->enable_node_cache);
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
	case PROP_ENABLE_NODE_CACHE:
		xb_silo_set_enable_node_cache (self, g_value_get_boolean (value));
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
	priv->file_monitors = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal,
						     g_object_unref, (GDestroyNotify) xb_silo_file_monitor_item_free);
	priv->strtab_tags = g_hash_table_new (g_str_hash, g_str_equal);
	priv->strindex = g_hash_table_new (g_str_hash, g_str_equal);
	priv->profile_str = g_string_new (NULL);

	priv->nodes = NULL;  /* initialised when first used */
	g_mutex_init (&priv->nodes_mutex);

#ifdef HAVE_LIBSTEMMER
	g_mutex_init (&priv->stemmer_mutex);
#endif

	priv->machine = xb_machine_new ();
	xb_machine_add_method (priv->machine, "attr", 1,
			       xb_silo_machine_func_attr_cb, self, NULL);
	xb_machine_add_method (priv->machine, "stem", 1,
			       xb_silo_machine_func_stem_cb, self, NULL);
	xb_machine_add_method (priv->machine, "text", 0,
			       xb_silo_machine_func_text_cb, self, NULL);
	xb_machine_add_method (priv->machine, "tail", 0,
			       xb_silo_machine_func_tail_cb, self, NULL);
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

	g_clear_pointer (&priv->nodes, g_hash_table_unref);
	g_mutex_clear (&priv->nodes_mutex);

#ifdef HAVE_LIBSTEMMER
	if (priv->stemmer_ctx != NULL)
		sb_stemmer_delete (priv->stemmer_ctx);
	g_mutex_clear (&priv->stemmer_mutex);
#endif

	g_free (priv->guid);
	g_string_free (priv->profile_str, TRUE);
	g_object_unref (priv->machine);
	g_hash_table_unref (priv->strindex);
	g_hash_table_unref (priv->file_monitors);
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
				     G_PARAM_READWRITE |
				     G_PARAM_CONSTRUCT |
				     G_PARAM_STATIC_NAME);
	g_object_class_install_property (object_class, PROP_GUID, pspec);

	/**
	 * XbSilo:allow-cancel:
	 */
	pspec = g_param_spec_boolean ("valid", NULL, NULL, TRUE,
				      G_PARAM_READABLE |
				      G_PARAM_STATIC_NAME);
	g_object_class_install_property (object_class, PROP_VALID, pspec);

	/**
	 * XbSilo:enable-node-cache:
	 *
	 * Whether to cache all #XbNode instances ever constructed in a single
	 * cache in the #XbSilo, so that the same #XbNode instance is always
	 * returned in query results for a given XPath. This is a form of
	 * memoisation, and allows xb_node_get_data() and xb_node_set_data() to
	 * be used.
	 *
	 * This is enabled by default to preserve compatibility with older
	 * versions of libxmlb, but most clients will want to disable it. It
	 * adds a large memory overhead (no #XbNode is ever finalised) but
	 * achieves moderately low hit rates for typical XML parsing workloads
	 * where most nodes are accessed only once or twice as they are
	 * processed and then processing moves on to other nodes.
	 *
	 * This property can only be changed before the #XbSilo is passed
	 * between threads. Changing it is not thread-safe.
	 *
	 * Since: 0.2.0
	 */
	pspec = g_param_spec_boolean ("enable-node-cache", NULL, NULL, TRUE,
				      G_PARAM_READWRITE |
				      G_PARAM_STATIC_NAME);
	g_object_class_install_property (object_class, PROP_ENABLE_NODE_CACHE, pspec);
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
