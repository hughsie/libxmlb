/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_SILO_H
#define __XB_SILO_H

G_BEGIN_DECLS

#include <glib-object.h>

#include "xb-node.h"

#define XB_TYPE_SILO (xb_silo_get_type ())
G_DECLARE_FINAL_TYPE (XbSilo, xb_silo, XB, SILO, GObject)

/**
 * XbSiloLoadFlags:
 * @XB_SILO_LOAD_FLAG_NONE:			No extra flags to use
 * @XB_SILO_LOAD_FLAG_NO_MAGIC:			No not check header signature
 * @XB_SILO_LOAD_FLAG_WATCH_BLOB:		Watch the XMLB file for changes
 *
 * The flags for loading a silo.
 **/
typedef enum {
	XB_SILO_LOAD_FLAG_NONE		= 0,			/* Since: 0.1.0 */
	XB_SILO_LOAD_FLAG_NO_MAGIC	= 1 << 0,		/* Since: 0.1.0 */
	XB_SILO_LOAD_FLAG_WATCH_BLOB	= 1 << 1,		/* Since: 0.1.0 */
	/*< private >*/
	XB_SILO_LOAD_FLAG_LAST
} XbSiloLoadFlags;

XbSilo		*xb_silo_new				(void);
XbSilo		*xb_silo_new_from_xml			(const gchar	*xml,
							 GError		**error);
GBytes		*xb_silo_get_bytes			(XbSilo		*self);
gboolean	 xb_silo_load_from_bytes		(XbSilo		*self,
							 GBytes		*blob,
							 XbSiloLoadFlags flags,
							 GError		**error);
gboolean	 xb_silo_load_from_file			(XbSilo		*self,
							 GFile		*file,
							 XbSiloLoadFlags flags,
							 GCancellable	*cancellable,
							 GError		**error);
gboolean	 xb_silo_save_to_file			(XbSilo		*self,
							 GFile		*file,
							 GCancellable	*cancellable,
							 GError		**error);
gchar		*xb_silo_to_string			(XbSilo		*self,
							 GError		**error);
guint		 xb_silo_get_size			(XbSilo		*self);
const gchar	*xb_silo_get_guid			(XbSilo		*self);
XbNode		*xb_silo_get_root			(XbSilo		*self);
gboolean	 xb_silo_is_valid			(XbSilo		*self);
gboolean	 xb_silo_watch_file			(XbSilo		*self,
							 GFile		*file,
							 GCancellable	*cancellable,
							 GError		**error);

G_END_DECLS

#endif /* __XB_SILO_H */

