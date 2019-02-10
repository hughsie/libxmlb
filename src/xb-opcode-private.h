/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "xb-opcode.h"

G_BEGIN_DECLS

XbOpcode	*xb_opcode_new			(XbOpcodeKind	 kind,
						 const gchar	*str,
						 guint32	 val,
						 GDestroyNotify	 destroy_func);
XbOpcode	*xb_opcode_bind_new		(void);
gboolean	 xb_opcode_is_bound		(XbOpcode	*self);
void		 xb_opcode_bind_str		(XbOpcode	*self,
						 gchar		*str,
						 GDestroyNotify	 destroy_func);
void		 xb_opcode_bind_val		(XbOpcode	*self,
						 guint32	 val);
void		 xb_opcode_set_kind		(XbOpcode	*self,
						 XbOpcodeKind	 kind);
void		 xb_opcode_set_val		(XbOpcode	*self,
						 guint32	 val);
gchar		*xb_opcode_get_sig		(XbOpcode	*self);

G_END_DECLS
