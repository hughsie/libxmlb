/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <string.h>
#include <gio/gio.h>

#include "xb-predicate.h"

gboolean
xb_predicate_query (XbPredicateKind kind, gint rc)
{
	if (kind == XB_PREDICATE_KIND_EQ)
		return rc == 0;
	if (kind == XB_PREDICATE_KIND_NE)
		return rc != 0;
	if (kind == XB_PREDICATE_KIND_GT)
		return rc > 0;
	if (kind == XB_PREDICATE_KIND_LT)
		return rc < 0;
	if (kind == XB_PREDICATE_KIND_LE)
		return rc <= 0;
	if (kind == XB_PREDICATE_KIND_GE)
		return rc >= 0;
	g_critical ("predicate %u unknown", kind);
	return FALSE;
}

static gchar *
xb_strndup_no_quotes (const gchar *text, gsize text_len)
{
	if (text_len >= 2) {
		if (text[0] == '\'' && text[text_len - 1] == '\'') {
			text += 1;
			text_len -= 2;
		}
	}
	return g_strndup (text, text_len);
}

XbPredicate *
xb_predicate_new (const gchar *text, gssize text_len, GError **error)
{
	g_autoptr(XbPredicate) self = g_slice_new0 (XbPredicate);
	struct {
		XbPredicateKind	 kind;
		gsize		 len;
		const gchar	*str;
	} kinds[] = {
		{ XB_PREDICATE_KIND_NE,		2,	"!=" },	/* has to be ordered by strlen */
		{ XB_PREDICATE_KIND_LE,		2,	"<=" },
		{ XB_PREDICATE_KIND_GE,		2,	">=" },
		{ XB_PREDICATE_KIND_EQ,		2,	"==" },
		{ XB_PREDICATE_KIND_CONTAINS,	2,	"~=" },
		{ XB_PREDICATE_KIND_EQ,		1,	"=" },
		{ XB_PREDICATE_KIND_GT,		1,	">" },
		{ XB_PREDICATE_KIND_LT,		1,	"<" },
		{ XB_PREDICATE_KIND_NONE,	0,	NULL }
	};

	/* assume NUL terminated */
	if (text_len < 0)
		text_len = strlen (text);

	/* parse */
	for (gsize i = 0; text[i] != '\0' && self->kind == XB_PREDICATE_KIND_NONE; i++) {
		for (guint j = 0; kinds[j].kind; j++) {
			if (strncmp (text + i, kinds[j].str, kinds[j].len) == 0) {
				self->kind = kinds[j].kind;
				self->lhs = g_strndup (text, i);
				self->rhs = xb_strndup_no_quotes (text + i + kinds[j].len,
								  text_len - (i + kinds[j].len));
				break;
			}
		}
	}
	if (self->lhs == NULL)
		self->lhs = g_strndup (text, text_len);
	//g_debug ("self->lhs=%s", self->lhs);
	//g_debug ("self->rhs=%s", self->rhs);

	/* optimize */
	if (text_len > 1 && text[0] == '@')
		self->quirk |= XB_PREDICATE_QUIRK_IS_ATTR;
	if (g_strcmp0 (self->lhs, "text()") == 0)
		self->quirk = XB_PREDICATE_QUIRK_IS_TEXT;

	/* @a => exists */
	if (self->kind == XB_PREDICATE_KIND_NONE &&
	    self->quirk == XB_PREDICATE_QUIRK_IS_ATTR)
		self->kind = XB_PREDICATE_KIND_EQ;

	/* last() */
	if (self->kind == XB_PREDICATE_KIND_NONE &&
	    self->quirk == XB_PREDICATE_QUIRK_NONE &&
	    g_strcmp0 (self->lhs, "last()") == 0) {
		self->kind = XB_PREDICATE_KIND_EQ;
		self->quirk |= XB_PREDICATE_QUIRK_IS_POSITION;
		self->quirk |= XB_PREDICATE_QUIRK_IS_FN_LAST;
	}

	/* [n] -> [position()=n] */
	if (self->kind == XB_PREDICATE_KIND_NONE &&
	    self->quirk == XB_PREDICATE_QUIRK_NONE) {
		g_autofree gchar *tmp = g_strndup (text, text_len);
		guint64 idx = g_ascii_strtoull (tmp, NULL, 10);
		if (idx == 0 || idx > G_MAXUINT) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_INVALID_DATA,
				     "failed to parse number '%s' as node position",
				     tmp);
			return NULL;
		}
		self->rhs = self->lhs;
		self->lhs = g_strdup ("position()");
		self->position = idx;
		self->kind = XB_PREDICATE_KIND_EQ;
		self->quirk |= XB_PREDICATE_QUIRK_IS_POSITION;
	}

	/* '=' */
	if (self->quirk == XB_PREDICATE_QUIRK_NONE &&
	    g_strcmp0 (self->lhs, self->rhs) == 0) {
		g_autofree gchar *tmp = g_strndup (text, text_len);
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "failed to parse predicate '%s' as no sections",
			     tmp);
		return NULL;
	}

	/* not recognised */
	if (self->kind == XB_PREDICATE_KIND_NONE) {
		g_autofree gchar *tmp = g_strndup (text, text_len);
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_INVALID_DATA,
			     "failed to parse invalid predicate '%s'",
			     tmp);
		return NULL;
	}
	return g_steal_pointer (&self);
}

void
xb_predicate_free (XbPredicate *self)
{
	g_free (self->lhs);
	g_free (self->rhs);
	g_slice_free (XbPredicate, self);
}
