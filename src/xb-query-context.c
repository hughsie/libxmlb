/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 * vi:set noexpandtab tabstop=8 shiftwidth=8:
 *
 * Copyright (C) 2020 Endless OS Foundation LLC
 *
 * Author: Philip Withnall <withnall@endlessm.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbQueryContext"

#include "config.h"

#include <glib.h>

#include "xb-query.h"
#include "xb-query-context.h"
#include "xb-value-bindings.h"


/* Why are #XbQueryContext and #XbValueBindings not the same object?
 * #XbQueryContext is associated with a query, but the #XbValueBindings is
 * associated with a query *and* the #XbMachine which runs for it. Once an
 * #XbQuery is turned into an #XbMachine to be evaluated, the #XbQueryContext is
 * ignored and only the #XbValueBindings are taken forward to be used, copied
 * and subsetted for various parts of the #XbMachine. */
typedef struct {
	guint limit;
	XbQueryFlags flags;
	XbValueBindings bindings;
	gpointer dummy[5];
} RealQueryContext;

G_STATIC_ASSERT (sizeof (XbQueryContext) == sizeof (RealQueryContext));
#if GLIB_CHECK_VERSION(2, 60, 0)
G_STATIC_ASSERT (G_ALIGNOF (XbQueryContext) == G_ALIGNOF (RealQueryContext));
#endif

G_DEFINE_BOXED_TYPE (XbQueryContext, xb_query_context,
		     xb_query_context_copy, xb_query_context_free)

/**
 * xb_query_context_init:
 * @self: an uninitialised #XbQueryContext to initialise
 *
 * Initialise a stack-allocated #XbQueryContext struct so it can be used.
 *
 * Stack-allocated #XbQueryContext instances should be freed once finished
 * with, using xb_query_context_clear() (or `g_auto(XbQueryContext)`, which is
 * equivalent).
 *
 * Since: 0.3.0
 */
void
xb_query_context_init (XbQueryContext *self)
{
	RealQueryContext *_self = (RealQueryContext *) self;

	_self->limit = 0;
	_self->flags = XB_QUERY_FLAG_NONE;
	xb_value_bindings_init (&_self->bindings);
}

/**
 * xb_query_context_clear:
 * @self: an #XbQueryContext
 *
 * Clear an #XbQueryContext, freeing any allocated memory it points to.
 *
 * After this function has been called, the contents of the #XbQueryContext are
 * undefined, and it’s only safe to call xb_query_context_init() on it.
 *
 * Since: 0.3.0
 */
void
xb_query_context_clear (XbQueryContext *self)
{
	RealQueryContext *_self = (RealQueryContext *) self;

	xb_value_bindings_clear (&_self->bindings);
}

/**
 * xb_query_context_copy:
 * @self: an #XbQueryContext
 *
 * Copy @self into a new heap-allocated #XbQueryContext instance.
 *
 * Returns: (transfer full): a copy of @self
 * Since: 0.3.0
 */
XbQueryContext *
xb_query_context_copy (XbQueryContext *self)
{
	RealQueryContext *_self = (RealQueryContext *) self;
	g_autoptr(XbQueryContext) copy = g_new0 (XbQueryContext, 1);
	RealQueryContext *_copy = (RealQueryContext *) copy;
	gsize i = 0;

	xb_query_context_init (copy);

	_copy->limit = _self->limit;
	_copy->flags = _self->flags;

	while (xb_value_bindings_copy_binding (&_self->bindings, i, &_copy->bindings, i))
		i++;

	return g_steal_pointer (&copy);
}

/**
 * xb_query_context_free:
 * @self: a heap-allocated #XbQueryContext
 *
 * Free a heap-allocated #XbQueryContext instance. This should be used on
 * #XbQueryContext instances created with xb_query_context_copy().
 *
 * For stack-allocated instances, xb_query_context_clear() should be used
 * instead.
 *
 * Since: 0.3.0
 */
void
xb_query_context_free (XbQueryContext *self)
{
	g_return_if_fail (self != NULL);

	xb_query_context_clear (self);
	g_free (self);
}

/**
 * xb_query_context_get_bindings:
 * @self: an #XbQueryContext
 *
 * Get the #XbValueBindings for this query context.
 *
 * Returns: (transfer none) (not nullable): bindings
 * Since: 0.3.0
 */
XbValueBindings *
xb_query_context_get_bindings (XbQueryContext *self)
{
	RealQueryContext *_self = (RealQueryContext *) self;

	g_return_val_if_fail (self != NULL, NULL);

	return &_self->bindings;
}

/**
 * xb_query_context_get_limit:
 * @self: an #XbQueryContext
 *
 * Get the limit on the number of query results. See
 * xb_query_context_set_limit().
 *
 * Returns: limit on results, or `0` if unlimited
 * Since: 0.3.0
 */
guint
xb_query_context_get_limit (XbQueryContext *self)
{
	RealQueryContext *_self = (RealQueryContext *) self;

	g_return_val_if_fail (self != NULL, 0);

	return _self->limit;
}

/**
 * xb_query_context_set_limit:
 * @self: an #XbQueryContext
 * @limit: number of query results to return, or `0` for unlimited
 *
 * Set the limit on the number of results to return from the query.
 *
 * Since: 0.3.0
 */
void
xb_query_context_set_limit (XbQueryContext *self, guint limit)
{
	RealQueryContext *_self = (RealQueryContext *) self;

	g_return_if_fail (self != NULL);

	_self->limit = limit;
}

/**
 * xb_query_context_get_flags:
 * @self: an #XbQueryContext
 *
 * Get the flags set on the context. See xb_query_context_set_flags().
 *
 * Returns: query flags
 * Since: 0.3.0
 */
XbQueryFlags
xb_query_context_get_flags (XbQueryContext *self)
{
	RealQueryContext *_self = (RealQueryContext *) self;

	g_return_val_if_fail (self != NULL, XB_QUERY_FLAG_NONE);

	return _self->flags;
}

/**
 * xb_query_context_set_flags:
 * @self: an #XbQueryContext
 * @flags: query flags, or %XB_QUERY_FLAG_NONE for none
 *
 * Set flags which affect the behaviour of the query.
 *
 * Since: 0.3.0
 */
void
xb_query_context_set_flags (XbQueryContext *self, XbQueryFlags flags)
{
	RealQueryContext *_self = (RealQueryContext *) self;

	g_return_if_fail (self != NULL);

	_self->flags = flags;
}
