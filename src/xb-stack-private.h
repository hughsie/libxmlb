/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-stack.h"

G_BEGIN_DECLS

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
