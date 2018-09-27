/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_MACHINE_H
#define __XB_MACHINE_H

G_BEGIN_DECLS

#include <glib-object.h>

#include "xb-opcode.h"

#define XB_TYPE_MACHINE (xb_machine_get_type ())
G_DECLARE_FINAL_TYPE (XbMachine, xb_machine, XB, MACHINE, GObject)

typedef enum {
	XB_MACHINE_DEBUG_FLAG_NONE		= 0,
	XB_MACHINE_DEBUG_FLAG_SHOW_STACK	= 1 << 0,
	XB_MACHINE_DEBUG_FLAG_SHOW_PARSING	= 1 << 1,
	XB_MACHINE_DEBUG_FLAG_LAST
} XbMachineDebugFlags;

typedef gboolean (*XbMachineOpcodeFixupCb)	(XbMachine		*self,
						 GPtrArray		*opcodes,
						 gpointer		 user_data,
						 GError			**error);
typedef gboolean (*XbMachineTextHandlerCb)	(XbMachine		*self,
						 GPtrArray		*opcodes,
						 const gchar		*text,
						 gboolean		*handled,
						 gpointer		 user_data,
						 GError			**error);
typedef gboolean (*XbMachineFuncCb)		(XbMachine		*self,
						 gboolean		*result,
						 gpointer		 user_data,
						 GError			**error);

XbMachine	*xb_machine_new			(void);
void		 xb_machine_set_debug_flags	(XbMachine		*self,
						 XbMachineDebugFlags	 flags);
GPtrArray	*xb_machine_parse		(XbMachine		*self,
						 const gchar		*text,
						 gssize			 text_len,
						 GError			**error);
gboolean	 xb_machine_run			(XbMachine		*self,
						 GPtrArray		*opcodes,
						 gboolean		*result,
						 GError			**error);

void		 xb_machine_add_opcode_fixup	(XbMachine		*self,
						 const gchar		*opcodes_sig,
						 XbMachineOpcodeFixupCb	 fixup_cb,
						 gpointer		 user_data);
void		 xb_machine_add_text_handler	(XbMachine		*self,
						 XbMachineTextHandlerCb	 fixup_cb,
						 gpointer		 user_data);
void		 xb_machine_add_func		(XbMachine		*self,
						 const gchar		*name,
						 guint			 n_opcodes,
						 XbMachineFuncCb	 func_cb,
						 gpointer		 user_data);
void		 xb_machine_add_operator	(XbMachine		*self,
						 const gchar		*id,
						 const gchar		*name);

XbOpcode	*xb_machine_opcode_func_new	(XbMachine		*self,
						 const gchar		*func_name);
gchar		*xb_machine_opcode_to_string	(XbMachine		*self,
						 XbOpcode		*opcode);
gchar		*xb_machine_opcodes_to_string	(XbMachine		*self,
						 GPtrArray		*opcodes);

XbOpcode	*xb_machine_stack_pop		(XbMachine		*self);
void		 xb_machine_stack_push		(XbMachine		*self,
						 XbOpcode		*opcode);
void		 xb_machine_stack_push_text	(XbMachine		*self,
						 const gchar		*str);
void		 xb_machine_stack_push_text_static (XbMachine		*self,
						 const gchar		*str);
void		 xb_machine_stack_push_text_steal (XbMachine		*self,
						 gchar			*str);
void		 xb_machine_stack_push_integer	(XbMachine		*self,
						 guint32		 val);

G_END_DECLS

#endif /* __XB_MACHINE_H */
