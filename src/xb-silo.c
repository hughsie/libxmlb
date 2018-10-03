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
#include "xb-common.h"
#include "xb-node-private.h"
#include "xb-opcode.h"
#include "xb-silo-private.h"

struct _XbSilo
{
	GObject			 parent_instance;
	GMappedFile		*mmap;
	gchar			*guid;
	GBytes			*blob;
	const guint8		*data;	/* pointers into ->blob */
	guint32			 datasz;
	guint32			 strtab;
	GHashTable		*strtab_tags;
	GHashTable		*nodes;
	GMutex			 nodes_mutex;
	XbMachine		*machine;
	XbSiloCurrent		 current;
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
inline XbSiloNode *
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
	if (self->blob == NULL)
		return NULL;
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

/* private */
guint32
xb_silo_get_strtab_idx (XbSilo *self, const gchar *element)
{
	gpointer value = NULL;
	if (!g_hash_table_lookup_extended (self->strtab_tags, element, NULL, &value))
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
	XbSiloHeader *hdr = (XbSiloHeader *) self->data;
	g_autoptr(GString) str = g_string_new (NULL);

	g_return_val_if_fail (XB_IS_SILO (self), NULL);

	g_string_append_printf (str, "magic:        %08x\n", (guint) hdr->magic);
	g_string_append_printf (str, "guid:         %s\n", self->guid);
	g_string_append_printf (str, "strtab:       @%" G_GUINT32_FORMAT "\n", hdr->strtab);
	g_string_append_printf (str, "strtab_ntags: %" G_GUINT16_FORMAT "\n", hdr->strtab_ntags);
	while (off < self->strtab) {
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
	for (off = 0; off < self->datasz - hdr->strtab;) {
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
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	return self->guid;
}

/* private */
XbMachine *
xb_silo_get_machine (XbSilo *self)
{
	g_return_val_if_fail (XB_IS_SILO (self), NULL);
	return self->machine;
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
	gsize sz = 0;
	gchar guid[UUID_STR_LEN] = { '\0' };
	guint32 off = 0;
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&self->nodes_mutex);

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (blob != NULL, FALSE);

	/* no longer valid */
	g_hash_table_remove_all (self->nodes);
	g_hash_table_remove_all (self->strtab_tags);
	g_clear_pointer (&self->guid, g_free);

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

	/* get GUID */
	uuid_unparse (hdr->guid, guid);
	self->guid = g_strdup (guid);

	/* check strtab */
	self->strtab = hdr->strtab;
	if (self->strtab > self->datasz) {
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
		g_hash_table_insert (self->strtab_tags,
				     (gpointer) tmp,
				     GUINT_TO_POINTER (off));
		off += strlen (tmp) + 1;
	}

	/* success */
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
	g_autofree gchar *fn = NULL;
	g_autoptr(GBytes) blob = NULL;

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	fn = g_file_get_path (file);
	self->mmap = g_mapped_file_new (fn, FALSE, error);
	if (self->mmap == NULL)
		return FALSE;
	blob = g_mapped_file_get_bytes (self->mmap);
	return xb_silo_load_from_bytes (self, blob, flags, error);
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
	g_autoptr(GFile) file_parent = NULL;

	g_return_val_if_fail (XB_IS_SILO (self), FALSE);
	g_return_val_if_fail (G_IS_FILE (file), FALSE);

	/* invalid */
	if (self->data == NULL) {
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
	return g_file_replace_contents (file,
					(const gchar *) self->data,
					(gsize) self->datasz, NULL, FALSE,
					G_FILE_CREATE_NONE, NULL,
					cancellable, error);
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
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new (&self->nodes_mutex);

	/* does already exist */
	n = g_hash_table_lookup (self->nodes, sn);
	if (n != NULL)
		return g_object_ref (n);

	/* create and add */
	n = xb_node_new (self, sn);
	g_hash_table_insert (self->nodes, sn, g_object_ref (n));
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
			      GError **error)
{
	XbSilo *silo = XB_SILO (user_data);
	XbSiloCurrent *current = xb_silo_get_current (silo);
	g_autoptr(XbOpcode) op = xb_machine_stack_pop (self, stack);
	const gchar *tmp = xb_opcode_get_str (op);
	xb_machine_stack_push_text_static (self, stack,
					   xb_silo_node_get_attr (silo, current->sn, tmp));
	return TRUE;
}

static gboolean
xb_silo_machine_func_text_cb (XbMachine *self,
			      GPtrArray *stack,
			      gboolean *result,
			      gpointer user_data,
			      GError **error)
{
	XbSilo *silo = XB_SILO (user_data);
	XbSiloCurrent *current = xb_silo_get_current (silo);
	xb_machine_stack_push_text_static (self, stack,
					   xb_silo_node_get_text (silo, current->sn));
	return TRUE;
}

static gboolean
xb_silo_machine_func_first_cb (XbMachine *self,
			       GPtrArray *stack,
			       gboolean *result,
			       gpointer user_data,
			       GError **error)
{
	XbSilo *silo = XB_SILO (user_data);
	XbSiloCurrent *current = xb_silo_get_current (silo);
	*result = *current->position == 1;
	return TRUE;
}

static gboolean
xb_silo_machine_func_last_cb (XbMachine *self,
			      GPtrArray *stack,
			      gboolean *result,
			      gpointer user_data,
			      GError **error)
{
	XbSilo *silo = XB_SILO (user_data);
	XbSiloCurrent *current = xb_silo_get_current (silo);
	*result = current->sn->next == 0;
	return TRUE;
}

static gboolean
xb_silo_machine_func_position_cb (XbMachine *self,
				  GPtrArray *stack,
				  gboolean *result,
				  gpointer user_data,
				  GError **error)
{
	XbSilo *silo = XB_SILO (user_data);
	XbSiloCurrent *current = xb_silo_get_current (silo);
	xb_machine_stack_push_integer (self, stack, *current->position);
	return TRUE;
}

static gboolean
xb_silo_machine_func_contains_cb (XbMachine *self,
				  GPtrArray *stack,
				  gboolean *result,
				  gpointer user_data,
				  GError **error)
{
	g_autoptr(XbOpcode) op1 = xb_machine_stack_pop (self, stack);
	g_autoptr(XbOpcode) op2 = xb_machine_stack_pop (self, stack);

	/* TEXT:TEXT */
	if (xb_opcode_get_kind (op1) == XB_OPCODE_KIND_TEXT &&
	    xb_opcode_get_kind (op2) == XB_OPCODE_KIND_TEXT) {
		*result = xb_string_contains_fuzzy (xb_opcode_get_str (op2),
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

XbSiloCurrent *
xb_silo_get_current (XbSilo *self)
{
	return &self->current;
}

static void
xb_silo_init (XbSilo *self)
{
	self->nodes = g_hash_table_new_full (g_direct_hash, g_direct_equal,
					     NULL, (GDestroyNotify) g_object_unref);
	self->strtab_tags = g_hash_table_new (g_str_hash, g_str_equal);

	g_mutex_init (&self->nodes_mutex);

	self->machine = xb_machine_new ();
	xb_machine_add_func (self->machine, "attr", 1,
			     xb_silo_machine_func_attr_cb, self, NULL);
	xb_machine_add_func (self->machine, "text", 0,
			     xb_silo_machine_func_text_cb, self, NULL);
	xb_machine_add_func (self->machine, "first", 0,
			     xb_silo_machine_func_first_cb, self, NULL);
	xb_machine_add_func (self->machine, "last", 0,
			     xb_silo_machine_func_last_cb, self, NULL);
	xb_machine_add_func (self->machine, "position", 0,
			     xb_silo_machine_func_position_cb, self, NULL);
	xb_machine_add_func (self->machine, "contains", 2,
			     xb_silo_machine_func_contains_cb, self, NULL);
	xb_machine_add_opcode_fixup (self->machine, "INTE",
				     xb_silo_machine_fixup_position_cb, self, NULL);
	xb_machine_add_opcode_fixup (self->machine, "TEXT,FUNC:attr",
				     xb_silo_machine_fixup_attr_exists_cb, self, NULL);
	xb_machine_add_text_handler (self->machine,
				     xb_silo_machine_fixup_attr_text_cb, self, NULL);
}

static void
xb_silo_finalize (GObject *obj)
{
	XbSilo *self = XB_SILO (obj);

	g_mutex_clear (&self->nodes_mutex);

	g_free (self->guid);
	g_object_unref (self->machine);
	g_hash_table_unref (self->nodes);
	g_hash_table_unref (self->strtab_tags);
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
