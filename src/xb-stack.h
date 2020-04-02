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
gboolean	 xb_stack_pop			(XbStack	*self,
						 XbOpcode	*opcode_out);
gboolean	 xb_stack_push			(XbStack	*self,
						 XbOpcode	**opcode_out);
G_DEPRECATED_FOR(xb_stack_push)
gboolean	 xb_stack_push_steal		(XbStack	*self,
						 XbOpcode	*opcode) G_GNUC_NORETURN;

G_END_DECLS
