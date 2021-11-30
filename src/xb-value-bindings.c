/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 * vi:set noexpandtab tabstop=8 shiftwidth=8:
 *
 * Copyright (C) 2020 Endless OS Foundation LLC
 *
 * Author: Philip Withnall <withnall@endlessm.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbValueBindings"

#include "config.h"

#include <glib.h>

#include "xb-opcode-private.h"
#include "xb-value-bindings.h"

typedef struct {
	enum {
		KIND_NONE,
		KIND_TEXT,
		KIND_INTEGER,
	} kind;
	union {
		gchar *text;
		guint32 integer;
	};
	GDestroyNotify destroy_func;
} BoundValue;

typedef struct {
	/* Currently limited to 4 values since that’s all that any client
	 * uses. This could be expanded to dynamically allow more in future. */
	BoundValue values[4];
	gpointer dummy[3];
} RealValueBindings;

G_STATIC_ASSERT (sizeof (XbValueBindings) == sizeof (RealValueBindings));

G_DEFINE_BOXED_TYPE (XbValueBindings, xb_value_bindings,
		     xb_value_bindings_copy, xb_value_bindings_free)

/**
 * xb_value_bindings_init:
 * @self: an uninitialised #XbValueBindings to initialise
 *
 * Initialise a stack-allocated #XbValueBindings struct so it can be used.
 *
 * Stack-allocated #XbValueBindings instances should be freed once finished
 * with, using xb_value_bindings_clear() (or `g_auto(XbValueBindings)`, which is
 * equivalent).
 *
 * Since: 0.3.0
 */
void
xb_value_bindings_init (XbValueBindings *self)
{
	RealValueBindings *_self = (RealValueBindings *) self;

	for (gsize i = 0; i < G_N_ELEMENTS (_self->values); i++)
		_self->values[i].kind = KIND_NONE;
}

static void
xb_value_bindings_clear_index (XbValueBindings *self, guint idx)
{
	RealValueBindings *_self = (RealValueBindings *) self;

	g_return_if_fail (idx < G_N_ELEMENTS (_self->values));

	if (_self->values[idx].kind == KIND_TEXT && _self->values[idx].destroy_func)
		_self->values[idx].destroy_func (_self->values[idx].text);
	_self->values[idx].kind = KIND_NONE;
	_self->values[idx].text = NULL;
	_self->values[idx].destroy_func = NULL;
}

/**
 * xb_value_bindings_clear:
 * @self: an #XbValueBindings
 *
 * Clear an #XbValueBindings, freeing any allocated memory it points to.
 *
 * After this function has been called, the contents of the #XbValueBindings are
 * undefined, and it’s only safe to call xb_value_bindings_init() on it.
 *
 * Since: 0.3.0
 */
void
xb_value_bindings_clear (XbValueBindings *self)
{
	RealValueBindings *_self = (RealValueBindings *) self;

	for (gsize i = 0; i < G_N_ELEMENTS (_self->values); i++)
		xb_value_bindings_clear_index (self, i);
}

/**
 * xb_value_bindings_copy:
 * @self: an #XbValueBindings
 *
 * Copy @self into a new heap-allocated #XbValueBindings instance.
 *
 * Returns: (transfer full): a copy of @self
 * Since: 0.3.0
 */
XbValueBindings *
xb_value_bindings_copy (XbValueBindings *self)
{
	RealValueBindings *_self = (RealValueBindings *) self;
	g_autoptr(XbValueBindings) copy = g_new0 (XbValueBindings, 1);

	xb_value_bindings_init (copy);

	for (gsize i = 0; i < G_N_ELEMENTS (_self->values); i++) {
		gboolean copied = xb_value_bindings_copy_binding (self, i, copy, i);
		g_assert (copied);
	}

	return g_steal_pointer (&copy);
}

/**
 * xb_value_bindings_free:
 * @self: a heap-allocated #XbValueBindings
 *
 * Free a heap-allocated #XbValueBindings instance. This should be used on
 * #XbValueBindings instances created with xb_value_bindings_copy().
 *
 * For stack-allocated instances, xb_value_bindings_clear() should be used
 * instead.
 *
 * Since: 0.3.0
 */
void
xb_value_bindings_free (XbValueBindings *self)
{
	g_return_if_fail (self != NULL);

	xb_value_bindings_clear (self);
	g_free (self);
}

/**
 * xb_value_bindings_is_bound:
 * @self: an #XbValueBindings
 * @idx: 0-based index of the binding to check
 *
 * Check whether a value has been bound to the given index using (for example)
 * xb_value_bindings_bind_str().
 *
 * Returns: %TRUE if a value is bound to @idx, %FALSE otherwise
 * Since: 0.3.0
 */
gboolean
xb_value_bindings_is_bound (XbValueBindings *self, guint idx)
{
	RealValueBindings *_self = (RealValueBindings *) self;

	return (idx < G_N_ELEMENTS (_self->values) && _self->values[idx].kind != KIND_NONE);
}

/**
 * xb_value_bindings_bind_str:
 * @self: an #XbValueBindings
 * @idx: 0-based index to bind to
 * @str: (transfer full) (not nullable): a string to bind to @idx
 * @destroy_func: (nullable): function to free @str
 *
 * Bind @str to @idx in the value bindings.
 *
 * This will overwrite any previous binding at @idx. It will take ownership of
 * @str, and an appropriate @destroy_func must be provided to free @str once the
 * binding is no longer needed. @destroy_func will be called exactly once at
 * some point before the #XbValueBindings is cleared or freed.
 *
 * Since: 0.3.0
 */
void
xb_value_bindings_bind_str (XbValueBindings *self, guint idx, const gchar *str, GDestroyNotify destroy_func)
{
	RealValueBindings *_self = (RealValueBindings *) self;

	g_return_if_fail (self != NULL);
	g_return_if_fail (str != NULL);

	/* Currently limited to two values, but this restriction could be lifted
	 * in future. */
	g_return_if_fail (idx < G_N_ELEMENTS (_self->values));

	xb_value_bindings_clear_index (self, idx);

	_self->values[idx].kind = KIND_TEXT;
	_self->values[idx].text = (gchar *) str;
	_self->values[idx].destroy_func = destroy_func;
}

/**
 * xb_value_bindings_bind_val:
 * @self: an #XbValueBindings
 * @idx: 0-based index to bind to
 * @val: an integer to bind to @idx
 *
 * Bind @val to @idx in the value bindings.
 *
 * This will overwrite any previous binding at @idx.
 *
 * Since: 0.3.0
 */
void
xb_value_bindings_bind_val (XbValueBindings *self, guint idx, guint32 val)
{
	RealValueBindings *_self = (RealValueBindings *) self;

	g_return_if_fail (self != NULL);

	/* Currently limited to two values, but this restriction could be lifted
	 * in future. */
	g_return_if_fail (idx < G_N_ELEMENTS (_self->values));

	xb_value_bindings_clear_index (self, idx);

	_self->values[idx].kind = KIND_INTEGER;
	_self->values[idx].integer = val;
	_self->values[idx].destroy_func = NULL;
}

/**
 * xb_value_bindings_lookup_opcode:
 * @self: an #XbValueBindings
 * @idx: 0-based index to look up the binding from
 * @opcode_out: (out caller-allocates) (not nullable): pointer to an #XbOpcode
 *     to initialise from the binding
 *
 * Initialises an #XbOpcode with the value bound to @idx, if a value is bound.
 * If no value is bound, @opcode_out is not touched and %FALSE is returned.
 *
 * @opcode_out is initialised to point to the data inside the #XbValueBindings,
 * so must have a shorter lifetime than the #XbValueBindings. It will be of kind
 * %XB_OPCODE_KIND_BOUND_TEXT or %XB_OPCODE_KIND_BOUND_INTEGER.
 *
 * Returns: %TRUE if @idx was bound, %FALSE otherwise
 * Since: 0.3.0
 */
gboolean
xb_value_bindings_lookup_opcode (XbValueBindings *self, guint idx, XbOpcode *opcode_out)
{
	RealValueBindings *_self = (RealValueBindings *) self;

	if (!xb_value_bindings_is_bound (self, idx))
		return FALSE;

	switch (_self->values[idx].kind) {
	case KIND_TEXT:
		xb_opcode_init (opcode_out, XB_OPCODE_KIND_BOUND_TEXT,
				_self->values[idx].text, 0, NULL);
		break;
	case KIND_INTEGER:
		xb_opcode_init (opcode_out, XB_OPCODE_KIND_BOUND_INTEGER, NULL,
				_self->values[idx].integer, NULL);
		break;
	case KIND_NONE:
	default:
		g_assert_not_reached ();
	}

	return TRUE;
}

/**
 * xb_value_bindings_copy_binding:
 * @self: an #XbValueBindings to copy from
 * @idx: 0-based index to look up the binding from in @self
 * @dest: an #XbValueBindings to copy to
 * @dest_idx: 0-based index to copy the binding to in @dest
 *
 * Copies the value bound at @idx on @self to @dest_idx on @dest. If no value is
 * bound at @idx, @dest is not modified and %FALSE is returned.
 *
 * @dest must be initialised. If a binding already exists at @dest_idx, it will
 * be overwritten.
 *
 * Returns: %TRUE if @idx was bound, %FALSE otherwise
 * Since: 0.3.0
 */
gboolean
xb_value_bindings_copy_binding (XbValueBindings *self, guint idx, XbValueBindings *dest, guint dest_idx)
{
	RealValueBindings *_self = (RealValueBindings *) self;

	if (!xb_value_bindings_is_bound (self, idx))
		return FALSE;

	switch (_self->values[idx].kind) {
	case KIND_TEXT:
		xb_value_bindings_bind_str (dest, dest_idx, _self->values[idx].text, NULL);
		break;
	case KIND_INTEGER:
		xb_value_bindings_bind_val (dest, dest_idx, _self->values[idx].integer);
		break;
	case KIND_NONE:
	default:
		g_assert_not_reached ();
	}

	return TRUE;
}
