/*
 * Copyright (C) 2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "xb-machine.h"

G_BEGIN_DECLS

gboolean	 xb_machine_stack_pop_two	(XbMachine	*self,
						 XbStack	*stack,
						 XbOpcode	*opcode1_out,
						 XbOpcode	*opcode2_out,
						 GError		**error);

G_END_DECLS
