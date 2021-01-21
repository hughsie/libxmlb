/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbMachine"

#include "config.h"

#include <gio/gio.h>

#include "xb-opcode-private.h"

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
	if (kind & XB_OPCODE_FLAG_FUNCTION)
		return "FUNC";
	if (kind & XB_OPCODE_FLAG_TEXT)
		return "TEXT";
	if (kind & XB_OPCODE_FLAG_INTEGER)
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

static gchar *
xb_opcode_to_string_internal (XbOpcode *self)
{
	/* special cases */
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

	/* bitwise fallbacks */
	if (self->kind & XB_OPCODE_KIND_FUNCTION)
		return g_strdup_printf ("%s()", xb_opcode_get_str_for_display (self));
	if (self->kind & XB_OPCODE_KIND_TEXT)
		return g_strdup_printf ("'%s'", xb_opcode_get_str_for_display (self));
	g_critical ("no to_string for kind 0x%x", self->kind);
	return NULL;
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
	g_autofree gchar *tmp = xb_opcode_to_string_internal (self);
	if (self->kind & XB_OPCODE_FLAG_TOKENIZED) {
		g_autofree gchar *tokens = g_strjoinv (",", (gchar **) self->tokens);
		return g_strdup_printf ("%s[%s]", tmp, tokens);
	}
	return g_steal_pointer (&tmp);
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
	return self->kind & ~XB_OPCODE_FLAG_TOKENIZED;
}

/**
 * xb_opcode_has_flag:
 * @self: a #XbOpcode
 * @flag: a #XbOpcodeFlags, e.g. #XB_OPCODE_FLAG_TOKENIZED
 *
 * Finds out if an opcode has a flag set.
 *
 * Returns: %TRUE if the flag is set
 *
 * Since: 0.3.1
 **/
gboolean
xb_opcode_has_flag (XbOpcode *self, XbOpcodeFlags flag)
{
	return (self->kind & flag) > 0;
}

/**
 * xb_opcode_add_flag:
 * @self: a #XbOpcode
 * @flag: a #XbOpcodeFlags, e.g. #XB_OPCODE_FLAG_TOKENIZED
 *
 * Adds a flag to the opcode.
 *
 * Since: 0.3.1
 **/
void
xb_opcode_add_flag (XbOpcode *self, XbOpcodeFlags flag)
{
	self->kind |= flag;
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
	return (self->kind & XB_OPCODE_FLAG_TEXT) > 0;
}

/* private */
gboolean
xb_opcode_is_binding (XbOpcode *self)
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
 * xb_opcode_get_tokens:
 * @self: a #XbOpcode
 *
 * Gets the tokenized string stored on the opcode.
 *
 * Returns: a #GStrv, or %NULL if unset
 *
 * Since: 0.3.1
 **/
const gchar **
xb_opcode_get_tokens (XbOpcode *self)
{
	return self->tokens;
}

/**
 * xb_opcode_clear:
 * @self: a #XbOpcode
 *
 * Clears any allocated data inside the opcode, but does not free the #XbOpcode
 * itself. This is suitable for calling on stack-allocated #XbOpcodes.
 *
 * Since: 0.2.0
 */
void
xb_opcode_clear (XbOpcode *self)
{
	if (self->destroy_func)
		self->destroy_func (self->ptr);
	self->destroy_func = NULL;
}

/**
 * xb_opcode_text_init:
 * @opcode: a stack allocated #XbOpcode to initialise
 * @str: a string
 *
 * Initialises a stack allocated #XbOpcode to contain a text literal.
 * The @str argument is copied internally and is not tied to the lifecycle of
 * the #XbOpcode.
 *
 * Since: 0.2.0
 **/
void
xb_opcode_text_init (XbOpcode *opcode, const gchar *str)
{
	xb_opcode_init (opcode, XB_OPCODE_KIND_TEXT, g_strdup (str), 0, g_free);
}

/**
 * xb_opcode_init:
 * @opcode: allocated opcode to fill
 * @kind: a #XbOpcodeKind, e.g. %XB_OPCODE_KIND_INTEGER
 * @str: a string
 * @val: a integer value
 * @destroy_func: (nullable): a #GDestroyNotify, e.g. g_free()
 *
 * Initialises a stack allocated #XbOpcode.
 *
 * Since: 0.2.0
 **/
void
xb_opcode_init (XbOpcode       *opcode,
                XbOpcodeKind    kind,
                const gchar    *str,
                guint32         val,
                GDestroyNotify  destroy_func)
{
	opcode->kind = kind;
	opcode->ptr = (gpointer) str;
	opcode->val = val;
	opcode->destroy_func = destroy_func;
}

/**
 * xb_opcode_text_init_static:
 * @opcode: a stack allocated #XbOpcode to initialise
 * @str: a string
 *
 * Initialises a stack allocated #XbOpcode to contain a text literal, where
 * @str is either static text or will outlive the #XbOpcode lifecycle.
 *
 * Since: 0.2.0
 **/
void
xb_opcode_text_init_static (XbOpcode *opcode, const gchar *str)
{
	xb_opcode_init (opcode, XB_OPCODE_KIND_TEXT, str, 0, NULL);
}

/**
 * xb_opcode_text_init_steal:
 * @opcode: a stack allocated #XbOpcode to initialise
 * @str: a string
 *
 * Initialises a stack allocated #XbOpcode to contain a text literal, stealing
 * the @str. Once the opcode is finalized g_free() will be called on @str.
 *
 * Since: 0.2.0
 **/
void
xb_opcode_text_init_steal (XbOpcode *opcode, gchar *str)
{
	xb_opcode_init (opcode, XB_OPCODE_KIND_TEXT, g_steal_pointer (&str), 0, g_free);
}

/**
 * xb_opcode_func_init:
 * @opcode: a stack allocated #XbOpcode to initialise
 * @func: a function index
 *
 * Initialises a stack allocated #XbOpcode to contain a specific function.
 * Custom functions can be registered using xb_machine_add_func() and retrieved
 * using xb_machine_opcode_func_new().
 *
 * Since: 0.2.0
 **/
void
xb_opcode_func_init (XbOpcode *opcode, guint32 func)
{
	xb_opcode_init (opcode, XB_OPCODE_KIND_FUNCTION, NULL, func, NULL);
}

/**
 * xb_opcode_bind_init:
 * @opcode: a stack allocated #XbOpcode to initialise
 *
 * Initialises a stack allocated #XbOpcode to contain a bind variable. A value
 * needs to be assigned to this opcode at runtime using
 * xb_value_bindings_bind_str() or xb_value_bindings_bind_val().
 *
 * Since: 0.2.0
 **/
void
xb_opcode_bind_init (XbOpcode *opcode)
{
	xb_opcode_init (opcode, XB_OPCODE_KIND_BOUND_INTEGER, NULL, 0, NULL);
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
xb_opcode_set_token (XbOpcode *self, guint idx, const gchar *val)
{
	g_return_if_fail (val != NULL);
	g_return_if_fail (val[0] != '\0');
	g_return_if_fail (idx < XB_OPCODE_TOKEN_MAX);
	self->tokens[idx] = val;
	self->kind |= XB_OPCODE_FLAG_TOKENIZED;
}

/* private */
void
xb_opcode_set_kind (XbOpcode *self, XbOpcodeKind kind)
{
	self->kind = kind;
}

/**
 * xb_opcode_integer_init:
 * @opcode: a stack allocated #XbOpcode to initialise
 * @val: a integer value
 *
 * Initialises a stack allocated #XbOpcode to contain an integer literal.
 *
 * Since: 0.2.0
 **/
void
xb_opcode_integer_init (XbOpcode *opcode, guint32 val)
{
	xb_opcode_init (opcode, XB_OPCODE_KIND_INTEGER, NULL, val, NULL);
}

/* private */
void
xb_opcode_bool_init (XbOpcode *opcode, gboolean val)
{
	xb_opcode_init (opcode, XB_OPCODE_KIND_BOOLEAN, NULL, !!val, NULL);
}
