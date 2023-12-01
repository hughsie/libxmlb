/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "xb-opcode.h"
#include "xb-stack.h"
#include "xb-value-bindings.h"

G_BEGIN_DECLS

#define XB_TYPE_MACHINE (xb_machine_get_type())
G_DECLARE_DERIVABLE_TYPE(XbMachine, xb_machine, XB, MACHINE, GObject)

struct _XbMachineClass {
	GObjectClass parent_class;
	/*< private >*/
	void (*_xb_reserved1)(void);
	void (*_xb_reserved2)(void);
	void (*_xb_reserved3)(void);
	void (*_xb_reserved4)(void);
	void (*_xb_reserved5)(void);
	void (*_xb_reserved6)(void);
	void (*_xb_reserved7)(void);
};

/**
 * XbMachineDebugFlags:
 * @XB_MACHINE_DEBUG_FLAG_NONE:			No debug flags to use
 * @XB_MACHINE_DEBUG_FLAG_SHOW_STACK:		Show the stack addition and removal
 * @XB_MACHINE_DEBUG_FLAG_SHOW_PARSING:		Show the XPath predicate parsing
 * @XB_MACHINE_DEBUG_FLAG_SHOW_OPTIMIZER:	Show the optimizer operation
 * @XB_MACHINE_DEBUG_FLAG_SHOW_SLOW_PATH:	Show the query slow paths
 *
 * The flags to control the amount of debugging is generated.
 **/
typedef enum {
	XB_MACHINE_DEBUG_FLAG_NONE = 0,
	XB_MACHINE_DEBUG_FLAG_SHOW_STACK = 1 << 0,
	XB_MACHINE_DEBUG_FLAG_SHOW_PARSING = 1 << 1,
	XB_MACHINE_DEBUG_FLAG_SHOW_OPTIMIZER = 1 << 2,
	XB_MACHINE_DEBUG_FLAG_SHOW_SLOW_PATH = 1 << 3,
	/*< private >*/
	XB_MACHINE_DEBUG_FLAG_LAST
} XbMachineDebugFlags;

/**
 * XbMachineParseFlags:
 * @XB_MACHINE_PARSE_FLAG_NONE:			No flags set
 * @XB_MACHINE_PARSE_FLAG_OPTIMIZE:		Run an optimization pass on the predicate
 *
 * The flags to control the parsing behaviour.
 **/
typedef enum {
	XB_MACHINE_PARSE_FLAG_NONE = 0,
	XB_MACHINE_PARSE_FLAG_OPTIMIZE = 1 << 0,
	/*< private >*/
	XB_MACHINE_PARSE_FLAG_LAST
} XbMachineParseFlags;

typedef gboolean (*XbMachineOpcodeFixupFunc)(XbMachine *self,
					     XbStack *opcodes,
					     gpointer user_data,
					     GError **error);
/* when next breaking API add @level here */
typedef gboolean (*XbMachineTextHandlerFunc)(XbMachine *self,
					     XbStack *opcodes,
					     const gchar *text,
					     gboolean *handled,
					     gpointer user_data,
					     GError **error);
/* when next breaking API add @level here */
typedef gboolean (*XbMachineMethodFunc)(XbMachine *self,
					XbStack *stack,
					gboolean *result_unused,
					gpointer exec_data,
					gpointer user_data,
					GError **error);

XbMachine *
xb_machine_new(void);
void
xb_machine_set_debug_flags(XbMachine *self, XbMachineDebugFlags flags) G_GNUC_NON_NULL(1);
G_DEPRECATED_FOR(xb_machine_parse_full)
XbStack *
xb_machine_parse(XbMachine *self, const gchar *text, gssize text_len, GError **error)
    G_GNUC_NON_NULL(1, 2);
XbStack *
xb_machine_parse_full(XbMachine *self,
		      const gchar *text,
		      gssize text_len,
		      XbMachineParseFlags flags,
		      GError **error) G_GNUC_NON_NULL(1, 2);

G_DEPRECATED_FOR(xb_machine_run_with_bindings)
gboolean
xb_machine_run(XbMachine *self,
	       XbStack *opcodes,
	       gboolean *result,
	       gpointer exec_data,
	       GError **error) G_GNUC_NON_NULL(1, 2, 3);
gboolean
xb_machine_run_with_bindings(XbMachine *self,
			     XbStack *opcodes,
			     XbValueBindings *bindings,
			     gboolean *result,
			     gpointer exec_data,
			     GError **error) G_GNUC_NON_NULL(1, 2);

void
xb_machine_add_opcode_fixup(XbMachine *self,
			    const gchar *opcodes_sig,
			    XbMachineOpcodeFixupFunc fixup_cb,
			    gpointer user_data,
			    GDestroyNotify user_data_free) G_GNUC_NON_NULL(1, 2, 3);
void
xb_machine_add_text_handler(XbMachine *self,
			    XbMachineTextHandlerFunc handler_cb,
			    gpointer user_data,
			    GDestroyNotify user_data_free) G_GNUC_NON_NULL(1, 2);
void
xb_machine_add_method(XbMachine *self,
		      const gchar *name,
		      guint n_opcodes,
		      XbMachineMethodFunc method_cb,
		      gpointer user_data,
		      GDestroyNotify user_data_free) G_GNUC_NON_NULL(1, 2, 4);
void
xb_machine_add_operator(XbMachine *self, const gchar *str, const gchar *name)
    G_GNUC_NON_NULL(1, 2, 3);

gboolean
xb_machine_opcode_func_init(XbMachine *self, XbOpcode *opcode, const gchar *func_name)
    G_GNUC_NON_NULL(1, 2, 3);

gboolean
xb_machine_stack_pop(XbMachine *self, XbStack *stack, XbOpcode *opcode_out, GError **error)
    G_GNUC_NON_NULL(1, 2, 3);
gboolean
xb_machine_stack_push(XbMachine *self, XbStack *stack, XbOpcode **opcode_out, GError **error)
    G_GNUC_NON_NULL(1, 2, 3);
gboolean
xb_machine_stack_push_text(XbMachine *self, XbStack *stack, const gchar *str, GError **error)
    G_GNUC_NON_NULL(1, 2);
gboolean
xb_machine_stack_push_text_static(XbMachine *self, XbStack *stack, const gchar *str, GError **error)
    G_GNUC_NON_NULL(1, 2);
gboolean
xb_machine_stack_push_text_steal(XbMachine *self, XbStack *stack, gchar *str, GError **error)
    G_GNUC_NON_NULL(1, 2, 3);
gboolean
xb_machine_stack_push_integer(XbMachine *self, XbStack *stack, guint32 val, GError **error)
    G_GNUC_NON_NULL(1, 2);
void
xb_machine_set_stack_size(XbMachine *self, guint stack_size) G_GNUC_NON_NULL(1);
guint
xb_machine_get_stack_size(XbMachine *self) G_GNUC_NON_NULL(1);

G_END_DECLS
