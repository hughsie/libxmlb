/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <string.h>
#include <gio/gio.h>

#include "xb-common.h"

/**
 * xb_string_replace: (skip)
 * @str: The #GString to operate on
 * @search: The text to search for
 * @replace: The text to use for substitutions
 *
 * Performs multiple search and replace operations on the given string.
 *
 * Returns: the number of replacements done, or 0 if @search is not found.
 *
 * Since: 0.1.0
 **/
guint
xb_string_replace (GString *str, const gchar *search, const gchar *replace)
{
	gchar *tmp;
	guint count = 0;
	gsize search_idx = 0;
	gsize replace_len;
	gsize search_len;

	g_return_val_if_fail (str != NULL, 0);
	g_return_val_if_fail (search != NULL, 0);
	g_return_val_if_fail (replace != NULL, 0);

	/* nothing to do */
	if (str->len == 0)
		return 0;

	search_len = strlen (search);
	replace_len = strlen (replace);

	do {
		tmp = g_strstr_len (str->str + search_idx, -1, search);
		if (tmp == NULL)
			break;

		/* advance the counter in case @replace contains @search */
		search_idx = (gsize) (tmp - str->str);

		/* reallocate the string if required */
		if (search_len > replace_len) {
			g_string_erase (str,
					(gssize) search_idx,
					(gssize) (search_len - replace_len));
			memcpy (tmp, replace, replace_len);
		} else if (search_len < replace_len) {
			g_string_insert_len (str,
					     (gssize) search_idx,
					     replace,
					     (gssize) (replace_len - search_len));
			/* we have to treat this specially as it could have
			 * been reallocated when the insertion happened */
			memcpy (str->str + search_idx, replace, replace_len);
		} else {
			/* just memcmp in the new string */
			memcpy (tmp, replace, replace_len);
		}
		search_idx += replace_len;
		count++;
	} while (TRUE);

	return count;
}

gboolean
xb_string_contains_fuzzy (const gchar *text, const gchar *search)
{
	guint search_sz;
	guint text_sz;

	/* can't possibly match */
	if (text == NULL || search == NULL)
		return FALSE;

	/* sanity check */
	text_sz = strlen (text);
	search_sz = strlen (search);
	if (search_sz > text_sz)
		return FALSE;
	for (guint i = 0; i < text_sz - search_sz + 1; i++) {
		if (g_ascii_strncasecmp (text + i, search, search_sz) == 0)
			return TRUE;
	}
	return FALSE;
}
