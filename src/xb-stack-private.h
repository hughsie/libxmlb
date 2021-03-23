/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-opcode-private.h"
#include "xb-stack.h"

G_BEGIN_DECLS

/* Members of this struct should not be accessed directly — use the accessor
 * functions below instead. */
struct _XbStack {
	/*< private >*/
	gint		 ref;
	gboolean	 stack_allocated;	/* whether this XbStack was allocated with alloca() */
	guint		 pos;	/* index of the next unused entry in .opcodes */
	guint		 max_size;
	XbOpcode	 opcodes[];	/* allocated as part of XbStack */
};

/**
 * xb_stack_new_inline:
 * @max_stack_size: maximum size of the stack
 *
 * Creates a stack for the XbMachine request. Only #XbOpcodes can be pushed and
 * popped from the stack.
 *
 * The stack will be allocated on the current C stack frame, so @max_stack_size
 * should be chosen to not overflow the C process’ stack.
 *
 * Returns: (transfer full): a #XbStack
 *
 * Since: 0.3.1
 **/
#define xb_stack_new_inline(max_stack_size) \
(G_GNUC_EXTENSION ({ \
	/* This function has to be static inline so we can use g_alloca(), which \
	 * is needed for performance reasons — about 3 million XbStacks are \
	 * allocated while starting gnome-software. */ \
	guint xsni_max_size = (max_stack_size); \
	XbStack *xsni_stack = g_alloca (sizeof(XbStack) + xsni_max_size * sizeof(XbOpcode)); \
	xsni_stack->ref = 1; \
	xsni_stack->stack_allocated = TRUE; \
	xsni_stack->pos = 0; \
	xsni_stack->max_size = xsni_max_size; \
	(XbStack *) xsni_stack; \
}))

XbStack		*xb_stack_new			(guint		 max_size);
void		 xb_stack_unref			(XbStack	*self);
XbStack		*xb_stack_ref			(XbStack	*self);
guint		 xb_stack_get_size		(XbStack	*self);
guint		 xb_stack_get_max_size		(XbStack	*self);
gboolean	 xb_stack_pop_two		(XbStack	*self,
						 XbOpcode	*opcode1_out,
						 XbOpcode	*opcode2_out,
						 GError		**error);
gboolean	 xb_stack_push_bool		(XbStack	*self,
						 gboolean	 val,
						 GError		**error);
XbOpcode	*xb_stack_peek			(XbStack	*self,
						 guint		 idx);
XbOpcode	*xb_stack_peek_head		(XbStack	*self);
XbOpcode	*xb_stack_peek_tail		(XbStack	*self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XbStack, xb_stack_unref)

G_END_DECLS
