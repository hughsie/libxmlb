/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * XbOpcodeFlags:
 * @XB_OPCODE_FLAG_NONE:			No flags set
 * @XB_OPCODE_FLAG_INTEGER:			Integer value set
 * @XB_OPCODE_FLAG_TEXT:			Text value set
 * @XB_OPCODE_FLAG_FUNCTION:			An operator
 * @XB_OPCODE_FLAG_BOUND:			A bound value, assigned later
 *
 * The opcode flags. The values have been carefully chosen so that a simple
 * bitmask can be done to know how to compare for equality.
 *
 * function─┐ ┌─string
 * bound──┐ │ │ ┌──integer
 *        │ │ │ │
 *  X X X X X X X
 *        8 4 2 1
 **/
typedef enum {
	XB_OPCODE_FLAG_UNKNOWN		= 0x0,		/* Since: 0.1.4 */
	XB_OPCODE_FLAG_INTEGER		= 1 << 0,	/* Since: 0.1.4 */
	XB_OPCODE_FLAG_TEXT		= 1 << 1,	/* Since: 0.1.4 */
	XB_OPCODE_FLAG_FUNCTION		= 1 << 2,	/* Since: 0.1.4 */
	XB_OPCODE_FLAG_BOUND		= 1 << 3,	/* Since: 0.1.4 */
	XB_OPCODE_FLAG_BOOLEAN		= 1 << 4,	/* Since: 0.1.11 */
	/*< private >*/
	XB_OPCODE_FLAG_LAST
} XbOpcodeFlags;

/**
 * XbOpcodeKind:
 * @XB_OPCODE_KIND_UNKNOWN:			Unknown opcode
 * @XB_OPCODE_KIND_INTEGER:			A literal integer value
 * @XB_OPCODE_KIND_TEXT:			A literal text value
 * @XB_OPCODE_KIND_FUNCTION:			An operator
 * @XB_OPCODE_KIND_BOUND_INTEGER:		A bound integer value
 * @XB_OPCODE_KIND_BOUND_TEXT:			A bound text value
 * @XB_OPCODE_KIND_INDEXED_TEXT:		An indexed text value
 **/
typedef enum {
	XB_OPCODE_KIND_UNKNOWN		= 0x0,							/* Since: 0.1.1 */
	XB_OPCODE_KIND_INTEGER		= XB_OPCODE_FLAG_INTEGER,				/* Since: 0.1.1 */
	XB_OPCODE_KIND_TEXT		= XB_OPCODE_FLAG_TEXT,					/* Since: 0.1.1 */
	XB_OPCODE_KIND_FUNCTION		= XB_OPCODE_FLAG_FUNCTION | XB_OPCODE_FLAG_INTEGER,	/* Since: 0.1.1 */
	XB_OPCODE_KIND_BOUND_UNSET	= XB_OPCODE_FLAG_BOUND,					/* Since: 0.1.4 */
	XB_OPCODE_KIND_BOUND_INTEGER	= XB_OPCODE_FLAG_BOUND | XB_OPCODE_FLAG_INTEGER,	/* Since: 0.1.4 */
	XB_OPCODE_KIND_BOUND_TEXT	= XB_OPCODE_FLAG_BOUND | XB_OPCODE_FLAG_TEXT,		/* Since: 0.1.4 */
	XB_OPCODE_KIND_INDEXED_TEXT	= XB_OPCODE_FLAG_INTEGER | XB_OPCODE_FLAG_TEXT,		/* Since: 0.1.4 */
	XB_OPCODE_KIND_BOOLEAN		= XB_OPCODE_FLAG_INTEGER | XB_OPCODE_FLAG_BOOLEAN,	/* Since: 0.1.11 */
	/*< private >*/
	XB_OPCODE_KIND_LAST
} XbOpcodeKind;

typedef struct _XbOpcode XbOpcode;

gboolean	 xb_opcode_cmp_val		(XbOpcode	*self);
gboolean	 xb_opcode_cmp_str		(XbOpcode	*self);

GType		 xb_opcode_get_type		(void);
gchar		*xb_opcode_to_string		(XbOpcode	*self);
const gchar	*xb_opcode_kind_to_string	(XbOpcodeKind	 kind);
XbOpcodeKind	 xb_opcode_kind_from_string	(const gchar	*str);

G_DEPRECATED_FOR(xb_opcode_clear)
void		 xb_opcode_unref		(XbOpcode	*self) G_GNUC_NORETURN;
G_DEPRECATED
XbOpcode	*xb_opcode_ref			(XbOpcode	*self) G_GNUC_NORETURN;

XbOpcodeKind	 xb_opcode_get_kind		(XbOpcode	*self);
const gchar	*xb_opcode_get_str		(XbOpcode	*self);
guint32		 xb_opcode_get_val		(XbOpcode	*self);

void		 xb_opcode_func_init		(XbOpcode	*opcode,
						 guint32	 func);
G_DEPRECATED_FOR(xb_opcode_func_init)
XbOpcode	*xb_opcode_func_new		(guint32	 func) G_GNUC_NORETURN;
void		 xb_opcode_integer_init		(XbOpcode	*opcode,
						 guint32	 val);
G_DEPRECATED_FOR(xb_opcode_integer_init)
XbOpcode	*xb_opcode_integer_new		(guint32	 val) G_GNUC_NORETURN;
void		 xb_opcode_text_init		(XbOpcode	*opcode,
						 const gchar	*str);
G_DEPRECATED_FOR(xb_opcode_text_init)
XbOpcode	*xb_opcode_text_new		(const gchar	*str) G_GNUC_NORETURN;
void		 xb_opcode_text_init_static	(XbOpcode	*opcode,
						 const gchar	*str);
G_DEPRECATED_FOR(xb_opcode_text_init_static)
XbOpcode	*xb_opcode_text_new_static	(const gchar	*str) G_GNUC_NORETURN;
void		 xb_opcode_text_init_steal	(XbOpcode	*opcode,
						 gchar		*str);
G_DEPRECATED_FOR(xb_opcode_text_init_steal)
XbOpcode	*xb_opcode_text_new_steal	(gchar		*str) G_GNUC_NORETURN;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XbOpcode, xb_opcode_unref)

G_END_DECLS
