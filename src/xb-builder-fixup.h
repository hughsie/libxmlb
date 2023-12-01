/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "xb-builder-node.h"

G_BEGIN_DECLS

#define XB_TYPE_BUILDER_FIXUP (xb_builder_fixup_get_type())

G_DECLARE_DERIVABLE_TYPE(XbBuilderFixup, xb_builder_fixup, XB, BUILDER_FIXUP, GObject)

struct _XbBuilderFixupClass {
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

typedef gboolean (*XbBuilderFixupFunc)(XbBuilderFixup *self,
				       XbBuilderNode *bn,
				       gpointer user_data,
				       GError **error);

XbBuilderFixup *
xb_builder_fixup_new(const gchar *id,
		     XbBuilderFixupFunc func,
		     gpointer user_data,
		     GDestroyNotify user_data_free) G_GNUC_NON_NULL(1, 2);
gint
xb_builder_fixup_get_max_depth(XbBuilderFixup *self) G_GNUC_NON_NULL(1);
void
xb_builder_fixup_set_max_depth(XbBuilderFixup *self, gint max_depth) G_GNUC_NON_NULL(1);

G_END_DECLS
