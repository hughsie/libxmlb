/*
 * Copyright 2025 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "xb-silo-node.h"

/* private */
gchar *
xb_silo_node_to_string(const XbSiloNode *self)
{
	GString *str = g_string_new("XbSiloNode:\n");
	g_string_append_printf(str, "  flags: 0x%x\n", self->flags);
	g_string_append_printf(str, "  attr_count: %u\n", self->attr_count);
	if (self->flags & XB_SILO_NODE_FLAG_IS_ELEMENT) {
		if (self->element_name != XB_SILO_UNSET)
			g_string_append_printf(str, "  element_name: %u\n", self->element_name);
		if (self->parent != XB_SILO_UNSET)
			g_string_append_printf(str, "  parent: @%u\n", self->parent);
		if (self->next != XB_SILO_UNSET)
			g_string_append_printf(str, "  next: @%u\n", self->next);
		if (self->text != XB_SILO_UNSET)
			g_string_append_printf(str, "  text: %u\n", self->text);
		if (self->tail != XB_SILO_UNSET)
			g_string_append_printf(str, "  tail: %u\n", self->tail);
	}
	for (guint idx = 0; idx < self->attr_count; idx++) {
		XbSiloNodeAttr *attr = xb_silo_node_get_attr(self, idx);
		g_string_append_printf(str, "  attr: %u=%u\n", attr->attr_name, attr->attr_value);
	}
	return g_string_free(str, FALSE);
}
