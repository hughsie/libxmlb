/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN "XbMachine"

#include "config.h"

#include <gio/gio.h>

#include "xb-opcode-private.h"
#include "xb-stack-private.h"

/**
 * xb_stack_unref:
 * @self: a #XbStack
 *
 * Decrements the reference count of the stack, freeing the object when the
 * refcount drops to zero.
 *
 * Since: 0.1.3
 **/
void
xb_stack_unref(XbStack *self)
{
	g_assert(self->ref > 0);
	if (--self->ref > 0)
		return;
	for (guint i = 0; i < self->pos; i++)
		xb_opcode_clear(&self->opcodes[i]);
	if (!self->stack_allocated)
		g_free(self);
}

/**
 * xb_stack_ref:
 * @self: a #XbStack
 *
 * Increments the refcount of the stack.
 *
 * Returns: (transfer none): the original @self #XbStack instance
 *
 * Since: 0.1.3
 **/
XbStack *
xb_stack_ref(XbStack *self)
{
	self->ref++;
	return self;
}

/**
 * xb_stack_pop:
 * @self: a #XbStack
 * @opcode_out: (out caller-allocates) (optional): return location for the popped #XbOpcode
 * @error: a #GError, or %NULL
 *
 * Pops an opcode off the stack.
 *
 * Returns: %TRUE if popping succeeded, %FALSE if the stack was empty already
 *
 * Since: 0.2.0
 **/
gboolean
xb_stack_pop(XbStack *self, XbOpcode *opcode_out, GError **error)
{
	if (self->pos == 0) {
		g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "stack is empty");
		return FALSE;
	}
	self->pos--;
	if (opcode_out != NULL)
		*opcode_out = self->opcodes[self->pos];
	return TRUE;
}

/**
 * xb_stack_pop_two: (skip):
 **/
gboolean
xb_stack_pop_two(XbStack *self, XbOpcode *opcode1_out, XbOpcode *opcode2_out, GError **error)
{
	if (self->pos < 2) {
		g_set_error_literal(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "stack is not full enough");
		return FALSE;
	}
	if (opcode1_out != NULL)
		*opcode1_out = self->opcodes[self->pos - 1];
	if (opcode2_out != NULL)
		*opcode2_out = self->opcodes[self->pos - 2];
	self->pos -= 2;
	return TRUE;
}

/**
 * xb_stack_peek:
 * @self: a #XbStack
 * @idx: index
 *
 * Peeks an opcode from the stack.
 *
 * Returns: (transfer none): a #XbOpcode
 *
 * Since: 0.1.3
 **/
XbOpcode *
xb_stack_peek(XbStack *self, guint idx)
{
	if (idx >= self->pos)
		return NULL;
	return &self->opcodes[idx];
}

/* private */
gboolean
xb_stack_push_bool(XbStack *self, gboolean val, GError **error)
{
	XbOpcode *op;
	if (!xb_stack_push(self, &op, error))
		return FALSE;
	xb_opcode_bool_init(op, val);
	return TRUE;
}

/* private */
XbOpcode *
xb_stack_peek_head(XbStack *self)
{
	if (self->pos == 0)
		return NULL;
	return &self->opcodes[0];
}

/* private */
XbOpcode *
xb_stack_peek_tail(XbStack *self)
{
	if (self->pos == 0)
		return NULL;
	return &self->opcodes[self->pos - 1];
}

/**
 * xb_stack_push:
 * @self: a #XbStack
 * @opcode_out: (out) (nullable): return location for the new #XbOpcode
 * @error: a #GError, or %NULL
 *
 * Pushes a new empty opcode onto the end of the stack. A pointer to the opcode
 * is returned in @opcode_out so that the caller can initialise it. This must be
 * done before the stack is next used as, for performance reasons, the newly
 * pushed opcode is not zero-initialised.
 *
 * Returns: %TRUE if a new empty opcode was returned, or %FALSE if the stack has
 *    reached its maximum size
 * Since: 0.2.0
 **/
gboolean
xb_stack_push(XbStack *self, XbOpcode **opcode_out, GError **error)
{
	if (self->pos >= self->max_size) {
		*opcode_out = NULL;
		g_set_error(error,
			    G_IO_ERROR,
			    G_IO_ERROR_NO_SPACE,
			    "stack is already at maximum size of %u",
			    self->max_size);
		return FALSE;
	}

	*opcode_out = &self->opcodes[self->pos++];
	return TRUE;
}

/**
 * xb_stack_get_size:
 * @self: a #XbStack
 *
 * Gets the current size of the stack.
 *
 * Returns: integer, where 0 is "empty"
 *
 * Since: 0.1.3
 **/
guint
xb_stack_get_size(XbStack *self)
{
	return self->pos;
}

/**
 * xb_stack_get_max_size:
 * @self: a #XbStack
 *
 * Gets the maximum size of the stack.
 *
 * Returns: integer
 *
 * Since: 0.1.3
 **/
guint
xb_stack_get_max_size(XbStack *self)
{
	return self->max_size;
}

/**
 * xb_stack_to_string:
 * @self: a #XbStack
 *
 * Returns a string representing a stack.
 *
 * Returns: text
 *
 * Since: 0.1.4
 **/
gchar *
xb_stack_to_string(XbStack *self)
{
	GString *str = g_string_new(NULL);
	for (guint i = 0; i < self->pos; i++) {
		g_autofree gchar *tmp = xb_opcode_to_string(&self->opcodes[i]);
		g_string_append_printf(str, "%s,", tmp);
	}
	if (str->len > 0)
		g_string_truncate(str, str->len - 1);
	return g_string_free(str, FALSE);
}

/**
 * xb_stack_new:
 * @max_size: maximum size of the stack
 *
 * Creates a stack for the XbMachine request. Only #XbOpcode's can be pushed and
 * popped from the stack.
 *
 * Unlike with xb_stack_new_inline(), this stack will be allocated on the heap.
 *
 * Returns: (transfer full): a #XbStack
 *
 * Since: 0.1.3
 **/
XbStack *
xb_stack_new(guint max_size)
{
	XbStack *self = g_malloc(sizeof(XbStack) + max_size * sizeof(XbOpcode));
	self->ref = 1;
	self->stack_allocated = FALSE;
	self->pos = 0;
	self->max_size = max_size;
	return self;
}

GType
xb_stack_get_type(void)
{
	static GType type = 0;
	if (G_UNLIKELY(!type)) {
		type = g_boxed_type_register_static("XbStack",
						    (GBoxedCopyFunc)xb_stack_ref,
						    (GBoxedFreeFunc)xb_stack_unref);
	}
	return type;
}
