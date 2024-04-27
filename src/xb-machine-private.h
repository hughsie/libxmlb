/*
 * Copyright 2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "xb-machine.h"

G_BEGIN_DECLS

gboolean
xb_machine_stack_pop_two(XbMachine *self,
			 XbStack *stack,
			 XbOpcode *opcode1_out,
			 XbOpcode *opcode2_out,
			 GError **error) G_GNUC_NON_NULL(1, 2, 3, 4);
void
xb_machine_opcode_tokenize(XbMachine *self, XbOpcode *op) G_GNUC_NON_NULL(1, 2);

G_END_DECLS
