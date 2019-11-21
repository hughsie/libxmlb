/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbMachine"

#include "config.h"

#include <gio/gio.h>

#include "xb-opcode-private.h"

struct _XbOpcode {
	gint		 ref;
	XbOpcodeKind	 kind;
	guint32		 val;
	gpointer	 ptr;
	GDestroyNotify	 destroy_func;
};

/**
 * xb_opcode_kind_to_string:
 * @kind: a #XbOpcodeKind, e.g. %XB_OPCODE_KIND_FUNCTION
 *
 * Converts the opcode kind to a string.
 *
 * Returns: opcode kind, e.g. `FUNC`
 *
 * Since: 0.1.1
 **/
const gchar *
xb_opcode_kind_to_string (XbOpcodeKind kind)
{
	if (kind == XB_OPCODE_KIND_FUNCTION)
		return "FUNC";
	if (kind == XB_OPCODE_KIND_TEXT)
		return "TEXT";
	if (kind == XB_OPCODE_KIND_INTEGER)
		return "INTE";
	if (kind == XB_OPCODE_KIND_BOUND_UNSET)
		return "BIND";
	if (kind == XB_OPCODE_KIND_BOUND_TEXT)
		return "?TXT";
	if (kind == XB_OPCODE_KIND_BOUND_INTEGER)
		return "?INT";
	if (kind == XB_OPCODE_KIND_INDEXED_TEXT)
		return "TEXI";
	if (kind == XB_OPCODE_KIND_BOOLEAN)
		return "BOOL";
	return NULL;
}

/**
 * xb_opcode_kind_from_string:
 * @str: a string, e.g. `FUNC`
 *
 * Converts a string to an opcode kind.
 *
 * Returns: a #XbOpcodeKind, e.g. %XB_OPCODE_KIND_TEXT
 *
 * Since: 0.1.1
 **/
XbOpcodeKind
xb_opcode_kind_from_string (const gchar *str)
{
	if (g_strcmp0 (str, "FUNC") == 0)
		return XB_OPCODE_KIND_FUNCTION;
	if (g_strcmp0 (str, "TEXT") == 0)
		return XB_OPCODE_KIND_TEXT;
	if (g_strcmp0 (str, "INTE") == 0)
		return XB_OPCODE_KIND_INTEGER;
	if (g_strcmp0 (str, "BIND") == 0)
		return XB_OPCODE_KIND_BOUND_INTEGER;
	if (g_strcmp0 (str, "?TXT") == 0)
		return XB_OPCODE_KIND_BOUND_TEXT;
	if (g_strcmp0 (str, "?INT") == 0)
		return XB_OPCODE_KIND_BOUND_INTEGER;
	if (g_strcmp0 (str, "TEXI") == 0)
		return XB_OPCODE_KIND_INDEXED_TEXT;
	if (g_strcmp0 (str, "BOOL") == 0)
		return XB_OPCODE_KIND_BOOLEAN;
	return XB_OPCODE_KIND_UNKNOWN;
}

/* private */
gchar *
xb_opcode_get_sig (XbOpcode *self)
{
	GString *str = g_string_new (xb_opcode_kind_to_string (self->kind));
	if (self->kind == XB_OPCODE_KIND_FUNCTION) {
		g_string_append_printf (str, ":%s",
					self->ptr != NULL ? (gchar *) self->ptr : "???");
	}
	return g_string_free (str, FALSE);
}

static const gchar *
xb_opcode_get_str_for_display (XbOpcode *self)
{
	if (self->ptr == NULL)
		return "(null)";
	return self->ptr;
}

/**
 * xb_opcode_to_string:
 * @self: a #XbOpcode
 *
 * Returns a string representing the specific opcode.
 *
 * Returns: text
 *
 * Since: 0.1.4
 **/
gchar *
xb_opcode_to_string (XbOpcode *self)
{
	if (self->kind == XB_OPCODE_KIND_FUNCTION)
		return g_strdup_printf ("%s()", xb_opcode_get_str_for_display (self));
	if (self->kind == XB_OPCODE_KIND_TEXT)
		return g_strdup_printf ("'%s'", xb_opcode_get_str_for_display (self));
	if (self->kind == XB_OPCODE_KIND_INDEXED_TEXT)
		return g_strdup_printf ("$'%s'", xb_opcode_get_str_for_display (self));
	if (self->kind == XB_OPCODE_KIND_INTEGER)
		return g_strdup_printf ("%u", xb_opcode_get_val (self));
	if (self->kind == XB_OPCODE_KIND_BOUND_INTEGER)
		return g_strdup ("?");
	if (self->kind == XB_OPCODE_KIND_BOUND_TEXT)
		return g_strdup_printf ("?'%s'", xb_opcode_get_str_for_display (self));
	if (self->kind == XB_OPCODE_KIND_BOUND_INTEGER)
		return g_strdup_printf ("?%u", xb_opcode_get_val (self));
	if (self->kind == XB_OPCODE_KIND_BOOLEAN)
		return g_strdup (xb_opcode_get_val (self) ? "True" : "False");
	g_critical ("no to_string for kind %u", self->kind);
	return NULL;
}

/**
 * xb_opcode_get_kind:
 * @self: a #XbOpcode
 *
 * Gets the opcode kind.
 *
 * Returns: a #XbOpcodeKind, e.g. %XB_OPCODE_KIND_INTEGER
 *
 * Since: 0.1.1
 **/
XbOpcodeKind
xb_opcode_get_kind (XbOpcode *self)
{
	return self->kind;
}

/**
 * xb_opcode_cmp_val:
 * @self: a #XbOpcode
 *
 * Checks if the opcode can be compared using the integer value.
 *
 * Returns: #%TRUE if this opcode can be compared as an integer
 *
 * Since: 0.1.1
 **/
inline gboolean
xb_opcode_cmp_val (XbOpcode *self)
{
	return self->kind == XB_OPCODE_KIND_INTEGER ||
		self->kind == XB_OPCODE_KIND_BOOLEAN ||
		self->kind == XB_OPCODE_KIND_BOUND_INTEGER;
}

/**
 * xb_opcode_cmp_str:
 * @self: a #XbOpcode
 *
 * Checks if the opcode can be compared using the string value.
 *
 * Returns: #%TRUE if this opcode can be compared as an string
 *
 * Since: 0.1.1
 **/
inline gboolean
xb_opcode_cmp_str (XbOpcode *self)
{
	return self->kind == XB_OPCODE_KIND_TEXT ||
		self->kind == XB_OPCODE_KIND_BOUND_TEXT ||
		self->kind == XB_OPCODE_KIND_INDEXED_TEXT;
}

/* private */
gboolean
xb_opcode_is_bound (XbOpcode *self)
{
	return (self->kind & XB_OPCODE_FLAG_BOUND) > 0;
}

/**
 * xb_opcode_get_val:
 * @self: a #XbOpcode
 *
 * Gets the integer value stored in the opcode. This may be a function ID,
 * a index into the string table or a literal integer.
 *
 * Returns: value, or 0 for unset.
 *
 * Since: 0.1.1
 **/
guint32
xb_opcode_get_val (XbOpcode *self)
{
	return self->val;
}

/**
 * xb_opcode_get_str:
 * @self: a #XbOpcode
 *
 * Gets the string value stored on the opcode.
 *
 * Returns: a string, or %NULL if unset
 *
 * Since: 0.1.1
 **/
const gchar *
xb_opcode_get_str (XbOpcode *self)
{
	return self->ptr;
}

/**
 * xb_opcode_unref:
 * @self: a #XbOpcode
 *
 * Decrements the reference count of the opcode, freeing the object when the
 * refcount drops to zero.
 *
 * Since: 0.1.1
 **/
void
xb_opcode_unref (XbOpcode *self)
{
	g_assert (self->ref > 0);
	if (--self->ref > 0)
		return;
	if (self->destroy_func)
		self->destroy_func (self->ptr);
	g_slice_free (XbOpcode, self);
}

/**
 * xb_opcode_ref:
 * @self: a #XbOpcode
 *
 * Increments the refcount of the opcode.
 *
 * Returns: (transfer none): the original @self #XbOpcode instance
 *
 * Since: 0.1.1
 **/
XbOpcode *
xb_opcode_ref (XbOpcode *self)
{
	self->ref++;
	return self;
}

/**
 * xb_opcode_text_new:
 * @str: a string
 *
 * Creates a new text literal opcode. The @str argument is copied internally
 * and is not tied to the lifecycle of the #XbOpcode.
 *
 * Returns: (transfer full): a #XbOpcode
 *
 * Since: 0.1.1
 **/
XbOpcode *
xb_opcode_text_new (const gchar *str)
{
	XbOpcode *self = g_slice_new0 (XbOpcode);
	self->ref = 1;
	self->kind = XB_OPCODE_KIND_TEXT;
	self->ptr = g_strdup (str);
	self->destroy_func = g_free;
	return self;
}

/**
 * xb_opcode_new:
 * @kind: a #XbOpcodeKind, e.g. %XB_OPCODE_KIND_INTEGER
 * @str: a string
 * @val: a integer value
 * @destroy_func: (nullable): a #GDestroyNotify, e.g. g_free()
 *
 * Creates a new opcode.
 *
 * Returns: (transfer full): a #XbOpcode
 *
 * Since: 0.1.4
 **/
XbOpcode *
xb_opcode_new (XbOpcodeKind kind,
	       const gchar *str,
	       guint32 val,
	       GDestroyNotify destroy_func)
{
	XbOpcode *self = g_slice_new0 (XbOpcode);
	self->ref = 1;
	self->kind = kind;
	self->ptr = (gpointer) str;
	self->val = val;
	self->destroy_func = destroy_func;
	return self;
}

/**
 * xb_opcode_text_new_static:
 * @str: a string
 *
 * Creates a new text literal opcode, where @str is either static text or will
 * outlive the #XbOpcode lifecycle.
 *
 * Returns: (transfer full): a #XbOpcode
 *
 * Since: 0.1.1
 **/
XbOpcode *
xb_opcode_text_new_static (const gchar *str)
{
	XbOpcode *self = g_slice_new0 (XbOpcode);
	self->ref = 1;
	self->kind = XB_OPCODE_KIND_TEXT;
	self->ptr = (gpointer) str;
	return self;
}

/**
 * xb_opcode_text_new_steal:
 * @str: a string
 *
 * Creates a new text literal opcode, stealing the @str. Once the opcode is
 * finalized g_free() will be called on @str.
 *
 * Returns: (transfer full): a #XbOpcode
 *
 * Since: 0.1.1
 **/
XbOpcode *
xb_opcode_text_new_steal (gchar *str)
{
	XbOpcode *self = g_slice_new0 (XbOpcode);
	self->ref = 1;
	self->kind = XB_OPCODE_KIND_TEXT;
	self->ptr = (gpointer) str;
	self->destroy_func = g_free;
	return self;
}

/**
 * xb_opcode_func_new:
 * @func: a function index
 *
 * Creates an opcode for a specific function. Custom functions can be registered
 * using xb_machine_add_func() and retrieved using xb_machine_opcode_func_new().
 *
 * Returns: (transfer full): a #XbOpcode
 *
 * Since: 0.1.1
 **/
XbOpcode *
xb_opcode_func_new (guint32 func)
{
	XbOpcode *self = g_slice_new0 (XbOpcode);
	self->ref = 1;
	self->kind = XB_OPCODE_KIND_FUNCTION;
	self->val = func;
	return self;
}

/**
 * xb_opcode_bind_new:
 *
 * Creates an opcode for a bind variable. A value needs to be assigned to this
 * opcode at runtime using xb_query_bind_str().
 *
 * Returns: (transfer full): a #XbOpcode
 *
 * Since: 0.1.4
 **/
XbOpcode *
xb_opcode_bind_new (void)
{
	XbOpcode *self = g_slice_new0 (XbOpcode);
	self->ref = 1;
	self->kind = XB_OPCODE_KIND_BOUND_INTEGER;
	return self;
}

/* private */
void
xb_opcode_bind_str (XbOpcode *self, gchar *str, GDestroyNotify destroy_func)
{
	if (self->destroy_func) {
		self->destroy_func (self->ptr);
		self->destroy_func = NULL;
	}
	self->kind = XB_OPCODE_KIND_BOUND_TEXT;
	self->ptr = (gpointer) str;
	self->destroy_func = (gpointer) destroy_func;
}

/* private */
void
xb_opcode_bind_val (XbOpcode *self, guint32 val)
{
	if (self->destroy_func) {
		self->destroy_func (self->ptr);
		self->destroy_func = NULL;
	}
	self->kind = XB_OPCODE_KIND_BOUND_INTEGER;
	self->val = val;
}

/* private */
void
xb_opcode_set_val (XbOpcode *self, guint32 val)
{
	self->val = val;
}

/* private */
void
xb_opcode_set_kind (XbOpcode *self, XbOpcodeKind kind)
{
	self->kind = kind;
}

/**
 * xb_opcode_integer_new:
 * @val: a integer value
 *
 * Creates an opcode with an literal integer.
 *
 * Returns: (transfer full): a #XbOpcode
 *
 * Since: 0.1.1
 **/
XbOpcode *
xb_opcode_integer_new (guint32 val)
{
	XbOpcode *self = g_slice_new0 (XbOpcode);
	self->ref = 1;
	self->kind = XB_OPCODE_KIND_INTEGER;
	self->val = val;
	return self;
}

/* private */
XbOpcode *
xb_opcode_bool_new (gboolean val)
{
	XbOpcode *self = g_slice_new0 (XbOpcode);
	self->ref = 1;
	self->kind = XB_OPCODE_KIND_BOOLEAN;
	self->val = val;
	return self;
}

GType
xb_opcode_get_type (void)
{
	static GType type = 0;
	if (G_UNLIKELY (!type)) {
		type = g_boxed_type_register_static ("XbOpcode",
						     (GBoxedCopyFunc) xb_opcode_ref,
						     (GBoxedFreeFunc) xb_opcode_unref);
	}
	return type;
}
