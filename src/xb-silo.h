/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include "xb-node.h"

G_BEGIN_DECLS

#define XB_TYPE_SILO (xb_silo_get_type ())
G_DECLARE_DERIVABLE_TYPE (XbSilo, xb_silo, XB, SILO, GObject)

struct _XbSiloClass {
	GObjectClass			 parent_class;
	/*< private >*/
	void (*_xb_reserved1)		(void);
	void (*_xb_reserved2)		(void);
	void (*_xb_reserved3)		(void);
	void (*_xb_reserved4)		(void);
	void (*_xb_reserved5)		(void);
	void (*_xb_reserved6)		(void);
	void (*_xb_reserved7)		(void);
};

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
	XB_SILO_LOAD_FLAG_NO_ABI	= 1 << 1,		/* Since: 0.1.0 */
	XB_SILO_LOAD_FLAG_WATCH_BLOB	= 1 << 2,		/* Since: 0.1.0 */
	/*< private >*/
	XB_SILO_LOAD_FLAG_LAST
} XbSiloLoadFlags;

/**
 * XbSiloProfileFlags:
 * @XB_SILO_PROFILE_FLAG_NONE:			No extra flags to use
 * @XB_SILO_PROFILE_FLAG_DEBUG:			Output profiling as debug
 * @XB_SILO_PROFILE_FLAG_APPEND:		Save profiling in an appended string
 * @XB_SILO_PROFILE_FLAG_XPATH:			Save XPATH queries
 *
 * The flags used when profiling a silo.
 **/
typedef enum {
	XB_SILO_PROFILE_FLAG_NONE	= 0,			/* Since: 0.1.1 */
	XB_SILO_PROFILE_FLAG_DEBUG	= 1 << 0,		/* Since: 0.1.1 */
	XB_SILO_PROFILE_FLAG_APPEND	= 1 << 1,		/* Since: 0.1.1 */
	XB_SILO_PROFILE_FLAG_XPATH	= 1 << 2,		/* Since: 0.1.1 */
	/*< private >*/
	XB_SILO_PROFILE_FLAG_LAST
} XbSiloProfileFlags;

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
void		 xb_silo_invalidate			(XbSilo		*self);
gboolean	 xb_silo_is_valid			(XbSilo		*self);
gboolean	 xb_silo_watch_file			(XbSilo		*self,
							 GFile		*file,
							 GCancellable	*cancellable,
							 GError		**error);
void		 xb_silo_set_profile_flags		(XbSilo		*self,
							 XbSiloProfileFlags profile_flags);
const gchar	*xb_silo_get_profile_string		(XbSilo		*self);

G_END_DECLS
