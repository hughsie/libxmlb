/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#ifndef __XB_PREDICATE_H
#define __XB_PREDICATE_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef enum {
	XB_PREDICATE_KIND_NONE,
	XB_PREDICATE_KIND_EQ,
	XB_PREDICATE_KIND_NE,
	XB_PREDICATE_KIND_GT,
	XB_PREDICATE_KIND_LT,
	XB_PREDICATE_KIND_LE,
	XB_PREDICATE_KIND_GE,
	XB_PREDICATE_KIND_LAST
} XbPredicateKind;

typedef enum {
	XB_PREDICATE_QUIRK_NONE		= 0,
	XB_PREDICATE_QUIRK_IS_TEXT	= 1 << 0,
	XB_PREDICATE_QUIRK_IS_ATTR	= 1 << 1,
	XB_PREDICATE_QUIRK_IS_POSITION	= 1 << 2,
	XB_PREDICATE_QUIRK_IS_FN_LAST	= 1 << 3,
	XB_PREDICATE_QUIRK_LAST
} XbPredicateQuirk;

typedef struct {
	XbPredicateKind		 kind;
	gchar			*lhs;
	gchar			*rhs;
	XbPredicateQuirk	 quirk;
	guint			 position;
} XbPredicate;

void		 xb_predicate_free		(XbPredicate	*self);
gboolean	 xb_predicate_query		(XbPredicateKind kind,
						 gint		 rc);
XbPredicate	*xb_predicate_new		(const gchar	*text,
						 gssize		 text_len,
						 GError		**error);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(XbPredicate, xb_predicate_free)

G_END_DECLS

#endif /* __XB_PREDICATE_H */
