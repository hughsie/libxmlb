/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbMachine"

#include "config.h"

#include <gio/gio.h>

#include "xb-opcode.h"

struct _XbOpcode {
	gint		 ref;
	XbOpcodeKind	 kind;
	guint32		 val;
	gpointer	 ptr;
	gboolean	 freeptr;
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
	return XB_OPCODE_KIND_UNKNOWN;
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
	return (self->kind & 0x01) > 0;
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
	return (self->kind & 0x02) > 0;
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
	if (self->freeptr)
		g_free (self->ptr);
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
 * Creates a new text literal opcode.
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
	self->freeptr = TRUE;
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
	self->freeptr = TRUE;
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
