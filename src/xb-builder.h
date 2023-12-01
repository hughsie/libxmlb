/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "xb-builder-fixup.h"
#include "xb-builder-node.h"
#include "xb-builder-source.h"
#include "xb-compile.h"
#include "xb-silo.h"

G_BEGIN_DECLS

#define XB_TYPE_BUILDER (xb_builder_get_type())

G_DECLARE_DERIVABLE_TYPE(XbBuilder, xb_builder, XB, BUILDER, GObject)

struct _XbBuilderClass {
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
 * XbBuilderCompileFlags:
 * @XB_BUILDER_COMPILE_FLAG_NONE:		No extra flags to use
 * @XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS:	Only load native languages
 * @XB_BUILDER_COMPILE_FLAG_IGNORE_INVALID:	Ignore invalid files without an error
 * @XB_BUILDER_COMPILE_FLAG_SINGLE_LANG:	Only store a single language
 * @XB_BUILDER_COMPILE_FLAG_WATCH_BLOB:		Watch the XMLB file for changes
 * @XB_BUILDER_COMPILE_FLAG_IGNORE_GUID:	Ignore the cache GUID value
 * @XB_BUILDER_COMPILE_FLAG_SINGLE_ROOT:	Require at most one root node
 *
 * The flags for converting to XML.
 **/
typedef enum {
	XB_BUILDER_COMPILE_FLAG_NONE = 0,		 /* Since: 0.1.0 */
	XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS = 1 << 1,	 /* Since: 0.1.0 */
	XB_BUILDER_COMPILE_FLAG_IGNORE_INVALID = 1 << 2, /* Since: 0.1.0 */
	XB_BUILDER_COMPILE_FLAG_SINGLE_LANG = 1 << 3,	 /* Since: 0.1.0 */
	XB_BUILDER_COMPILE_FLAG_WATCH_BLOB = 1 << 4,	 /* Since: 0.1.0 */
	XB_BUILDER_COMPILE_FLAG_IGNORE_GUID = 1 << 5,	 /* Since: 0.1.7 */
	XB_BUILDER_COMPILE_FLAG_SINGLE_ROOT = 1 << 6,	 /* Since: 0.3.4 */
	/*< private >*/
	XB_BUILDER_COMPILE_FLAG_LAST
} XbBuilderCompileFlags;

XbBuilder *
xb_builder_new(void);
void
xb_builder_append_guid(XbBuilder *self, const gchar *guid) G_GNUC_NON_NULL(1, 2);
void
xb_builder_import_source(XbBuilder *self, XbBuilderSource *source) G_GNUC_NON_NULL(1, 2);
void
xb_builder_import_node(XbBuilder *self, XbBuilderNode *bn) G_GNUC_NON_NULL(1, 2);
XbSilo *
xb_builder_compile(XbBuilder *self,
		   XbBuilderCompileFlags flags,
		   GCancellable *cancellable,
		   GError **error) G_GNUC_NON_NULL(1);
XbSilo *
xb_builder_ensure(XbBuilder *self,
		  GFile *file,
		  XbBuilderCompileFlags flags,
		  GCancellable *cancellable,
		  GError **error) G_GNUC_NON_NULL(1, 2);
void
xb_builder_add_locale(XbBuilder *self, const gchar *locale) G_GNUC_NON_NULL(1, 2);
void
xb_builder_add_fixup(XbBuilder *self, XbBuilderFixup *fixup) G_GNUC_NON_NULL(1, 2);
void
xb_builder_set_profile_flags(XbBuilder *self, XbSiloProfileFlags profile_flags) G_GNUC_NON_NULL(1);

G_END_DECLS
