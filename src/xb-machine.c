/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbMachine"

#include "config.h"

#include <gio/gio.h>
#include <string.h>

#if !GLIB_CHECK_VERSION(2,54,0)
#include <errno.h>
#endif

#include "xb-machine.h"
#include "xb-opcode.h"
#include "xb-string-private.h"

typedef struct {
	GObject			 parent_instance;
	XbMachineDebugFlags	 debug_flags;
	GPtrArray		*methods;	/* of XbMachineMethodItem */
	GPtrArray		*operators;	/* of XbMachineOperator */
	GPtrArray		*text_handlers;	/* of XbMachineTextHandlerItem */
	GHashTable		*opcode_fixup;	/* of str[XbMachineOpcodeFixupItem] */
} XbMachinePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (XbMachine, xb_machine, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (xb_machine_get_instance_private (o))

typedef struct {
	gchar			*str;
	gsize			 strsz;
	gchar			*name;
} XbMachineOperator;

typedef struct {
	XbMachineOpcodeFixupFunc fixup_cb;
	gpointer		 user_data;
	GDestroyNotify		 user_data_free;
} XbMachineOpcodeFixupItem;

typedef struct {
	XbMachineTextHandlerFunc handler_cb;
	gpointer		 user_data;
	GDestroyNotify		 user_data_free;
} XbMachineTextHandlerItem;

typedef struct {
	guint32			 idx;
	gchar			*name;
	guint			 n_opcodes;
	XbMachineMethodFunc	 method_cb;
	gpointer		 user_data;
	GDestroyNotify		 user_data_free;
} XbMachineMethodItem;

/**
 * xb_machine_set_debug_flags:
 * @self: a #XbMachine
 * @flags: #XbMachineDebugFlags, e.g. %XB_MACHINE_DEBUG_FLAG_SHOW_STACK
 *
 * Sets the debug level of the virtual machine.
 *
 * Since: 0.1.1
 **/
void
xb_machine_set_debug_flags (XbMachine *self, XbMachineDebugFlags flags)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_MACHINE (self));
	priv->debug_flags = flags;
}

/**
 * xb_machine_add_operator:
 * @self: a #XbMachine
 * @str: operator string, e.g. `==`
 * @name: function name, e.g. `contains`
 *
 * Adds a new operator to the virtual machine. Operators can then be used
 * instead of explicit methods like `eq()`.
 *
 * You need to add a custom operator using xb_machine_add_operator() before
 * using xb_machine_parse(). Common operators like `<=` and `=` are built-in
 * and do not have to be added manually.
 *
 * Since: 0.1.1
 **/
void
xb_machine_add_operator (XbMachine *self, const gchar *str, const gchar *name)
{
	XbMachineOperator *op;
	XbMachinePrivate *priv = GET_PRIVATE (self);

	g_return_if_fail (XB_IS_MACHINE (self));
	g_return_if_fail (str != NULL);
	g_return_if_fail (name != NULL);

	op = g_slice_new0 (XbMachineOperator);
	op->str = g_strdup (str);
	op->strsz = strlen (str);
	op->name = g_strdup (name);
	g_ptr_array_add (priv->operators, op);
}

/**
 * xb_machine_add_method:
 * @self: a #XbMachine
 * @name: function name, e.g. `contains`
 * @n_opcodes: minimum number of opcodes requried on the stack
 * @method_cb: function to call
 * @user_data: user pointer to pass to @method_cb, or %NULL
 * @user_data_free: a function which gets called to free @user_data, or %NULL
 *
 * Adds a new function to the virtual machine. Registered functions can then be
 * used as methods.
 *
 * You need to add a custom function using xb_machine_add_method() before using
 * methods that may reference it, for example xb_machine_add_opcode_fixup().
 *
 * Since: 0.1.1
 **/
void
xb_machine_add_method (XbMachine *self,
		       const gchar *name,
		       guint n_opcodes,
		       XbMachineMethodFunc method_cb,
		       gpointer user_data,
		       GDestroyNotify user_data_free)
{
	XbMachineMethodItem *item;
	XbMachinePrivate *priv = GET_PRIVATE (self);

	g_return_if_fail (XB_IS_MACHINE (self));
	g_return_if_fail (name != NULL);
	g_return_if_fail (method_cb != NULL);

	item = g_slice_new0 (XbMachineMethodItem);
	item->idx = priv->methods->len;
	item->name = g_strdup (name);
	item->n_opcodes = n_opcodes;
	item->method_cb = method_cb;
	item->user_data = user_data;
	item->user_data_free = user_data_free;
	g_ptr_array_add (priv->methods, item);
}

/**
 * xb_machine_add_opcode_fixup:
 * @self: a #XbMachine
 * @opcodes_sig: signature, e.g. `INTE,TEXT`
 * @fixup_cb: callback
 * @user_data: user pointer to pass to @fixup_cb
 * @user_data_free: a function which gets called to free @user_data, or %NULL
 *
 * Adds an opcode fixup. Fixups can be used to optimize the stack of opcodes or
 * to add support for a nonstandard feature, for instance supporting missing
 * attributes to functions.
 *
 * Since: 0.1.1
 **/
void
xb_machine_add_opcode_fixup (XbMachine *self,
			     const gchar *opcodes_sig,
			     XbMachineOpcodeFixupFunc fixup_cb,
			     gpointer user_data,
			     GDestroyNotify user_data_free)
{
	XbMachineOpcodeFixupItem *item = g_slice_new0 (XbMachineOpcodeFixupItem);
	XbMachinePrivate *priv = GET_PRIVATE (self);
	item->fixup_cb = fixup_cb;
	item->user_data = user_data;
	item->user_data_free = user_data_free;
	g_hash_table_insert (priv->opcode_fixup, g_strdup (opcodes_sig), item);
}

/**
 * xb_machine_add_text_handler:
 * @self: a #XbMachine
 * @handler_cb: callback
 * @user_data: user pointer to pass to @handler_cb
 * @user_data_free: a function which gets called to free @user_data, or %NULL
 *
 * Adds a text handler. This allows the virtual machine to support nonstandard
 * encoding or shorthand mnemonics for standard functions.
 *
 * Since: 0.1.1
 **/
void
xb_machine_add_text_handler (XbMachine *self,
			     XbMachineTextHandlerFunc handler_cb,
			     gpointer user_data,
			     GDestroyNotify user_data_free)
{
	XbMachineTextHandlerItem *item = g_slice_new0 (XbMachineTextHandlerItem);
	XbMachinePrivate *priv = GET_PRIVATE (self);
	item->handler_cb = handler_cb;
	item->user_data = user_data;
	item->user_data_free = user_data_free;
	g_ptr_array_add (priv->text_handlers, item);
}

static XbMachineMethodItem *
xb_machine_find_func (XbMachine *self, const gchar *func_name)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	for (guint i = 0; i < priv->methods->len; i++) {
		XbMachineMethodItem *item = g_ptr_array_index (priv->methods, i);
		if (g_strcmp0 (item->name, func_name) == 0)
			return item;
	}
	return NULL;
}

/**
 * xb_machine_opcode_func_new:
 * @self: a #XbMachine
 * @func_name: function name, e.g. `eq`
 *
 * Creates a new opcode for a registered function. Some standard opcodes are
 * registered by default, for instance `eq` or `ge`. Other opcodes have to be
 * added using xb_machine_add_method().
 *
 * Returns: a new #XbOpcode, or %NULL
 *
 * Since: 0.1.1
 **/
XbOpcode *
xb_machine_opcode_func_new (XbMachine *self, const gchar *func_name)
{
	XbMachineMethodItem *item = xb_machine_find_func (self, func_name);
	if (item == NULL) {
		g_critical ("failed to find %s", func_name);
		return NULL;
	}
	return xb_opcode_func_new (item->idx);
}

static gboolean
xb_machine_parse_add_func (XbMachine *self,
			   GPtrArray *opcodes,
			   const gchar *func_name,
			   GError **error)
{
	XbMachineMethodItem *item;

	/* find function by name */
	item = xb_machine_find_func (self, func_name);
	if (item == NULL) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "function %s() is not supported",
			     func_name);
		return FALSE;
	}

	/* create new opcode */
	g_ptr_array_add (opcodes, xb_opcode_func_new (item->idx));
	return TRUE;
}

#if !GLIB_CHECK_VERSION(2,54,0)
static gboolean
str_has_sign (const gchar *str)
{
	return str[0] == '-' || str[0] == '+';
}

static gboolean
str_has_hex_prefix (const gchar *str)
{
	return str[0] == '0' && g_ascii_tolower (str[1]) == 'x';
}

static gboolean
g_ascii_string_to_unsigned (const gchar *str,
			    guint base,
			    guint64 min,
			    guint64 max,
			    guint64 *out_num,
			    GError **error)
{
	const gchar *end_ptr = NULL;
	gint saved_errno = 0;
	guint64 number;

	g_return_val_if_fail (str != NULL, FALSE);
	g_return_val_if_fail (base >= 2 && base <= 36, FALSE);
	g_return_val_if_fail (min <= max, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (str[0] == '\0') {
		g_set_error_literal (error,
				     G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
				     "Empty string is not a number");
		return FALSE;
	}

	errno = 0;
	number = g_ascii_strtoull (str, (gchar **)&end_ptr, base);
	saved_errno = errno;

	if (g_ascii_isspace (str[0]) || str_has_sign (str) ||
	    (base == 16 && str_has_hex_prefix (str)) ||
	    (saved_errno != 0 && saved_errno != ERANGE) ||
	    end_ptr == NULL ||
	    *end_ptr != '\0') {
		g_set_error (error,
			     G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			     "“%s” is not an unsigned number", str);
		return FALSE;
	}
	if (saved_errno == ERANGE || number < min || number > max) {
		g_autofree gchar *min_str = g_strdup_printf ("%" G_GUINT64_FORMAT, min);
		g_autofree gchar *max_str = g_strdup_printf ("%" G_GUINT64_FORMAT, max);
		g_set_error (error,
			     G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			     "Number “%s” is out of bounds [%s, %s]",
			     str, min_str, max_str);
		return FALSE;
	}
	if (out_num != NULL)
		*out_num = number;
	return TRUE;
}
#endif

static gboolean
xb_machine_parse_add_text_raw (XbMachine *self,
			       GPtrArray *opcodes,
			       const gchar *str,
			       GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	guint64 val;
	gsize text_len;

	/* NULL is perfectly valid */
	if (str == NULL) {
		g_ptr_array_add (opcodes, xb_opcode_text_new_static (str));
		return TRUE;
	}

	/* never add empty literals */
	text_len = strlen (str);
	if (text_len == 0)
		return TRUE;

	/* do any additional handlers */
	for (guint i = 0; i < priv->text_handlers->len; i++) {
		XbMachineTextHandlerItem *item = g_ptr_array_index (priv->text_handlers, i);
		gboolean handled = FALSE;
		if (!item->handler_cb (self, opcodes, str, &handled, item->user_data, error))
			return FALSE;
		if (handled)
			return TRUE;
	}

	/* quoted text */
	if (text_len >= 2) {
		if (str[0] == '\'' && str[text_len - 1] == '\'') {
			gchar *tmp = g_strndup (str + 1, text_len - 2);
			g_ptr_array_add (opcodes, xb_opcode_text_new_steal (tmp));
			return TRUE;
		}
	}

	/* check for plain integer */
	if (g_ascii_string_to_unsigned (str, 10, 0, G_MAXUINT32, &val, NULL)) {
		g_ptr_array_add (opcodes, xb_opcode_integer_new (val));
		return TRUE;
	}

	/* not supported */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "cannot parse text or number `%s`", str);
	return FALSE;
}

static gboolean
xb_machine_parse_add_text (XbMachine *self,
			   GPtrArray *opcodes,
			   const gchar *str,
			   GError **error)
{
	return xb_machine_parse_add_text_raw (self, opcodes, str, error);
}

static gssize
xb_machine_parse_section (XbMachine *self,
			  GPtrArray *opcodes,
			  const gchar *text,
			  gsize start,
			  gssize text_len,
			  guint level,
			  GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	g_autoptr(GString) acc = g_string_new (NULL);

	/* sanity check */
	if (level > 20) {
		g_autofree gchar *tmp = g_strndup (text, text_len);
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "nesting deeper than 20 levels supported: %s",
			     tmp);
		return G_MAXSSIZE;
	}

	/* build accumulator until hitting either bracket, then recurse */
	for (gssize i = start; i < text_len; i++) {
		if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_PARSING)
			g_debug ("@%u, >%c", level, text[i]);
		if (text[i] == ',')
			continue;
		if (text[i] == '(') {
			i = xb_machine_parse_section (self, opcodes, text, i + 1, text_len, level + 1, error);
			if (i == 0) {
				g_set_error_literal (error,
						     G_IO_ERROR,
						     G_IO_ERROR_INVALID_DATA,
						     "failed to find matching bracket");
				return G_MAXSSIZE;
			}
			if (i == G_MAXSSIZE)
				return G_MAXSSIZE;
			if (!xb_machine_parse_add_func (self, opcodes, acc->str, error))
				return G_MAXSSIZE;
			g_string_truncate (acc, 0);
			continue;
		}
		if (text[i] == ')') {
			if (acc->len > 0) {
				if (!xb_machine_parse_add_text (self, opcodes, acc->str, error))
					return G_MAXSSIZE;
			}
			g_string_truncate (acc, 0);
			return i;
		}

		g_string_append_c (acc, text[i]);
	}

	/* any left over */
	if (acc->len > 0) {
		if (!xb_machine_parse_add_text (self, opcodes, acc->str, error))
			return G_MAXSSIZE;
		g_string_truncate (acc, 0);
	}

	return 0;
}

static gchar *
xb_machine_get_opcodes_sig (XbMachine *self, GPtrArray *opcodes)
{
	GString *str = g_string_new (NULL);
	XbMachinePrivate *priv = GET_PRIVATE (self);
	for (guint i = 0; i < opcodes->len; i++) {
		XbOpcode *opcode = g_ptr_array_index (opcodes, i);
		g_assert (opcode != NULL);
		if (xb_opcode_get_kind (opcode) == XB_OPCODE_KIND_FUNCTION) {
			XbMachineMethodItem *item;
			item = g_ptr_array_index (priv->methods, xb_opcode_get_val (opcode));
			if (item == NULL) {
				g_string_append (str, "FUNC:???,");
			} else {
				g_string_append_printf (str, "FUNC:%s,", item->name);
			}
			continue;
		}
		if (xb_opcode_get_kind (opcode) == XB_OPCODE_KIND_TEXT) {
			g_string_append (str, "TEXT,");
			continue;
		}
		if (xb_opcode_get_kind (opcode) == XB_OPCODE_KIND_INTEGER) {
			g_string_append (str, "INTE,");
			continue;
		}
		g_critical ("unknown type");
	}
	if (str->len > 0)
		g_string_truncate (str, str->len - 1);
	return g_string_free (str, FALSE);
}

static gboolean
xb_machine_parse_part (XbMachine *self,
		       GPtrArray *opcodes,
		       const gchar *text,
		       gsize start,
		       gssize text_len,
		       GError **error)
{
	gsize rc = xb_machine_parse_section (self, opcodes, text, start, text_len, 0, error);
	if (rc == G_MAXSSIZE) {
		g_prefix_error (error, "failed to parse part: ");
		return FALSE;
	}
	return TRUE;
}

/**
 * xb_machine_parse:
 * @self: a #XbMachine
 * @text: predicate to parse, e.g. `contains(text(),'xyx')`
 * @text_len: length of @text, or -1 if @text is `NUL` terminated
 * @error: a #GError, or %NULL
 *
 * Parses an XPath predicate. Not all of XPath 1.0 or XPath 1.0 is supported,
 * and new functions and mnemonics can be added using xb_machine_add_method()
 * and xb_machine_add_text_handler().
 *
 * Returns: (transfer container) (element-type XbOpcode): opcodes, or %NULL on error
 *
 * Since: 0.1.1
 **/
GPtrArray *
xb_machine_parse (XbMachine *self,
		  const gchar *text,
		  gssize text_len,
		  GError **error)
{
	XbMachineOpcodeFixupItem *item;
	XbMachinePrivate *priv = GET_PRIVATE (self);
	g_autoptr(GPtrArray) opcodes = NULL;
	g_autofree gchar *opcodes_sig = NULL;

	g_return_val_if_fail (XB_IS_MACHINE (self), NULL);
	g_return_val_if_fail (text != NULL, NULL);

	/* assume NUL terminated */
	if (text_len < 0)
		text_len = strlen (text);
	if (text_len == 0) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "string was zero size");
		return NULL;
	}

	/* look for foo=bar */
	opcodes = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_opcode_unref);
	for (gssize i = 0; i < text_len && opcodes->len == 0; i++) {
		for (guint j = 0; j < priv->operators->len; j++) {
			XbMachineOperator *op = g_ptr_array_index (priv->operators, j);
			if (strncmp (text + i, op->str, op->strsz) == 0) {
				XbOpcode *opcode = NULL;

				/* match opcode, which should always exist */
				opcode = xb_machine_opcode_func_new (self, op->name);
				if (opcode == NULL) {
					g_set_error (error,
						     G_IO_ERROR,
						     G_IO_ERROR_NOT_SUPPORTED,
						     "built-in function not found: %s",
						     op->name);
					return NULL;
				}
				if (!xb_machine_parse_part (self, opcodes, text,
							    0, /* start */
							    i, /* end */
							    error))
					return NULL;
				if (!xb_machine_parse_part (self, opcodes, text,
							    i + op->strsz,
							    text_len,
							    error))
					return NULL;
				g_ptr_array_add (opcodes, opcode);
				break;
			}
		}
	}

	/* remainder */
	if (opcodes->len == 0) {
		if (!xb_machine_parse_part (self, opcodes, text, 0, text_len, error))
			return NULL;
	}

	/* do any fixups */
	opcodes_sig = xb_machine_get_opcodes_sig (self, opcodes);
	item = g_hash_table_lookup (priv->opcode_fixup, opcodes_sig);
	if (item != NULL) {
		if (!item->fixup_cb (self, opcodes, item->user_data, error))
			return NULL;
	}

	/* success */
	return g_steal_pointer (&opcodes);
}

static void
xb_machine_debug_show_stack (XbMachine *self, GPtrArray *stack)
{
	g_autofree gchar *str = NULL;
	if (stack->len == 0) {
		g_debug ("stack is empty");
		return;
	}
	str = xb_machine_opcodes_to_string (self, stack);
	g_debug ("stack: %s", str);
}

static gboolean
xb_machine_run_func (XbMachine *self,
		     GPtrArray *stack,
		     XbOpcode *opcode,
		     gboolean *result,
		     gpointer exec_data,
		     GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	XbMachineMethodItem *item = g_ptr_array_index (priv->methods, xb_opcode_get_val (opcode));

	/* optional debugging */
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK) {
		g_autofree gchar *str = xb_machine_opcode_to_string (self, opcode);
		g_debug ("running: %s", str);
		xb_machine_debug_show_stack (self, stack);
	}

	/* check we have enough stack elements */
	if (item->n_opcodes > stack->len) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "function required %u arguments, stack only has %u",
			     item->n_opcodes, stack->len);
		return FALSE;
	}
	if (!item->method_cb (self, stack, result, item->user_data, exec_data, error)) {
		g_prefix_error (error, "failed to call %s(): ", item->name);
		return FALSE;
	}
	return TRUE;
}

/**
 * xb_machine_opcode_to_string:
 * @self: a #XbMachine
 * @opcode: a #XbOpcode
 *
 * Returns a string representing the specific opcode.
 *
 * Returns: text
 *
 * Since: 0.1.1
 **/
gchar *
xb_machine_opcode_to_string (XbMachine *self, XbOpcode *opcode)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);

	g_return_val_if_fail (XB_IS_MACHINE (self), NULL);
	g_return_val_if_fail (opcode != NULL, NULL);

	if (xb_opcode_get_kind (opcode) == XB_OPCODE_KIND_FUNCTION) {
		XbMachineMethodItem *item;
		item = g_ptr_array_index (priv->methods, xb_opcode_get_val (opcode));
		return g_strdup_printf ("%s()", item->name);
	}
	if (xb_opcode_get_kind (opcode) == XB_OPCODE_KIND_TEXT)
		return g_strdup_printf ("'%s'", xb_opcode_get_str (opcode));
	if (xb_opcode_get_kind (opcode) == XB_OPCODE_KIND_INTEGER)
		return g_strdup_printf ("%u", xb_opcode_get_val (opcode));
	g_critical ("no to_string for kind %u", xb_opcode_get_kind (opcode));
	return NULL;
}

/**
 * xb_machine_opcodes_to_string:
 * @self: a #XbMachine
 * @opcodes: (element-type XbOpcode): opcodes
 *
 * Returns a string representing a set of opcodes.
 *
 * Returns: text
 *
 * Since: 0.1.1
 **/
gchar *
xb_machine_opcodes_to_string (XbMachine *self, GPtrArray *opcodes)
{
	GString *str = g_string_new (NULL);

	g_return_val_if_fail (XB_IS_MACHINE (self), NULL);
	g_return_val_if_fail (opcodes != NULL, NULL);

	for (guint i = 0; i < opcodes->len; i++) {
		XbOpcode *opcode = g_ptr_array_index (opcodes, i);
		g_autofree gchar *tmp = xb_machine_opcode_to_string (self, opcode);
		g_string_append_printf (str, "%s,", tmp);
	}
	if (str->len > 0)
		g_string_truncate (str, str->len - 1);
	return g_string_free (str, FALSE);
}

/**
 * xb_machine_run:
 * @self: a #XbMachine
 * @opcodes: (element-type XbOpcode): opcodes
 * @result: (out): return status after running @opcodes
 * @exec_data: per-run user data that is passed to all the XbMachineMethodFunc functions
 * @error: a #GError, or %NULL
 *
 * Runs a set of opcodes on the virtual machine.
 *
 * It is safe to call this function from a different thread to the one that
 * created the #XbMachine.
 *
 * Returns: a new #XbOpcode, or %NULL
 *
 * Since: 0.1.1
 **/
gboolean
xb_machine_run (XbMachine *self,
		GPtrArray *opcodes,
		gboolean *result,
		gpointer exec_data,
		GError **error)
{
	g_autoptr(GPtrArray) stack = NULL;

	g_return_val_if_fail (XB_IS_MACHINE (self), FALSE);
	g_return_val_if_fail (opcodes != NULL, FALSE);
	g_return_val_if_fail (result != NULL, FALSE);

	/* process each opcode */
	stack = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_opcode_unref);
	for (guint i = 0; i < opcodes->len; i++) {
		XbOpcode *opcode = g_ptr_array_index (opcodes, i);
		XbOpcodeKind kind = xb_opcode_get_kind (opcode);

		/* add to stack */
		if (kind == XB_OPCODE_KIND_TEXT ||
		    kind == XB_OPCODE_KIND_INTEGER) {
			xb_machine_stack_push (self, stack, opcode);
			continue;
		}

		/* process the stack */
		if (kind == XB_OPCODE_KIND_FUNCTION) {
			if (!xb_machine_run_func (self,
						  stack,
						  opcode,
						  result,
						  exec_data,
						  error))
				return FALSE;
			if (*result == FALSE)
				return TRUE;
			continue;
		}

		/* invalid */
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "opcode kind %u not recognised",
			     kind);
		return FALSE;
	}

	/* the stack should have been completely consumed */
	if (stack->len > 0) {
		g_autofree gchar *tmp = xb_machine_opcodes_to_string (self, stack);
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "%u opcodes remain on the stack (%s)",
			     stack->len, tmp);
		return FALSE;
	}

	/* success */
	return TRUE;
}

/**
 * xb_machine_stack_pop:
 * @self: a #XbMachine
 * @stack: (element-type XbOpcode): the working stack
 *
 * Pops an opcode from the stack.
 *
 * Returns: (transfer full): a new #XbOpcode, or %NULL
 *
 * Since: 0.1.1
 **/
XbOpcode *
xb_machine_stack_pop (XbMachine *self, GPtrArray *stack)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	XbOpcode *opcode;
	if (stack->len == 0)
		return NULL;
	opcode = g_ptr_array_index (stack, stack->len - 1);
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK) {
		g_autofree gchar *str = xb_machine_opcode_to_string (self, opcode);
		g_debug ("popping: %s", str);
	}
	xb_opcode_ref (opcode);
	g_ptr_array_remove_index (stack, stack->len - 1);
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		xb_machine_debug_show_stack (self, stack);
	return opcode;
}

/**
 * xb_machine_stack_push:
 * @self: a #XbMachine
 * @stack: (element-type XbOpcode): the working stack
 * @opcode: a #XbOpcode
 *
 * Adds an opcode to the stack.
 *
 * Since: 0.1.1
 **/
void
xb_machine_stack_push (XbMachine *self, GPtrArray *stack, XbOpcode *opcode)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK) {
		g_autofree gchar *str = xb_machine_opcode_to_string (self, opcode);
		g_debug ("pushing: %s", str);
	}
	g_ptr_array_add (stack, xb_opcode_ref (opcode));
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		xb_machine_debug_show_stack (self, stack);
}

/**
 * xb_machine_stack_push_text:
 * @self: a #XbMachine
 * @stack: (element-type XbOpcode): the working stack
 * @str: text literal
 *
 * Adds a text literal to the stack, copying @str.
 *
 * Since: 0.1.1
 **/
void
xb_machine_stack_push_text (XbMachine *self, GPtrArray *stack, const gchar *str)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		g_debug ("pushing: %s", str);
	g_ptr_array_add (stack, xb_opcode_text_new (str));
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		xb_machine_debug_show_stack (self, stack);
}

/**
 * xb_machine_stack_push_text_static:
 * @self: a #XbMachine
 * @stack: (element-type XbOpcode): the working stack
 * @str: text literal
 *
 * Adds static text literal to the stack.
 *
 * Since: 0.1.1
 **/
void
xb_machine_stack_push_text_static (XbMachine *self, GPtrArray *stack, const gchar *str)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		g_debug ("pushing: %s", str);
	g_ptr_array_add (stack, xb_opcode_text_new_static (str));
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		xb_machine_debug_show_stack (self, stack);
}

/**
 * xb_machine_stack_push_text_steal:
 * @self: a #XbMachine
 * @stack: (element-type XbOpcode): the working stack
 * @str: text literal
 *
 * Adds a stolen text literal to the stack.
 *
 * Since: 0.1.1
 **/
void
xb_machine_stack_push_text_steal (XbMachine *self, GPtrArray *stack, gchar *str)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		g_debug ("pushing: %s", str);
	g_ptr_array_add (stack, xb_opcode_text_new_steal (str));
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		xb_machine_debug_show_stack (self, stack);
}

/**
 * xb_machine_stack_push_integer:
 * @self: a #XbMachine
 * @stack: (element-type XbOpcode): the working stack
 * @val: interger literal
 *
 * Adds an integer literal to the stack.
 *
 * Since: 0.1.1
 **/
void
xb_machine_stack_push_integer (XbMachine *self, GPtrArray *stack, guint32 val)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		g_debug ("pushing: %u", val);
	g_ptr_array_add (stack, xb_opcode_integer_new (val));
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		xb_machine_debug_show_stack (self, stack);
}

static gboolean
xb_machine_func_eq_cb (XbMachine *self,
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
		*result = g_strcmp0 (xb_opcode_get_str (op1),
				     xb_opcode_get_str (op2)) == 0;
		return TRUE;
	}

	/* INTE:INTE */
	if (xb_opcode_cmp_val (op1) && xb_opcode_cmp_val (op2)) {
		*result = xb_opcode_get_val (op1) == xb_opcode_get_val (op2);
		return TRUE;
	}

	/* INTE:TEXT */
	if (xb_opcode_cmp_val (op1) && xb_opcode_cmp_str (op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (op2) == NULL) {
			*result = FALSE;
			return TRUE;
		}
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (op2),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		*result = val == xb_opcode_get_val (op1);
		return TRUE;
	}

	/* INTE:TEXT */
	if (xb_opcode_cmp_val (op2) && xb_opcode_cmp_str (op1)) {
		guint64 val = 0;
		if (xb_opcode_get_str (op1) == NULL) {
			*result = FALSE;
			return TRUE;
		}
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (op1),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		*result = val == xb_opcode_get_val (op2);
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
xb_machine_func_ne_cb (XbMachine *self,
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
		*result = g_strcmp0 (xb_opcode_get_str (op1),
				     xb_opcode_get_str (op2)) != 0;
		return TRUE;
	}

	/* INTE:INTE */
	if (xb_opcode_cmp_val (op1) && xb_opcode_cmp_val (op2)) {
		*result = xb_opcode_get_val (op1) != xb_opcode_get_val (op2);
		return TRUE;
	}

	/* INTE:TEXT */
	if (xb_opcode_cmp_val (op1) && xb_opcode_cmp_str (op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (op2) == NULL) {
			*result = FALSE;
			return TRUE;
		}
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (op2),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		*result = val != xb_opcode_get_val (op1);
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
xb_machine_func_lt_cb (XbMachine *self,
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
		*result = g_strcmp0 (xb_opcode_get_str (op2),
				     xb_opcode_get_str (op1)) < 0;
		return TRUE;
	}
	/* INTE:INTE */
	if (xb_opcode_cmp_val (op1) && xb_opcode_cmp_val (op2)) {
		*result = xb_opcode_get_val (op2) < xb_opcode_get_val (op1);
		return TRUE;
	}

	/* INTE:TEXT */
	if (xb_opcode_cmp_val (op1) && xb_opcode_cmp_str (op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (op2) == NULL) {
			*result = FALSE;
			return TRUE;
		}
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (op2),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		*result = val < xb_opcode_get_val (op1);
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
xb_machine_func_gt_cb (XbMachine *self,
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
		*result = g_strcmp0 (xb_opcode_get_str (op2),
				     xb_opcode_get_str (op1)) > 0;
		return TRUE;
	}

	/* INTE:INTE */
	if (xb_opcode_cmp_val (op1) && xb_opcode_cmp_val (op2)) {
		*result = xb_opcode_get_val (op2) > xb_opcode_get_val (op1);
		return TRUE;
	}

	/* INTE:TEXT */
	if (xb_opcode_cmp_val (op1) && xb_opcode_cmp_str (op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (op2) == NULL) {
			*result = FALSE;
			return TRUE;
		}
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (op2),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		*result = val > xb_opcode_get_val (op1);
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
xb_machine_func_le_cb (XbMachine *self,
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
		*result = g_strcmp0 (xb_opcode_get_str (op2),
				     xb_opcode_get_str (op1)) <= 0;
		return TRUE;
	}

	/* INTE:INTE */
	if (xb_opcode_cmp_val (op1) && xb_opcode_cmp_val (op2)) {
		*result = xb_opcode_get_val (op2) <= xb_opcode_get_val (op1);
		return TRUE;
	}

	/* INTE:TEXT */
	if (xb_opcode_cmp_val (op1) && xb_opcode_cmp_str (op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (op2) == NULL) {
			*result = FALSE;
			return TRUE;
		}
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (op2),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		*result = val <= xb_opcode_get_val (op1);
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
xb_machine_func_lower_cb (XbMachine *self,
			  GPtrArray *stack,
			  gboolean *result,
			  gpointer user_data,
			  gpointer exec_data,
			  GError **error)
{
	g_autoptr(XbOpcode) op = xb_machine_stack_pop (self, stack);
	xb_machine_stack_push_text_steal (self, stack,
					  g_ascii_strdown (xb_opcode_get_str (op), -1));
	return TRUE;
}

static gboolean
xb_machine_func_upper_cb (XbMachine *self,
			  GPtrArray *stack,
			  gboolean *result,
			  gpointer user_data,
			  gpointer exec_data,
			  GError **error)
{
	g_autoptr(XbOpcode) op = xb_machine_stack_pop (self, stack);
	xb_machine_stack_push_text_steal (self, stack,
					  g_ascii_strup (xb_opcode_get_str (op), -1));
	return TRUE;
}

static gboolean
xb_machine_func_not_cb (XbMachine *self,
			GPtrArray *stack,
			gboolean *result,
			gpointer user_data,
			gpointer exec_data,
			GError **error)
{
	g_autoptr(XbOpcode) op1 = xb_machine_stack_pop (self, stack);

	/* TEXT */
	if (xb_opcode_cmp_str (op1)) {
		*result = xb_opcode_get_str (op1) == NULL;
		return TRUE;
	}

	/* INTE */
	if (xb_opcode_cmp_val (op1)) {
		*result = xb_opcode_get_val (op1) == 0;
		return TRUE;
	}

	/* fail */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "%s type not supported",
		     xb_opcode_kind_to_string (xb_opcode_get_kind (op1)));
	return FALSE;
}

static gboolean
xb_machine_func_ge_cb (XbMachine *self,
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
		*result = g_strcmp0 (xb_opcode_get_str (op2),
				     xb_opcode_get_str (op1)) >= 0;
		return TRUE;
	}

	/* INTE:INTE */
	if (xb_opcode_cmp_val (op1) && xb_opcode_cmp_val (op2)) {
		*result = xb_opcode_get_val (op2) >= xb_opcode_get_val (op1);
		return TRUE;
	}

	/* INTE:TEXT */
	if (xb_opcode_cmp_val (op1) && xb_opcode_cmp_str (op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (op2) == NULL) {
			*result = FALSE;
			return TRUE;
		}
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (op2),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		*result = val >= xb_opcode_get_val (op1);
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
xb_machine_func_contains_cb (XbMachine *self,
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
		*result = xb_string_contains (xb_opcode_get_str (op2),
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
xb_machine_func_starts_with_cb (XbMachine *self,
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
		*result = g_str_has_prefix (xb_opcode_get_str (op2),
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
xb_machine_func_ends_with_cb (XbMachine *self,
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
		*result = g_str_has_suffix (xb_opcode_get_str (op2),
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
xb_machine_func_number_cb (XbMachine *self,
			   GPtrArray *stack,
			   gboolean *result,
			   gpointer user_data,
			   gpointer exec_data,
			   GError **error)
{
	g_autoptr(XbOpcode) op1 = xb_machine_stack_pop (self, stack);

	/* TEXT */
	if (xb_opcode_cmp_str (op1)) {
		guint64 val = 0;
		if (xb_opcode_get_str (op1) == NULL) {
			*result = FALSE;
			return TRUE;
		}
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (op1),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		xb_machine_stack_push_integer (self, stack, val);
		return TRUE;
	}

	/* fail */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "%s types not supported",
		     xb_opcode_kind_to_string (xb_opcode_get_kind (op1)));
	return FALSE;
}

static gboolean
xb_machine_func_strlen_cb (XbMachine *self,
			   GPtrArray *stack,
			   gboolean *result,
			   gpointer user_data,
			   gpointer exec_data,
			   GError **error)
{
	g_autoptr(XbOpcode) op1 = xb_machine_stack_pop (self, stack);

	/* TEXT */
	if (xb_opcode_cmp_str (op1)) {
		if (xb_opcode_get_str (op1) == NULL) {
			*result = FALSE;
			return TRUE;
		}
		xb_machine_stack_push_integer (self, stack, strlen (xb_opcode_get_str (op1)));
		return TRUE;
	}

	/* fail */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "%s types not supported",
		     xb_opcode_kind_to_string (xb_opcode_get_kind (op1)));
	return FALSE;
}

static gboolean
xb_machine_func_string_cb (XbMachine *self,
			   GPtrArray *stack,
			   gboolean *result,
			   gpointer user_data,
			   gpointer exec_data,
			   GError **error)
{
	g_autoptr(XbOpcode) op1 = xb_machine_stack_pop (self, stack);

	/* INTE */
	if (xb_opcode_cmp_val (op1)) {
		gchar *tmp = g_strdup_printf ("%" G_GUINT32_FORMAT,
					      xb_opcode_get_val (op1));
		xb_machine_stack_push_text_steal (self, stack, tmp);
		return TRUE;
	}

	/* fail */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "%s types not supported",
		     xb_opcode_kind_to_string (xb_opcode_get_kind (op1)));
	return FALSE;
}

static void
xb_machine_opcode_fixup_free (XbMachineOpcodeFixupItem *item)
{
	if (item->user_data_free != NULL)
		item->user_data_free (item->user_data);
	g_slice_free (XbMachineOpcodeFixupItem, item);
}

static void
xb_machine_func_free (XbMachineMethodItem *item)
{
	if (item->user_data_free != NULL)
		item->user_data_free (item->user_data);
	g_free (item->name);
	g_slice_free (XbMachineMethodItem, item);
}

static void
xb_machine_text_handler_free (XbMachineTextHandlerItem *item)
{
	if (item->user_data_free != NULL)
		item->user_data_free (item->user_data);
	g_slice_free (XbMachineTextHandlerItem, item);
}

static void
xb_machine_operator_free (XbMachineOperator *op)
{
	g_free (op->str);
	g_free (op->name);
	g_slice_free (XbMachineOperator, op);
}

static void
xb_machine_init (XbMachine *self)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	priv->methods = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_machine_func_free);
	priv->operators = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_machine_operator_free);
	priv->text_handlers = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_machine_text_handler_free);
	priv->opcode_fixup = g_hash_table_new_full (g_str_hash, g_str_equal,
						     g_free, (GDestroyNotify) xb_machine_opcode_fixup_free);

	/* build-in functions */
	xb_machine_add_method (self, "eq", 2, xb_machine_func_eq_cb, NULL, NULL);
	xb_machine_add_method (self, "ne", 2, xb_machine_func_ne_cb, NULL, NULL);
	xb_machine_add_method (self, "lt", 2, xb_machine_func_lt_cb, NULL, NULL);
	xb_machine_add_method (self, "gt", 2, xb_machine_func_gt_cb, NULL, NULL);
	xb_machine_add_method (self, "le", 2, xb_machine_func_le_cb, NULL, NULL);
	xb_machine_add_method (self, "ge", 2, xb_machine_func_ge_cb, NULL, NULL);
	xb_machine_add_method (self, "not", 1, xb_machine_func_not_cb, NULL, NULL);
	xb_machine_add_method (self, "lower-case", 1, xb_machine_func_lower_cb, NULL, NULL);
	xb_machine_add_method (self, "upper-case", 1, xb_machine_func_upper_cb, NULL, NULL);
	xb_machine_add_method (self, "contains", 2, xb_machine_func_contains_cb, NULL, NULL);
	xb_machine_add_method (self, "starts-with", 2, xb_machine_func_starts_with_cb, NULL, NULL);
	xb_machine_add_method (self, "ends-with", 2, xb_machine_func_ends_with_cb, NULL, NULL);
	xb_machine_add_method (self, "string", 1, xb_machine_func_string_cb, NULL, NULL);
	xb_machine_add_method (self, "number", 1, xb_machine_func_number_cb, NULL, NULL);
	xb_machine_add_method (self, "string-length", 1, xb_machine_func_strlen_cb, NULL, NULL);

	/* built-in operators */
	xb_machine_add_operator (self, "!=", "ne");
	xb_machine_add_operator (self, "<=", "le");
	xb_machine_add_operator (self, ">=", "ge");
	xb_machine_add_operator (self, "==", "eq");
	xb_machine_add_operator (self, "=", "eq");
	xb_machine_add_operator (self, ">", "gt");
	xb_machine_add_operator (self, "<", "lt");
}

static void
xb_machine_finalize (GObject *obj)
{
	XbMachine *self = XB_MACHINE (obj);
	XbMachinePrivate *priv = GET_PRIVATE (self);
	g_ptr_array_unref (priv->methods);
	g_ptr_array_unref (priv->operators);
	g_ptr_array_unref (priv->text_handlers);
	g_hash_table_unref (priv->opcode_fixup);
	G_OBJECT_CLASS (xb_machine_parent_class)->finalize (obj);
}

static void
xb_machine_class_init (XbMachineClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = xb_machine_finalize;
}

/**
 * xb_machine_new:
 *
 * Creates a new virtual machine.
 *
 * Returns: a new #XbMachine
 *
 * Since: 0.1.1
 **/
XbMachine *
xb_machine_new (void)
{
	return g_object_new (XB_TYPE_MACHINE, NULL);
}
