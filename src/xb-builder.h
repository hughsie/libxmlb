/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_BUILDER_H
#define __XB_BUILDER_H

#include <gio/gio.h>

#include "xb-builder-node.h"
#include "xb-silo.h"

G_BEGIN_DECLS

#define XB_TYPE_BUILDER (xb_builder_get_type ())

G_DECLARE_FINAL_TYPE (XbBuilder, xb_builder, XB, BUILDER, GObject)

/**
 * XbBuilderCompileFlags:
 * @XB_BUILDER_COMPILE_FLAG_NONE:		No extra flags to use
 * @XB_BUILDER_COMPILE_FLAG_LITERAL_TEXT:	Do not attempt to repair XML whitespace
 * @XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS:	Only load native languages
 * @XB_BUILDER_COMPILE_FLAG_IGNORE_INVALID:	Ignore invalid files without an error
 *
 * The flags for converting to XML.
 **/
typedef enum {
	XB_BUILDER_COMPILE_FLAG_NONE		= 0,		/* Since: 0.1.0 */
	XB_BUILDER_COMPILE_FLAG_LITERAL_TEXT	= 1 << 0,	/* Since: 0.1.0 */
	XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS	= 1 << 1,	/* Since: 0.1.0 */
	XB_BUILDER_COMPILE_FLAG_IGNORE_INVALID	= 1 << 2,	/* Since: 0.1.0 */
	/*< private >*/
	XB_BUILDER_COMPILE_FLAG_LAST
} XbBuilderCompileFlags;

XbBuilder	*xb_builder_new			(void);
void		 xb_builder_append_guid		(XbBuilder		*self,
						 const gchar		*guid);
void		 xb_builder_import_node		(XbBuilder		*self,
						 XbBuilderNode		*bn);
gboolean	 xb_builder_import_xml		(XbBuilder		*self,
						 const gchar		*xml,
						 GError			**error);
gboolean	 xb_builder_import_dir		(XbBuilder		*self,
						 const gchar		*path,
						 XbBuilderNode		*info,
						 GCancellable		*cancellable,
						 GError			**error);
gboolean	 xb_builder_import_file		(XbBuilder		*self,
						 GFile			*file,
						 XbBuilderNode		*info,
						 GCancellable		*cancellable,
						 GError			**error);
XbSilo		*xb_builder_compile		(XbBuilder		*self,
						 XbBuilderCompileFlags	 flags,
						 GCancellable		*cancellable,
						 GError			**error);
XbSilo		*xb_builder_ensure		(XbBuilder		*self,
						 GFile			*file,
						 XbBuilderCompileFlags	 flags,
						 GCancellable		*cancellable,
						 GError			**error);

G_END_DECLS

#endif /* __XB_BUILDER_H */
