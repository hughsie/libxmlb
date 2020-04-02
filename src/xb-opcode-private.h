/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "xb-opcode.h"

G_BEGIN_DECLS

struct _XbOpcode {
	XbOpcodeKind	 kind;
	guint32		 val;
	gpointer	 ptr;
	GDestroyNotify	 destroy_func;
};

#define XB_OPCODE_INIT() { 0, 0, NULL, NULL }

/**
 * xb_opcode_steal:
 * @op_ptr: (transfer full): pointer to an #XbOpcode to steal
 *
 * Steal the stack-allocated #XbOpcode pointed to by @op_ptr, returning its
 * value and clearing its previous storage location using `memset()`.
 *
 * Returns: the value of @op_ptr
 * Since: 0.2.0
 */
static inline XbOpcode
xb_opcode_steal (XbOpcode *op_ptr)
{
	XbOpcode op = *op_ptr;
	memset (op_ptr, 0, sizeof (XbOpcode));
	return op;
}

void		 xb_opcode_init			(XbOpcode	*opcode,
						 XbOpcodeKind	 kind,
						 const gchar	*str,
						 guint32	 val,
						 GDestroyNotify	 destroy_func);
void		 xb_opcode_clear		(XbOpcode	*opcode);
void		 xb_opcode_bind_init		(XbOpcode	*opcode);
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
void		 xb_opcode_bool_init		(XbOpcode	*opcode,
						 gboolean	 val);

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (XbOpcode, xb_opcode_clear)

G_END_DECLS
