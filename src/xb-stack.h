/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-opcode.h"

G_BEGIN_DECLS

typedef struct _XbStack XbStack;

GType		 xb_stack_get_type		(void);
gchar		*xb_stack_to_string		(XbStack	*self);
XbOpcode	*xb_stack_pop			(XbStack	*self);
gboolean	 xb_stack_push			(XbStack	*self,
						 XbOpcode	*opcode);
gboolean	 xb_stack_push_steal		(XbStack	*self,
						 XbOpcode	*opcode);

G_END_DECLS
