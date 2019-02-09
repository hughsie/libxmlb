/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

G_BEGIN_DECLS

#include <glib-object.h>

#include "xb-stack.h"

XbStack		*xb_stack_new			(guint		 max_size);
void		 xb_stack_unref			(XbStack	*self);
XbStack		*xb_stack_ref			(XbStack	*self);
guint		 xb_stack_get_size		(XbStack	*self);
guint		 xb_stack_get_max_size		(XbStack	*self);
XbOpcode	*xb_stack_peek			(XbStack	*self,
						 guint		 idx);
XbOpcode	*xb_stack_peek_head		(XbStack	*self);
XbOpcode	*xb_stack_peek_tail		(XbStack	*self);
GPtrArray	*xb_stack_steal_all		(XbStack	*self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XbStack, xb_stack_unref)

G_END_DECLS
