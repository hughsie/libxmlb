/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_OPCODE_H
#define __XB_OPCODE_H

G_BEGIN_DECLS

#include <glib-object.h>

/**
 * XbOpcodeKind:
 * @XB_OPCODE_KIND_UNKNOWN:			Unknown opcode
 * @XB_OPCODE_KIND_FUNCTION:			An operator
 * @XB_OPCODE_KIND_TEXT:			A text operand
 * @XB_OPCODE_KIND_INTEGER:			An integer operand
 *
 * The kinds of opcode.
 **/
typedef enum {
	XB_OPCODE_KIND_UNKNOWN,
	XB_OPCODE_KIND_FUNCTION,
	XB_OPCODE_KIND_TEXT,
	XB_OPCODE_KIND_INTEGER,
	/*< private >*/
	XB_OPCODE_KIND_LAST
} XbOpcodeKind;

typedef struct _XbOpcode XbOpcode;

GType		 xb_opcode_get_type		(void);
const gchar	*xb_opcode_kind_to_string	(XbOpcodeKind	 kind);
XbOpcodeKind	 xb_opcode_kind_from_string	(const gchar	*str);

void		 xb_opcode_unref		(XbOpcode	*self);
XbOpcode	*xb_opcode_ref			(XbOpcode	*self);

XbOpcodeKind	 xb_opcode_get_kind		(XbOpcode	*self);
const gchar	*xb_opcode_get_str		(XbOpcode	*self);
guint32		 xb_opcode_get_val		(XbOpcode	*self);

XbOpcode	*xb_opcode_func_new		(guint32	 func);
XbOpcode	*xb_opcode_integer_new		(guint32	 val);
XbOpcode	*xb_opcode_text_new		(const gchar	*str);
XbOpcode	*xb_opcode_text_new_static	(const gchar	*str);
XbOpcode	*xb_opcode_text_new_steal	(gchar		*str);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XbOpcode, xb_opcode_unref)

G_END_DECLS

#endif /* __XB_OPCODE_H */
