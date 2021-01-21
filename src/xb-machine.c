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

#include "xb-machine-private.h"
#include "xb-opcode-private.h"
#include "xb-silo-private.h"
#include "xb-stack-private.h"
#include "xb-string-private.h"

typedef struct {
	XbMachineDebugFlags	 debug_flags;
	GPtrArray		*methods;	/* of XbMachineMethodItem */
	GPtrArray		*operators;	/* of XbMachineOperator */
	GPtrArray		*text_handlers;	/* of XbMachineTextHandlerItem */
	GHashTable		*opcode_fixup;	/* of str[XbMachineOpcodeFixupItem] */
	guint			 stack_size;
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
 * @n_opcodes: minimum number of opcodes required on the stack
 * @method_cb: function to call
 * @user_data: user pointer to pass to @method_cb, or %NULL
 * @user_data_free: a function which gets called to free @user_data, or %NULL
 *
 * Adds a new function to the virtual machine. Registered functions can then be
 * used as methods.
 *
 * The @method_cb must not modify the stack it’s passed unless it’s going to
 * succeed. In particular, if a method call is not optimisable, it must not
 * modify the stack it’s passed.
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
 * xb_machine_opcode_func_init:
 * @self: a #XbMachine
 * @opcode: (out caller-allocates): a stack allocated #XbOpcode to initialise
 * @func_name: function name, e.g. `eq`
 *
 * Initialises a stack allocated #XbOpcode for a registered function.
 * Some standard functions are registered by default, for instance `eq` or `ge`.
 * Other functions have to be added using xb_machine_add_method().
 *
 * Returns: %TRUE if the function was found and the opcode initialised, %FALSE
 *    otherwise
 * Since: 0.2.0
 **/
gboolean
xb_machine_opcode_func_init (XbMachine *self, XbOpcode *opcode, const gchar *func_name)
{
	XbMachineMethodItem *item = xb_machine_find_func (self, func_name);
	if (item == NULL)
		return FALSE;
	xb_opcode_init (opcode, XB_OPCODE_KIND_FUNCTION,
			g_strdup (func_name),
			item->idx, g_free);
	return TRUE;
}

static gboolean
xb_machine_parse_add_func (XbMachine *self,
			   XbStack *opcodes,
			   const gchar *func_name,
			   GError **error)
{
	XbOpcode *opcode;

	if (!xb_stack_push (opcodes, &opcode, error))
		return FALSE;

	/* match opcode, which should always exist */
	if (!xb_machine_opcode_func_init (self, opcode, func_name)) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "built-in function not found: %s", func_name);
		xb_stack_pop (opcodes, NULL, NULL);
		return FALSE;
	}

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
xb_machine_parse_add_text (XbMachine *self,
			   XbStack *opcodes,
			   const gchar *text,
			   gssize text_len,
			   GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	g_autofree gchar *str = NULL;
	guint64 val;

	/* NULL is perfectly valid */
	if (text == NULL) {
		XbOpcode *opcode;
		if (!xb_stack_push (opcodes, &opcode, error))
			return FALSE;
		xb_opcode_text_init_static (opcode, str);
		return TRUE;
	}

	/* never add empty literals */
	if (text_len < 0)
		text_len = strlen (text);
	if (text_len == 0)
		return TRUE;

	/* do any additional handlers */
	str = g_strndup (text, text_len);
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
			g_autofree gchar *tmp = g_strndup (str + 1, text_len - 2);
			XbOpcode *opcode;
			if (!xb_stack_push (opcodes, &opcode, error))
				return FALSE;
			xb_opcode_text_init_steal (opcode, g_steal_pointer (&tmp));
			return TRUE;
		}
	}

	/* indexed text */
	if (text_len >= 3) {
		if (str[0] == '$' && str[1] == '\'' && str[text_len - 1] == '\'') {
			gchar *tmp = g_strndup (str + 2, text_len - 3);
			XbOpcode *opcode;
			if (!xb_stack_push (opcodes, &opcode, error))
				return FALSE;
			xb_opcode_init (opcode,
					XB_OPCODE_KIND_INDEXED_TEXT,
					tmp,
					XB_SILO_UNSET,
					g_free);
			return TRUE;
		}
	}

	/* bind variables */
	if (g_strcmp0 (str, "?") == 0) {
		XbOpcode *opcode;
		if (!xb_stack_push (opcodes, &opcode, error))
			return FALSE;
		xb_opcode_bind_init (opcode);
		return TRUE;
	}

	/* check for plain integer */
	if (g_ascii_string_to_unsigned (str, 10, 0, G_MAXUINT32, &val, NULL)) {
		XbOpcode *opcode;
		if (!xb_stack_push (opcodes, &opcode, error))
			return FALSE;
		xb_opcode_integer_init (opcode, val);
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
xb_machine_parse_section (XbMachine *self,
			  XbStack *opcodes,
			  const gchar *text,
			  gssize text_len,
			  gboolean is_method,
			  GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);

	/* fall back for simplicity */
	if (text_len < 0)
		text_len = strlen (text);
	if (text_len == 0)
		return TRUE;

//	g_debug ("xb_machine_parse_section{%s} method=%i", g_strndup (text, text_len), is_method);
	for (gssize i = 0; i < text_len; i++) {
		for (guint j = 0; j < priv->operators->len; j++) {
			XbMachineOperator *op = g_ptr_array_index (priv->operators, j);
			if (strncmp (text + i, op->str, op->strsz) == 0) {
				if (is_method) {
					/* after then before */
					if (!xb_machine_parse_section (self, opcodes, text + i + op->strsz, -1, is_method, error))
						return FALSE;
					if (i > 0) {
						if (!xb_machine_parse_section (self, opcodes, text, i, FALSE, error))
							return FALSE;
					}
					if (!xb_machine_parse_add_func (self, opcodes, op->name, error))
						return FALSE;
				} else {
					/* before then after */
					if (i > 0) {
						if (!xb_machine_parse_section (self, opcodes, text, i, FALSE, error))
							return FALSE;
					}
					if (!xb_machine_parse_section (self, opcodes, text + i + op->strsz, -1, is_method, error))
						return FALSE;
					if (!xb_machine_parse_add_func (self, opcodes, op->name, error))
						return FALSE;
				}
				return TRUE;
			}
		}
	}

	/* nothing matched */
	if (is_method)
		return xb_machine_parse_add_func (self, opcodes, text, error);
	return xb_machine_parse_add_text (self, opcodes, text, text_len, error);
}

static gboolean
xb_machine_parse_sections (XbMachine *self,
			   XbStack *opcodes,
			   const gchar *text,
			   gsize text_len,
			   gboolean is_method,
			   GError **error)
{
	g_autofree gchar *tmp = NULL;
	if (text_len == 0)
		return TRUE;

	/* leading comma */
	if (text[0] == ',') {
		tmp = g_strndup (text + 1, text_len - 1);
	} else {
		tmp = g_strndup (text, text_len);
	}

//	g_debug ("xb_machine_parse_sections{%s} method=%i", tmp, is_method);
	for (gint i = text_len - 1; i >= 0; i--) {
//		g_debug ("%u\t\t%c", i, tmp[i]);
		if (tmp[i] == ',') {
			tmp[i] = '\0';
			if (is_method) {
				if (!xb_machine_parse_add_func (self,
								opcodes,
								tmp + i + 1,
								error))
					return FALSE;
				is_method = FALSE;
			} else {
				if (!xb_machine_parse_section (self,
							       opcodes,
							       tmp + i + 1,
							       -1,
							       TRUE,
							       error))
					return FALSE;
			}
		}
	}
	if (tmp[0] != '\0') {
		if (!xb_machine_parse_section (self,
					       opcodes,
					       tmp,
					       -1,
					       is_method,
					       error))
			return FALSE;
	}
	return TRUE;
}

static gchar *
xb_machine_get_opcodes_sig (XbMachine *self, XbStack *opcodes)
{
	GString *str = g_string_new (NULL);
	for (guint i = 0; i < xb_stack_get_size (opcodes); i++) {
		XbOpcode *op = xb_stack_peek (opcodes, i);
		g_autofree gchar *sig = xb_opcode_get_sig (op);
		g_string_append_printf (str, "%s,", sig);
	}
	if (str->len > 0)
		g_string_truncate (str, str->len - 1);
	return g_string_free (str, FALSE);
}

/* @results *must* have enough space
 * @op is transfer full into this function */
static gboolean
xb_machine_opcodes_optimize_fn (XbMachine *self,
				XbStack *opcodes,
				XbOpcode op,
				XbStack *results,
				GError **error)
{
	XbMachineMethodItem *item;
	XbMachinePrivate *priv = GET_PRIVATE (self);
	g_autofree gchar *stack_str = NULL;
	g_autoptr(GError) error_local = NULL;
	g_auto(XbOpcode) op_result = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op_owned = op;

	/* a function! lets check the arg length */
	if (xb_opcode_get_kind (&op) != XB_OPCODE_KIND_FUNCTION) {
		XbOpcode *op_out;
		if (!xb_stack_push (results, &op_out, error))
			return FALSE;
		*op_out = xb_opcode_steal (&op_owned);
		return TRUE;
	}

	/* get function, check if we have enough arguments */
	item = g_ptr_array_index (priv->methods, xb_opcode_get_val (&op));
	if (item->n_opcodes > xb_stack_get_size (opcodes)) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "predicate invalid -- not enough args");
		return FALSE;
	}

	/* run the method. it's only supposed to pop its arguments off the stack
	 * if it can complete successfully */
	stack_str = xb_stack_to_string (opcodes);
	if (!item->method_cb (self, opcodes, NULL, item->user_data, NULL, &error_local)) {
		XbOpcode *op_out;

		if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_OPTIMIZER) {
			g_debug ("ignoring optimized call to %s(%s): %s",
				 item->name,
				 stack_str,
				 error_local->message);
		}
		if (!xb_stack_push (results, &op_out, error))
			return FALSE;
		*op_out = xb_opcode_steal (&op_owned);
		return TRUE;
	}

	/* the method ran, add the result. the arguments have already been popped */
	if (!xb_machine_stack_pop (self, opcodes, &op_result, error))
		return FALSE;
	if (xb_opcode_get_kind (&op_result) != XB_OPCODE_KIND_BOOLEAN ||
	    xb_opcode_get_val (&op_result)) {
		XbOpcode *op_out;

		if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_OPTIMIZER) {
			g_autofree gchar *tmp = xb_opcode_to_string (&op_result);
			g_debug ("method ran, adding result %s", tmp);
		}
		if (!xb_stack_push (results, &op_out, error))
			return FALSE;
		*op_out = xb_opcode_steal (&op_result);

		return TRUE;
	}

	/* the predicate will always evalulate to FALSE */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_INVALID_DATA,
		     "the predicate will always evalulate to FALSE: %s",
		     stack_str);
	return FALSE;
}

static gboolean
xb_machine_opcodes_optimize (XbMachine *self, XbStack *opcodes, GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	g_autoptr(XbStack) results = xb_stack_new (xb_stack_get_size (opcodes));
	g_auto(XbOpcode) op = XB_OPCODE_INIT ();

	/* debug */
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK) {
		g_autofree gchar *str = xb_stack_to_string (opcodes);
		g_debug ("before optimizing: %s", str);
	}

	/* process the stack in reverse order */
	while (xb_machine_stack_pop (self, opcodes, &op, NULL)) {
		/* this takes ownership of @op */
		if (!xb_machine_opcodes_optimize_fn (self,
						     opcodes,
						     xb_opcode_steal (&op),
						     results,
						     error))
			return FALSE;
	}

	/* copy back the result into the opcodes stack (and reverse it) */
	while (xb_stack_pop (results, &op, NULL)) {
		XbOpcode *op_out;
		if (!xb_stack_push (opcodes, &op_out, error))
			return FALSE;
		*op_out = xb_opcode_steal (&op);
	}

	/* debug */
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK) {
		g_autofree gchar *str = xb_stack_to_string (opcodes);
		g_debug ("after optimizing: %s", str);
	}
	return TRUE;
}

static gsize
xb_machine_parse_text (XbMachine *self,
		       XbStack *opcodes,
		       const gchar *text,
		       gsize text_len,
		       guint level,
		       GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	guint tail = 0;

	/* sanity check */
	if (level > 20) {
		g_autofree gchar *tmp = g_strndup (text, text_len);
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "nesting deeper than 20 levels supported: %s", tmp);
		return G_MAXSIZE;
	}

	//g_debug ("%u xb_machine_parse_text{%s}", level, g_strndup (text, text_len));
	for (guint i = 0; i < text_len; i++) {
		if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_PARSING)
			g_debug ("LVL %u\t%u:\t\t%c", level, i, text[i]);
		if (text[i] == '(') {
			gsize j = 0;
			j = xb_machine_parse_text (self,
						   opcodes,
						   text + i + 1,
						   text_len - i,
						   level + 1,
						   error);
			if (j == G_MAXSIZE)
				return G_MAXSIZE;
			if (!xb_machine_parse_sections (self,
							opcodes,
							text + tail,
							i - tail,
							TRUE,
							error))
				return G_MAXSIZE;
			i += j;
			tail = i + 1;
			continue;
		}
		if (text[i] == ')') {
			if (!xb_machine_parse_sections (self,
							opcodes,
							text + tail,
							i - tail,
							FALSE,
							error))
				return G_MAXSIZE;
			return i + 1;
		}
	}
	if (tail != text_len && level > 0) {
		g_autofree gchar *tmp = g_strndup (text, text_len);
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "brackets did not match: %s", tmp);
		return G_MAXSIZE;
	}
	if (!xb_machine_parse_sections (self,
					opcodes,
					text + tail,
					text_len - tail,
					FALSE,
					error))
		return G_MAXSIZE;
	return 0;
}


/**
 * xb_machine_parse_full:
 * @self: a #XbMachine
 * @text: predicate to parse, e.g. `contains(text(),'xyx')`
 * @text_len: length of @text, or -1 if @text is `NUL` terminated
 * @flags: #XbMachineParseFlags, e.g. %XB_MACHINE_PARSE_FLAG_OPTIMIZE
 * @error: a #GError, or %NULL
 *
 * Parses an XPath predicate. Not all of XPath 1.0 or XPath 1.0 is supported,
 * and new functions and mnemonics can be added using xb_machine_add_method()
 * and xb_machine_add_text_handler().
 *
 * Returns: (transfer full): opcodes, or %NULL on error
 *
 * Since: 0.1.4
 **/
XbStack *
xb_machine_parse_full (XbMachine *self,
		       const gchar *text,
		       gssize text_len,
		       XbMachineParseFlags flags,
		       GError **error)
{
	XbMachineOpcodeFixupItem *item;
	XbMachinePrivate *priv = GET_PRIVATE (self);
	guint level = 0;
	g_autoptr(XbStack) opcodes = NULL;
	g_autofree gchar *opcodes_sig = NULL;

	g_return_val_if_fail (XB_IS_MACHINE (self), NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

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

	/* parse into opcodes */
	opcodes = xb_stack_new (priv->stack_size);
	if (xb_machine_parse_text (self, opcodes, text, text_len, level, error) == G_MAXSIZE)
		return NULL;

	/* do any fixups */
	opcodes_sig = xb_machine_get_opcodes_sig (self, opcodes);
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_OPTIMIZER)
		g_debug ("opcodes_sig=%s", opcodes_sig);
	item = g_hash_table_lookup (priv->opcode_fixup, opcodes_sig);
	if (item != NULL) {
		if (!item->fixup_cb (self, opcodes, item->user_data, error))
			return NULL;
	}

	/* optimize */
	if (flags & XB_MACHINE_PARSE_FLAG_OPTIMIZE) {
		for (guint i = 0; i < 10; i++) {
			guint oldsz = xb_stack_get_size (opcodes);

			/* Is the stack optimal already? */
			if (oldsz == 1)
				break;

			if (!xb_machine_opcodes_optimize (self, opcodes, error))
				return NULL;
			if (oldsz == xb_stack_get_size (opcodes))
				break;
		}
	}

	/* success */
	return g_steal_pointer (&opcodes);
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
 * Returns: (transfer full): opcodes, or %NULL on error
 *
 * Since: 0.1.1
 **/
XbStack *
xb_machine_parse (XbMachine *self,
		  const gchar *text,
		  gssize text_len,
		  GError **error)
{
	return xb_machine_parse_full (self, text, text_len,
				      XB_MACHINE_PARSE_FLAG_OPTIMIZE,
				      error);
}

static void
xb_machine_debug_show_stack (XbMachine *self, XbStack *stack)
{
	g_autofree gchar *str = NULL;
	if (xb_stack_get_size (stack) == 0) {
		g_debug ("stack is empty");
		return;
	}
	str = xb_stack_to_string (stack);
	g_debug ("stack: %s", str);
}

static gboolean
xb_machine_run_func (XbMachine *self,
		     XbStack *stack,
		     XbOpcode *opcode,
		     gpointer exec_data,
		     GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	XbMachineMethodItem *item = g_ptr_array_index (priv->methods, xb_opcode_get_val (opcode));

	/* optional debugging */
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK) {
		g_autofree gchar *str = xb_opcode_to_string (opcode);
		g_debug ("running: %s", str);
		xb_machine_debug_show_stack (self, stack);
	}

	/* check we have enough stack elements */
	if (item->n_opcodes > xb_stack_get_size (stack)) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "function required %u arguments, stack only has %u",
			     item->n_opcodes, xb_stack_get_size (stack));
		return FALSE;
	}
	if (!item->method_cb (self, stack, NULL, item->user_data, exec_data, error)) {
		g_prefix_error (error, "failed to call %s(): ", item->name);
		return FALSE;
	}
	return TRUE;
}

/**
 * xb_machine_run:
 * @self: a #XbMachine
 * @opcodes: a #XbStack of opcodes
 * @result: (out): return status after running @opcodes
 * @exec_data: per-run user data that is passed to all the #XbMachineMethodFunc functions
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
 * Deprecated: 0.3.0: Use xb_machine_run_with_bindings() instead.
 **/
gboolean
xb_machine_run (XbMachine *self,
		XbStack *opcodes,
		gboolean *result,
		gpointer exec_data,
		GError **error)
{
	return xb_machine_run_with_bindings (self, opcodes, NULL, result, exec_data, error);
}

/**
 * xb_machine_run_with_bindings:
 * @self: a #XbMachine
 * @opcodes: a #XbStack of opcodes
 * @bindings: (nullable) (transfer none): values bound to opcodes of type
 *     %XB_OPCODE_KIND_BOUND_INTEGER or %XB_OPCODE_KIND_BOUND_TEXT, or %NULL if
 *     the query doesn’t need any bound values
 * @result: (out): return status after running @opcodes
 * @exec_data: per-run user data that is passed to all the #XbMachineMethodFunc functions
 * @error: a #GError, or %NULL
 *
 * Runs a set of opcodes on the virtual machine, using the bound values given in
 * @bindings to substitute for bound opcodes.
 *
 * It is safe to call this function from a different thread to the one that
 * created the #XbMachine.
 *
 * Returns: a new #XbOpcode, or %NULL
 *
 * Since: 0.3.0
 **/
gboolean
xb_machine_run_with_bindings (XbMachine        *self,
			      XbStack          *opcodes,
			      XbValueBindings  *bindings,
			      gboolean         *result,
			      gpointer          exec_data,
			      GError          **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	g_auto(XbOpcode) opcode_success = XB_OPCODE_INIT ();
	g_autoptr(XbStack) stack = NULL;
	guint opcodes_stack_size = xb_stack_get_size (opcodes);
	guint bound_opcode_idx = 0;

	g_return_val_if_fail (XB_IS_MACHINE (self), FALSE);
	g_return_val_if_fail (opcodes != NULL, FALSE);
	g_return_val_if_fail (result != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* process each opcode */
	stack = xb_stack_new (priv->stack_size);
	for (guint i = 0; i < opcodes_stack_size; i++) {
		XbOpcode *opcode = xb_stack_peek (opcodes, i);
		XbOpcodeKind kind = xb_opcode_get_kind (opcode);

		/* replace post-0.3.0-style bound opcodes with their bound values */
		if (bindings != NULL &&
		    (kind == XB_OPCODE_KIND_BOUND_TEXT ||
		     kind == XB_OPCODE_KIND_BOUND_INTEGER)) {
			XbOpcode *machine_opcode;
			if (!xb_machine_stack_push (self,
						    stack,
						    &machine_opcode,
						    error))
				return FALSE;
			if (!xb_value_bindings_lookup_opcode (bindings, bound_opcode_idx++, machine_opcode)) {
				g_autofree gchar *tmp1 = xb_stack_to_string (stack);
				g_autofree gchar *tmp2 = xb_stack_to_string (opcodes);
				g_set_error (error,
					     G_IO_ERROR,
					     G_IO_ERROR_INVALID_DATA,
					     "opcode was not bound at runtime, stack:%s, opcodes:%s",
					     tmp1, tmp2);
				return FALSE;
			}
			continue;
		} else if (kind == XB_OPCODE_KIND_BOUND_UNSET) {
			g_autofree gchar *tmp1 = xb_stack_to_string (stack);
			g_autofree gchar *tmp2 = xb_stack_to_string (opcodes);
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "opcode was not bound at runtime, stack:%s, opcodes:%s",
				     tmp1, tmp2);
			return FALSE;
		}

		/* process the stack */
		if (kind == XB_OPCODE_KIND_FUNCTION) {
			if (!xb_machine_run_func (self,
						  stack,
						  opcode,
						  exec_data,
						  error))
				return FALSE;
			continue;
		}

		/* add to stack; this uses a const copy of the input opcode,
		 * so ownership of anything allocated on the heap remains with
		 * the caller */
		if (kind == XB_OPCODE_KIND_TEXT ||
		    kind == XB_OPCODE_KIND_BOOLEAN ||
		    kind == XB_OPCODE_KIND_INTEGER ||
		    kind == XB_OPCODE_KIND_INDEXED_TEXT ||
		    (bindings == NULL &&
		     (kind == XB_OPCODE_KIND_BOUND_TEXT ||
		      kind == XB_OPCODE_KIND_BOUND_INTEGER))) {
			XbOpcode *machine_opcode;
			if (!xb_machine_stack_push (self,
						    stack,
						    &machine_opcode,
						    error))
				return FALSE;
			*machine_opcode = *opcode;
			machine_opcode->destroy_func = NULL;
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

	/* the stack should have one boolean left on the stack */
	if (xb_stack_get_size (stack) != 1) {
		g_autofree gchar *tmp = xb_stack_to_string (stack);
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "%u opcodes remain on the stack (%s)",
			     xb_stack_get_size (stack), tmp);
		return FALSE;
	}
	if (!xb_stack_pop (stack, &opcode_success, error))
		return FALSE;
	if (xb_opcode_get_kind (&opcode_success) != XB_OPCODE_KIND_BOOLEAN) {
		g_autofree gchar *tmp = xb_stack_to_string (stack);
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "Expected boolean, got: %s", tmp);
		return FALSE;
	}
	if (result != NULL)
		*result = xb_opcode_get_val (&opcode_success);

	/* success */
	return TRUE;
}

/**
 * xb_machine_stack_pop:
 * @self: a #XbMachine
 * @stack: a #XbStack
 * @opcode_out: (out caller-allocates) (optional): return location for the popped #XbOpcode
 * @error: a #GError, or %NULL
 *
 * Pops an opcode from the stack.
 *
 * Returns: %TRUE if popping succeeded, %FALSE if the stack was empty already
 *
 * Since: 0.2.0
 **/
gboolean
xb_machine_stack_pop (XbMachine *self, XbStack *stack, XbOpcode *opcode_out, GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	gboolean retval;

	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK) {
		XbOpcode *opcode_peek = xb_stack_peek (stack, xb_stack_get_size (stack) - 1);
		if (opcode_peek != NULL) {
			g_autofree gchar *str = xb_opcode_to_string (opcode_peek);
			g_debug ("popping: %s", str);
		} else {
			g_debug ("not popping: stack empty");
		}
	}

	retval = xb_stack_pop (stack, opcode_out, error);

	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		xb_machine_debug_show_stack (self, stack);

	return retval;
}

/**
 * xb_machine_stack_pop_two: (skip):
 **/
gboolean
xb_machine_stack_pop_two (XbMachine *self, XbStack *stack,
			  XbOpcode *opcode1_out, XbOpcode *opcode2_out,
			  GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	gboolean retval;

	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK) {
		XbOpcode *opcode_peek1 = xb_stack_peek (stack, xb_stack_get_size (stack) - 1);
		XbOpcode *opcode_peek2 = xb_stack_peek (stack, xb_stack_get_size (stack) - 2);
		if (opcode_peek1 != NULL && opcode_peek2 != NULL) {
			g_autofree gchar *str1 = xb_opcode_to_string (opcode_peek1);
			g_autofree gchar *str2 = xb_opcode_to_string (opcode_peek2);
			g_debug ("popping1: %s", str1);
			g_debug ("popping2: %s", str2);
		} else {
			g_debug ("not popping: stack empty");
		}
	}

	retval = xb_stack_pop_two (stack, opcode1_out, opcode2_out, error);

	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		xb_machine_debug_show_stack (self, stack);

	return retval;
}

/**
 * xb_machine_stack_push:
 * @self: a #XbMachine
 * @stack: a #XbStack
 * @opcode_out: (out) (nullable): return location for the new #XbOpcode
 * @error: return location for a #GError, or %NULL
 *
 * Pushes a new empty opcode onto the end of the stack. A pointer to the opcode
 * is returned in @opcode_out so that the caller can initialise it.
 *
 * If the stack reaches its maximum size, %G_IO_ERROR_NO_SPACE will be returned.
 *
 * Returns: %TRUE if a new empty opcode was returned, or %FALSE if the stack has
 *    reached its maximum size
 * Since: 0.2.0
 **/
gboolean
xb_machine_stack_push (XbMachine *self, XbStack *stack, XbOpcode **opcode_out, GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);

	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK) {
		g_debug ("pushing generic opcode");
	}

	return xb_stack_push (stack, opcode_out, error);
}

/**
 * xb_machine_stack_push_text:
 * @self: a #XbMachine
 * @stack: a #XbStack
 * @str: text literal
 * @error: return location for a #GError, or %NULL
 *
 * Adds a text literal to the stack, copying @str.
 *
 * Errors are as for xb_machine_stack_push().
 *
 * Returns: %TRUE on success, %FALSE otherwise
 * Since: 0.2.0
 **/
gboolean
xb_machine_stack_push_text (XbMachine *self, XbStack *stack, const gchar *str, GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	XbOpcode *opcode;

	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		g_debug ("pushing: %s", str);

	if (!xb_stack_push (stack, &opcode, error))
		return FALSE;
	xb_opcode_text_init (opcode, str);
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		xb_machine_debug_show_stack (self, stack);

	return TRUE;
}

/**
 * xb_machine_stack_push_text_static:
 * @self: a #XbMachine
 * @stack: a #XbStack
 * @str: text literal
 * @error: return location for a #GError, or %NULL
 *
 * Adds static text literal to the stack.
 *
 * Errors are as for xb_machine_stack_push().
 *
 * Returns: %TRUE on success, %FALSE otherwise
 * Since: 0.2.0
 **/
gboolean
xb_machine_stack_push_text_static (XbMachine *self, XbStack *stack, const gchar *str, GError **error)
{
	XbOpcode *opcode;

	XbMachinePrivate *priv = GET_PRIVATE (self);
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		g_debug ("pushing: %s", str);

	if (!xb_stack_push (stack, &opcode, error))
		return FALSE;
	xb_opcode_text_init_static (opcode, str);
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		xb_machine_debug_show_stack (self, stack);

	return TRUE;
}

/**
 * xb_machine_stack_push_text_steal:
 * @self: a #XbMachine
 * @stack: a #XbStack
 * @str: (transfer full): text literal
 * @error: return location for a #GError, or %NULL
 *
 * Adds a stolen text literal to the stack.
 *
 * Errors are as for xb_machine_stack_push().
 *
 * Returns: %TRUE on success, %FALSE otherwise
 * Since: 0.2.0
 **/
gboolean
xb_machine_stack_push_text_steal (XbMachine *self, XbStack *stack, gchar *str, GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	XbOpcode *opcode;
	g_autofree gchar *str_stolen = g_steal_pointer (&str);

	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		g_debug ("pushing: %s", str_stolen);

	if (!xb_stack_push (stack, &opcode, error))
		return FALSE;
	xb_opcode_text_init_steal (opcode, g_steal_pointer (&str_stolen));
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		xb_machine_debug_show_stack (self, stack);

	return TRUE;
}

/**
 * xb_machine_stack_push_integer:
 * @self: a #XbMachine
 * @stack: a #XbStack
 * @val: integer literal
 * @error: return location for a #GError, or %NULL
 *
 * Adds an integer literal to the stack.
 *
 * Errors are as for xb_machine_stack_push().
 *
 * Returns: %TRUE on success, %FALSE otherwise
 * Since: 0.2.0
 **/
gboolean
xb_machine_stack_push_integer (XbMachine *self, XbStack *stack, guint32 val, GError **error)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	XbOpcode *opcode;

	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		g_debug ("pushing: %u", val);

	if (!xb_stack_push (stack, &opcode, error))
		return FALSE;
	xb_opcode_integer_init (opcode, val);
	if (priv->debug_flags & XB_MACHINE_DEBUG_FLAG_SHOW_STACK)
		xb_machine_debug_show_stack (self, stack);

	return TRUE;
}

/**
 * xb_machine_set_stack_size:
 * @self: a #XbMachine
 * @stack_size: integer
 *
 * Sets the maximum stack size used for the machine.
 *
 * The stack size will be affective for new jobs started with xb_machine_run()
 * and xb_machine_parse().
 *
 * Since: 0.1.3
 **/
void
xb_machine_set_stack_size (XbMachine *self, guint stack_size)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (XB_IS_MACHINE (self));
	g_return_if_fail (stack_size != 0);
	priv->stack_size = stack_size;
}

/**
 * xb_machine_get_stack_size:
 * @self: a #XbMachine
 *
 * Gets the maximum stack size used for the machine.
 *
 * Returns: integer
 *
 * Since: 0.1.3
 **/
guint
xb_machine_get_stack_size (XbMachine *self)
{
	XbMachinePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (XB_IS_MACHINE (self), 0);
	return priv->stack_size;
}

typedef gboolean (*OpcodeCheckFunc) (XbOpcode *op);

static gboolean
xb_opcode_cmp_val_or_str (XbOpcode *op)
{
	return xb_opcode_cmp_str (op) || xb_opcode_cmp_val (op);
}

static gboolean
xb_machine_check_one_arg (XbStack *stack, OpcodeCheckFunc f, GError **error)
{
	XbOpcode *head;

	head = xb_stack_peek_tail (stack);
	if (head == NULL || !f (head)) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "%s type not supported",
			     (head != NULL) ? xb_opcode_kind_to_string (xb_opcode_get_kind (head)) : "(null)");
		return FALSE;
	}

	return TRUE;
}

static gboolean
xb_machine_check_two_args (XbStack *stack,
			   OpcodeCheckFunc f1,
			   OpcodeCheckFunc f2,
			   GError **error)
{
	XbOpcode *head1 = NULL;
	XbOpcode *head2 = NULL;
	guint stack_size = xb_stack_get_size (stack);

	if (stack_size >= 2) {
		head1 = xb_stack_peek (stack, stack_size - 1);
		head2 = xb_stack_peek (stack, stack_size - 2);
	}
	if (head1 == NULL || head2 == NULL ||
	    !f1 (head1) || !f2 (head2)) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "%s:%s types not supported",
			     (head1 != NULL) ? xb_opcode_kind_to_string (xb_opcode_get_kind (head1)) : "(null)",
			     (head2 != NULL) ? xb_opcode_kind_to_string (xb_opcode_get_kind (head2)) : "(null)");
		return FALSE;
	}

	return TRUE;
}

static gboolean
xb_machine_func_and_cb (XbMachine *self,
			XbStack *stack,
			gboolean *result,
			gpointer user_data,
			gpointer exec_data,
			GError **error)
{
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT ();

	if (!xb_machine_check_two_args (stack, xb_opcode_cmp_val, xb_opcode_cmp_val, error))
		return FALSE;
	if (!xb_machine_stack_pop_two (self, stack, &op1, &op2, error))
		return FALSE;

	/* INTE:INTE */
	return xb_stack_push_bool (stack,
				   xb_opcode_get_val (&op1) && xb_opcode_get_val (&op2),
				   error);
}

static gboolean
xb_machine_func_or_cb (XbMachine *self,
		       XbStack *stack,
		       gboolean *result,
		       gpointer user_data,
		       gpointer exec_data,
		       GError **error)
{
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT ();

	if (!xb_machine_check_two_args (stack, xb_opcode_cmp_val, xb_opcode_cmp_val, error))
		return FALSE;
	if (!xb_machine_stack_pop_two (self, stack, &op1, &op2, error))
		return FALSE;

	/* INTE:INTE */
	return xb_stack_push_bool (stack,
				   xb_opcode_get_val (&op1) || xb_opcode_get_val (&op2),
				   error);
}

static gboolean
xb_machine_func_eq_cb (XbMachine *self,
		       XbStack *stack,
		       gboolean *result,
		       gpointer user_data,
		       gpointer exec_data,
		       GError **error)
{
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT ();

	if (!xb_machine_check_two_args (stack,
					xb_opcode_cmp_val_or_str,
					xb_opcode_cmp_val_or_str,
					error))
		return FALSE;
	if (!xb_machine_stack_pop_two (self, stack, &op1, &op2, error))
		return FALSE;

	/* INTE:INTE */
	if (xb_opcode_cmp_val (&op1) && xb_opcode_cmp_val (&op2))
		return xb_stack_push_bool (stack, xb_opcode_get_val (&op1) == xb_opcode_get_val (&op2), error);

	/* TEXT:TEXT */
	if (xb_opcode_cmp_str (&op1) && xb_opcode_cmp_str (&op2))
		return xb_stack_push_bool (stack, g_strcmp0 (xb_opcode_get_str (&op1),
							     xb_opcode_get_str (&op2)) == 0, error);

	/* INTE:TEXT */
	if (xb_opcode_cmp_val (&op1) && xb_opcode_cmp_str (&op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (&op2) == NULL)
			return xb_stack_push_bool (stack, FALSE, error);
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (&op2),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		return xb_stack_push_bool (stack, val == xb_opcode_get_val (&op1), error);
	}

	/* TEXT:INTE */
	if (xb_opcode_cmp_str (&op1) && xb_opcode_cmp_val (&op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (&op1) == NULL)
			return xb_stack_push_bool (stack, FALSE, error);
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (&op1),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		return xb_stack_push_bool (stack, val == xb_opcode_get_val (&op2), error);
	}

	/* should have been checked above */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "cannot compare %s and %s",
		     xb_opcode_kind_to_string (xb_opcode_get_kind (&op1)),
		     xb_opcode_kind_to_string (xb_opcode_get_kind (&op2)));
	return FALSE;
}

static gboolean
xb_machine_func_ne_cb (XbMachine *self,
		       XbStack *stack,
		       gboolean *result,
		       gpointer user_data,
		       gpointer exec_data,
		       GError **error)
{
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT ();

	if (!xb_machine_check_two_args (stack,
					xb_opcode_cmp_val_or_str,
					xb_opcode_cmp_val_or_str,
					error))
		return FALSE;
	if (!xb_machine_stack_pop_two (self, stack, &op1, &op2, error))
		return FALSE;

	/* INTE:INTE */
	if (xb_opcode_cmp_val (&op1) && xb_opcode_cmp_val (&op2)) {
		return xb_stack_push_bool (stack,
					   xb_opcode_get_val (&op1) != xb_opcode_get_val (&op2),
					   error);
	}

	/* TEXT:TEXT */
	if (xb_opcode_cmp_str (&op1) && xb_opcode_cmp_str (&op2)) {
		return xb_stack_push_bool (stack,
					   g_strcmp0 (xb_opcode_get_str (&op1),
						      xb_opcode_get_str (&op2)) != 0,
					   error);
	}

	/* INTE:TEXT */
	if (xb_opcode_cmp_val (&op1) && xb_opcode_cmp_str (&op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (&op2) == NULL)
			return xb_stack_push_bool (stack, FALSE, error);
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (&op2),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		return xb_stack_push_bool (stack,
					   val != xb_opcode_get_val (&op1),
					   error);
	}

	/* TEXT:INTE */
	if (xb_opcode_cmp_str (&op1) && xb_opcode_cmp_val (&op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (&op1) == NULL)
			return xb_stack_push_bool (stack, FALSE, error);
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (&op1),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		return xb_stack_push_bool (stack, val != xb_opcode_get_val (&op2), error);
	}

	/* should have been checked above */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "cannot compare %s and %s",
		     xb_opcode_kind_to_string (xb_opcode_get_kind (&op1)),
		     xb_opcode_kind_to_string (xb_opcode_get_kind (&op2)));
	return FALSE;
}

static gboolean
xb_machine_func_lt_cb (XbMachine *self,
		       XbStack *stack,
		       gboolean *result,
		       gpointer user_data,
		       gpointer exec_data,
		       GError **error)
{
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT ();

	if (!xb_machine_check_two_args (stack,
					xb_opcode_cmp_val_or_str,
					xb_opcode_cmp_val_or_str,
					error))
		return FALSE;
	if (!xb_machine_stack_pop_two (self, stack, &op1, &op2, error))
		return FALSE;

	/* INTE:INTE */
	if (xb_opcode_cmp_val (&op1) && xb_opcode_cmp_val (&op2)) {
		return xb_stack_push_bool (stack,
					   xb_opcode_get_val (&op2) < xb_opcode_get_val (&op1),
					   error);
	}

	/* TEXT:TEXT */
	if (xb_opcode_cmp_str (&op1) && xb_opcode_cmp_str (&op2)) {
		return xb_stack_push_bool (stack,
					   g_strcmp0 (xb_opcode_get_str (&op2),
						      xb_opcode_get_str (&op1)) < 0,
					   error);
	}

	/* INTE:TEXT */
	if (xb_opcode_cmp_val (&op1) && xb_opcode_cmp_str (&op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (&op2) == NULL)
			return xb_stack_push_bool (stack, FALSE, error);
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (&op2),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		return xb_stack_push_bool (stack, val < xb_opcode_get_val (&op1), error);
	}

	/* TEXT:INTE */
	if (xb_opcode_cmp_str (&op1) && xb_opcode_cmp_val (&op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (&op1) == NULL)
			return xb_stack_push_bool (stack, FALSE, error);
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (&op1),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		return xb_stack_push_bool (stack, val < xb_opcode_get_val (&op2), error);
	}

	/* should have been checked above */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "cannot compare %s and %s",
		     xb_opcode_kind_to_string (xb_opcode_get_kind (&op1)),
		     xb_opcode_kind_to_string (xb_opcode_get_kind (&op2)));
	return FALSE;
}

static gboolean
xb_machine_func_gt_cb (XbMachine *self,
		       XbStack *stack,
		       gboolean *result,
		       gpointer user_data,
		       gpointer exec_data,
		       GError **error)
{
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT ();

	if (!xb_machine_check_two_args (stack,
					xb_opcode_cmp_val_or_str,
					xb_opcode_cmp_val_or_str,
					error))
		return FALSE;
	if (!xb_machine_stack_pop_two (self, stack, &op1, &op2, error))
		return FALSE;

	/* INTE:INTE */
	if (xb_opcode_cmp_val (&op1) && xb_opcode_cmp_val (&op2)) {
		return xb_stack_push_bool (stack,
					   xb_opcode_get_val (&op2) > xb_opcode_get_val (&op1),
					   error);
	}

	/* TEXT:TEXT */
	if (xb_opcode_cmp_str (&op1) && xb_opcode_cmp_str (&op2)) {
		return xb_stack_push_bool (stack,
					   g_strcmp0 (xb_opcode_get_str (&op2),
						      xb_opcode_get_str (&op1)) > 0,
					   error);
	}

	/* INTE:TEXT */
	if (xb_opcode_cmp_val (&op1) && xb_opcode_cmp_str (&op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (&op2) == NULL)
			return xb_stack_push_bool (stack, FALSE, error);
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (&op2),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		return xb_stack_push_bool (stack, val > xb_opcode_get_val (&op1), error);
	}

	/* TEXT:INTE */
	if (xb_opcode_cmp_str (&op1) && xb_opcode_cmp_val (&op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (&op1) == NULL)
			return xb_stack_push_bool (stack, FALSE, error);
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (&op1),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		return xb_stack_push_bool (stack, val > xb_opcode_get_val (&op2), error);
	}

	/* should have been checked above */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "cannot compare %s and %s",
		     xb_opcode_kind_to_string (xb_opcode_get_kind (&op1)),
		     xb_opcode_kind_to_string (xb_opcode_get_kind (&op2)));
	return FALSE;
}

static gboolean
xb_machine_func_le_cb (XbMachine *self,
		       XbStack *stack,
		       gboolean *result,
		       gpointer user_data,
		       gpointer exec_data,
		       GError **error)
{
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT ();

	if (!xb_machine_check_two_args (stack,
					xb_opcode_cmp_val_or_str,
					xb_opcode_cmp_val_or_str,
					error))
		return FALSE;
	if (!xb_machine_stack_pop_two (self, stack, &op1, &op2, error))
		return FALSE;

	/* INTE:INTE */
	if (xb_opcode_cmp_val (&op1) && xb_opcode_cmp_val (&op2)) {
		return xb_stack_push_bool (stack,
					   xb_opcode_get_val (&op2) <= xb_opcode_get_val (&op1),
					   error);
	}

	/* TEXT:TEXT */
	if (xb_opcode_cmp_str (&op1) && xb_opcode_cmp_str (&op2)) {
		return xb_stack_push_bool (stack,
					   g_strcmp0 (xb_opcode_get_str (&op2),
						      xb_opcode_get_str (&op1)) <= 0,
					   error);
		return TRUE;
	}

	/* INTE:TEXT */
	if (xb_opcode_cmp_val (&op1) && xb_opcode_cmp_str (&op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (&op2) == NULL)
			return xb_stack_push_bool (stack, FALSE, error);
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (&op2),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		return xb_stack_push_bool (stack, val <= xb_opcode_get_val (&op1), error);
	}

	/* TEXT:INTE */
	if (xb_opcode_cmp_str (&op1) && xb_opcode_cmp_val (&op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (&op1) == NULL)
			return xb_stack_push_bool (stack, FALSE, error);
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (&op1),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		return xb_stack_push_bool (stack, val <= xb_opcode_get_val (&op2), error);
	}

	/* should have been checked above */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "cannot compare %s and %s",
		     xb_opcode_kind_to_string (xb_opcode_get_kind (&op1)),
		     xb_opcode_kind_to_string (xb_opcode_get_kind (&op2)));
	return FALSE;
}

static gboolean
xb_machine_func_lower_cb (XbMachine *self,
			  XbStack *stack,
			  gboolean *result,
			  gpointer user_data,
			  gpointer exec_data,
			  GError **error)
{
	g_auto(XbOpcode) op = XB_OPCODE_INIT ();

	if (!xb_machine_check_one_arg (stack, xb_opcode_cmp_str, error))
		return FALSE;
	if (!xb_machine_stack_pop (self, stack, &op, error))
		return FALSE;

	/* TEXT */
	return xb_machine_stack_push_text_steal (self, stack,
						 g_utf8_strdown (xb_opcode_get_str (&op), -1),
						 error);
}

static gboolean
xb_machine_func_upper_cb (XbMachine *self,
			  XbStack *stack,
			  gboolean *result,
			  gpointer user_data,
			  gpointer exec_data,
			  GError **error)
{
	g_auto(XbOpcode) op = XB_OPCODE_INIT ();

	if (!xb_machine_check_one_arg (stack, xb_opcode_cmp_str, error))
		return FALSE;
	if (!xb_machine_stack_pop (self, stack, &op, error))
		return FALSE;

	/* TEXT */
	return xb_machine_stack_push_text_steal (self, stack,
						 g_utf8_strup (xb_opcode_get_str (&op), -1),
						 error);
}

static gboolean
xb_machine_func_not_cb (XbMachine *self,
			XbStack *stack,
			gboolean *result,
			gpointer user_data,
			gpointer exec_data,
			GError **error)
{
	g_auto(XbOpcode) op = XB_OPCODE_INIT ();

	if (!xb_machine_check_one_arg (stack, xb_opcode_cmp_val_or_str, error))
		return FALSE;
	if (!xb_machine_stack_pop (self, stack, &op, error))
		return FALSE;

	/* TEXT */
	if (xb_opcode_cmp_str (&op))
		return xb_stack_push_bool (stack, xb_opcode_get_str (&op) == NULL, error);

	/* INTE */
	if (xb_opcode_cmp_val (&op))
		return xb_stack_push_bool (stack, xb_opcode_get_val (&op) == 0, error);

	/* should have been checked above */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "cannot invert %s",
		     xb_opcode_kind_to_string (xb_opcode_get_kind (&op)));
	return FALSE;
}

static gboolean
xb_machine_func_ge_cb (XbMachine *self,
		       XbStack *stack,
		       gboolean *result,
		       gpointer user_data,
		       gpointer exec_data,
		       GError **error)
{
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT ();

	if (!xb_machine_check_two_args (stack,
					xb_opcode_cmp_val_or_str,
					xb_opcode_cmp_val_or_str,
					error))
		return FALSE;
	if (!xb_machine_stack_pop_two (self, stack, &op1, &op2, error))
		return FALSE;

	/* TEXT:TEXT */
	if (xb_opcode_cmp_str (&op1) && xb_opcode_cmp_str (&op2)) {
		return xb_stack_push_bool (stack, g_strcmp0 (xb_opcode_get_str (&op2),
							     xb_opcode_get_str (&op1)) >= 0, error);
	}

	/* INTE:INTE */
	if (xb_opcode_cmp_val (&op1) && xb_opcode_cmp_val (&op2)) {
		return xb_stack_push_bool (stack, xb_opcode_get_val (&op2) >= xb_opcode_get_val (&op1), error);
	}

	/* INTE:TEXT */
	if (xb_opcode_cmp_val (&op1) && xb_opcode_cmp_str (&op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (&op2) == NULL)
			return xb_stack_push_bool (stack, FALSE, error);
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (&op2),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		return xb_stack_push_bool (stack, val >= xb_opcode_get_val (&op1), error);
	}

	/* TEXT:INTE */
	if (xb_opcode_cmp_str (&op1) && xb_opcode_cmp_val (&op2)) {
		guint64 val = 0;
		if (xb_opcode_get_str (&op1) == NULL)
			return xb_stack_push_bool (stack, FALSE, error);
		if (!g_ascii_string_to_unsigned (xb_opcode_get_str (&op1),
						 10, 0, G_MAXUINT32,
						 &val, error)) {
			return FALSE;
		}
		return xb_stack_push_bool (stack, val >= xb_opcode_get_val (&op2), error);
	}

	/* should have been checked above */
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_NOT_SUPPORTED,
		     "cannot compare %s and %s",
		     xb_opcode_kind_to_string (xb_opcode_get_kind (&op1)),
		     xb_opcode_kind_to_string (xb_opcode_get_kind (&op2)));
	return FALSE;
}

static gboolean
xb_machine_func_contains_cb (XbMachine *self,
			     XbStack *stack,
			     gboolean *result,
			     gpointer user_data,
			     gpointer exec_data,
			     GError **error)
{
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT ();

	if (!xb_machine_check_two_args (stack,
					xb_opcode_cmp_str,
					xb_opcode_cmp_str,
					error))
		return FALSE;
	if (!xb_machine_stack_pop_two (self, stack, &op1, &op2, error))
		return FALSE;

	/* TEXT:TEXT */
	return xb_stack_push_bool (stack,
				   xb_string_contains (xb_opcode_get_str (&op2),
						       xb_opcode_get_str (&op1)),
				   error);
}

static gboolean
xb_machine_func_starts_with_cb (XbMachine *self,
			        XbStack *stack,
			        gboolean *result,
			        gpointer user_data,
			        gpointer exec_data,
			        GError **error)
{
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT ();

	if (!xb_machine_check_two_args (stack,
					xb_opcode_cmp_str,
					xb_opcode_cmp_str,
					error))
		return FALSE;
	if (!xb_machine_stack_pop_two (self, stack, &op1, &op2, error))
		return FALSE;

	/* TEXT:TEXT */
	return xb_stack_push_bool (stack,
				   g_str_has_prefix (xb_opcode_get_str (&op2),
						     xb_opcode_get_str (&op1)),
				   error);
}

static gboolean
xb_machine_func_ends_with_cb (XbMachine *self,
			      XbStack *stack,
			      gboolean *result,
			      gpointer user_data,
			      gpointer exec_data,
			      GError **error)
{
	g_auto(XbOpcode) op1 = XB_OPCODE_INIT ();
	g_auto(XbOpcode) op2 = XB_OPCODE_INIT ();

	if (!xb_machine_check_two_args (stack,
					xb_opcode_cmp_str,
					xb_opcode_cmp_str,
					error))
		return FALSE;
	if (!xb_machine_stack_pop_two (self, stack, &op1, &op2, error))
		return FALSE;

	/* TEXT:TEXT */
	return xb_stack_push_bool (stack,
				   g_str_has_suffix (xb_opcode_get_str (&op2),
						     xb_opcode_get_str (&op1)),
				   error);
}

static gboolean
xb_machine_func_number_cb (XbMachine *self,
			   XbStack *stack,
			   gboolean *result,
			   gpointer user_data,
			   gpointer exec_data,
			   GError **error)
{
	guint64 val = 0;
	g_auto(XbOpcode) op = XB_OPCODE_INIT ();

	if (!xb_machine_check_one_arg (stack, xb_opcode_cmp_str, error))
		return FALSE;
	if (!xb_machine_stack_pop (self, stack, &op, error))
		return FALSE;

	/* TEXT */
	if (xb_opcode_get_str (&op) == NULL)
		return xb_stack_push_bool (stack, FALSE, error);
	if (!g_ascii_string_to_unsigned (xb_opcode_get_str (&op),
					 10, 0, G_MAXUINT32,
					 &val, error)) {
		return FALSE;
	}
	return xb_machine_stack_push_integer (self, stack, val, error);
}

static gboolean
xb_machine_func_strlen_cb (XbMachine *self,
			   XbStack *stack,
			   gboolean *result,
			   gpointer user_data,
			   gpointer exec_data,
			   GError **error)
{
	g_auto(XbOpcode) op = XB_OPCODE_INIT ();

	if (!xb_machine_check_one_arg (stack, xb_opcode_cmp_str, error))
		return FALSE;
	if (!xb_machine_stack_pop (self, stack, &op, error))
		return FALSE;

	/* TEXT */
	if (xb_opcode_get_str (&op) == NULL)
		return xb_stack_push_bool (stack, FALSE, error);
	return xb_machine_stack_push_integer (self,
					      stack,
					      strlen (xb_opcode_get_str (&op)),
					      error);
}

static gboolean
xb_machine_func_string_cb (XbMachine *self,
			   XbStack *stack,
			   gboolean *result,
			   gpointer user_data,
			   gpointer exec_data,
			   GError **error)
{
	gchar *tmp;
	g_auto(XbOpcode) op = XB_OPCODE_INIT ();

	if (!xb_machine_check_one_arg (stack, xb_opcode_cmp_val, error))
		return FALSE;
	if (!xb_machine_stack_pop (self, stack, &op, error))
		return FALSE;

	/* INTE */
	tmp = g_strdup_printf ("%" G_GUINT32_FORMAT, xb_opcode_get_val (&op));
	return xb_machine_stack_push_text_steal (self, stack, tmp, error);
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
	priv->stack_size = 10;
	priv->methods = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_machine_func_free);
	priv->operators = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_machine_operator_free);
	priv->text_handlers = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_machine_text_handler_free);
	priv->opcode_fixup = g_hash_table_new_full (g_str_hash, g_str_equal,
						     g_free, (GDestroyNotify) xb_machine_opcode_fixup_free);

	/* built-in functions */
	xb_machine_add_method (self, "and", 2, xb_machine_func_and_cb, NULL, NULL);
	xb_machine_add_method (self, "or", 2, xb_machine_func_or_cb, NULL, NULL);
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
	xb_machine_add_operator (self, " and ", "and");
	xb_machine_add_operator (self, " or ", "or");
	xb_machine_add_operator (self, "&&", "and");
	xb_machine_add_operator (self, "||", "or");
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
