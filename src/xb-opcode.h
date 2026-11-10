/*
 * Copyright 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "xb-compile.h"

G_BEGIN_DECLS

/**
 * XbOpcodeFlags:
 *
 * The opcode flags. The values have been carefully chosen so that a simple
 * bitmask can be done to know how to compare for equality.
 *
 * function─┐ ┌─string
 * bound──┐ │ │ ┌──integer
 * token┐ │ │ │ │
 *  X X X X X X X
 *        8 4 2 1
 *
 * Since: 0.1.4
 **/
typedef enum {
	/**
	 * XB_OPCODE_FLAG_UNKNOWN:
	 *
	 * No flags set.
	 *
	 * Since: 0.1.4
	 **/
	XB_OPCODE_FLAG_UNKNOWN = 0x0,
	/**
	 * XB_OPCODE_FLAG_INTEGER:
	 *
	 * Integer value set.
	 *
	 * Since: 0.1.4
	 **/
	XB_OPCODE_FLAG_INTEGER = 1 << 0,
	/**
	 * XB_OPCODE_FLAG_TEXT:
	 *
	 * Text value set.
	 *
	 * Since: 0.1.4
	 **/
	XB_OPCODE_FLAG_TEXT = 1 << 1,
	/**
	 * XB_OPCODE_FLAG_FUNCTION:
	 *
	 * An operator.
	 *
	 * Since: 0.1.4
	 **/
	XB_OPCODE_FLAG_FUNCTION = 1 << 2,
	/**
	 * XB_OPCODE_FLAG_BOUND:
	 *
	 * A bound value, assigned later.
	 *
	 * Since: 0.1.4
	 **/
	XB_OPCODE_FLAG_BOUND = 1 << 3,
	/**
	 * XB_OPCODE_FLAG_BOOLEAN:
	 *
	 * Boolean value set.
	 *
	 * Since: 0.1.11
	 **/
	XB_OPCODE_FLAG_BOOLEAN = 1 << 4,
	/**
	 * XB_OPCODE_FLAG_TOKENIZED:
	 *
	 * Tokenized text.
	 *
	 * Since: 0.3.1
	 **/
	XB_OPCODE_FLAG_TOKENIZED = 1 << 5,
	/*< private >*/
	XB_OPCODE_FLAG_LAST
} XbOpcodeFlags;

/**
 * XbOpcodeKind:
 *
 * The kind of opcode.
 *
 * Since: 0.1.1
 **/
typedef enum {
	/**
	 * XB_OPCODE_KIND_UNKNOWN:
	 *
	 * Unknown opcode.
	 *
	 * Since: 0.1.1
	 **/
	XB_OPCODE_KIND_UNKNOWN = 0x0,
	/**
	 * XB_OPCODE_KIND_INTEGER:
	 *
	 * A literal integer value.
	 *
	 * Since: 0.1.1
	 **/
	XB_OPCODE_KIND_INTEGER = XB_OPCODE_FLAG_INTEGER,
	/**
	 * XB_OPCODE_KIND_TEXT:
	 *
	 * A literal text value.
	 *
	 * Since: 0.1.1
	 **/
	XB_OPCODE_KIND_TEXT = XB_OPCODE_FLAG_TEXT,
	/**
	 * XB_OPCODE_KIND_FUNCTION:
	 *
	 * An operator.
	 *
	 * Since: 0.1.1
	 **/
	XB_OPCODE_KIND_FUNCTION = XB_OPCODE_FLAG_FUNCTION | XB_OPCODE_FLAG_INTEGER,
	/**
	 * XB_OPCODE_KIND_BOUND_UNSET:
	 *
	 * A bound value, not yet assigned.
	 *
	 * Since: 0.1.4
	 **/
	XB_OPCODE_KIND_BOUND_UNSET = XB_OPCODE_FLAG_BOUND,
	/**
	 * XB_OPCODE_KIND_BOUND_INTEGER:
	 *
	 * A bound integer value.
	 *
	 * Since: 0.1.4
	 **/
	XB_OPCODE_KIND_BOUND_INTEGER = XB_OPCODE_FLAG_BOUND | XB_OPCODE_FLAG_INTEGER,
	/**
	 * XB_OPCODE_KIND_BOUND_TEXT:
	 *
	 * A bound text value.
	 *
	 * Since: 0.1.4
	 **/
	XB_OPCODE_KIND_BOUND_TEXT = XB_OPCODE_FLAG_BOUND | XB_OPCODE_FLAG_TEXT,
	/**
	 * XB_OPCODE_KIND_INDEXED_TEXT:
	 *
	 * An indexed text value.
	 *
	 * Since: 0.1.4
	 **/
	XB_OPCODE_KIND_INDEXED_TEXT = XB_OPCODE_FLAG_INTEGER | XB_OPCODE_FLAG_TEXT,
	/**
	 * XB_OPCODE_KIND_BOOLEAN:
	 *
	 * A boolean value.
	 *
	 * Since: 0.1.11
	 **/
	XB_OPCODE_KIND_BOOLEAN = XB_OPCODE_FLAG_INTEGER | XB_OPCODE_FLAG_BOOLEAN,
	/**
	 * XB_OPCODE_KIND_BOUND_INDEXED_TEXT:
	 *
	 * A bound indexed text value.
	 *
	 * Since: 0.3.12
	 **/
	XB_OPCODE_KIND_BOUND_INDEXED_TEXT =
	    XB_OPCODE_FLAG_BOUND | XB_OPCODE_FLAG_INTEGER | XB_OPCODE_FLAG_TEXT,
	/*< private >*/
	XB_OPCODE_KIND_LAST
} XbOpcodeKind;

typedef struct _XbOpcode XbOpcode;

gboolean
xb_opcode_cmp_val(XbOpcode *self) G_GNUC_NON_NULL(1);
gboolean
xb_opcode_cmp_str(XbOpcode *self) G_GNUC_NON_NULL(1);

gchar *
xb_opcode_to_string(XbOpcode *self) G_GNUC_NON_NULL(1);
const gchar *
xb_opcode_kind_to_string(XbOpcodeKind kind);
XbOpcodeKind
xb_opcode_kind_from_string(const gchar *str);

XbOpcodeKind
xb_opcode_get_kind(XbOpcode *self) G_GNUC_NON_NULL(1);
const gchar *
xb_opcode_get_str(XbOpcode *self) G_GNUC_NON_NULL(1);
guint32
xb_opcode_get_val(XbOpcode *self) G_GNUC_NON_NULL(1);

void
xb_opcode_func_init(XbOpcode *self, guint32 func) G_GNUC_NON_NULL(1);
void
xb_opcode_integer_init(XbOpcode *self, guint32 val) G_GNUC_NON_NULL(1);
void
xb_opcode_text_init(XbOpcode *self, const gchar *str) G_GNUC_NON_NULL(1);
void
xb_opcode_text_init_static(XbOpcode *self, const gchar *str) G_GNUC_NON_NULL(1);
void
xb_opcode_text_init_steal(XbOpcode *self, gchar *str) G_GNUC_NON_NULL(1);

G_END_DECLS
