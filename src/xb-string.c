/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"XbSilo"

#include "config.h"

#include <string.h>
#include <gio/gio.h>

#include "xb-string-private.h"

/**
 * xb_string_replace: (skip)
 * @str: The #GString to operate on
 * @search: The text to search for
 * @replace: The text to use for substitutions
 *
 * Performs multiple search and replace operations on the given string.
 *
 * Returns: the number of replacements done, or 0 if @search is not found.
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

/**
 * xb_string_append_union:
 * @xpath: The #GString to operate on
 * @fmt: The format string
 * @...: varargs for @fmt
 *
 * Appends an XPath query into the string, automatically adding the union
 * operator (`|`) if required.
 *
 * Since: 0.1.2
 **/
void
xb_string_append_union (GString *xpath, const gchar *fmt, ...)
{
	va_list args;

	g_return_if_fail (xpath != NULL);
	g_return_if_fail (fmt != NULL);

	if (xpath->len > 0)
		g_string_append (xpath, "|");
	va_start (args, fmt);
#pragma clang diagnostic ignored "-Wformat-nonliteral"
	g_string_append_vprintf (xpath, fmt, args);
#pragma clang diagnostic pop
	va_end (args);
}

/**
 * xb_string_contains: (skip)
 * @text: The source string
 * @search: The text to search for
 *
 * Searches for a substring match.
 *
 * Returns: %TRUE if the string @search is contained in @text.
 **/
gboolean
xb_string_contains (const gchar *text, const gchar *search)
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
		if (strncmp (text + i, search, search_sz) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * xb_string_search: (skip)
 * @text: The ASCII source string
 * @search: The ASCII text to search for
 *
 * Searches for a fuzzy search match, ignoring search matches that are not at
 * the start of the token.
 *
 * Returns: %TRUE if the string @search is contained in @text.
 **/
gboolean
xb_string_search (const gchar *text, const gchar *search)
{
	guint search_sz;
	guint text_sz;
	gboolean is_sow = TRUE;

	/* can't possibly match */
	if (text == NULL || text[0] == '\0')
		return FALSE;
	if (search == NULL || search[0] == '\0')
		return FALSE;

	/* sanity check */
	text_sz = strlen (text);
	search_sz = strlen (search);
	if (search_sz > text_sz)
		return FALSE;
	for (guint i = 0; i < text_sz - search_sz + 1; i++) {
		if (!g_ascii_isalnum (text[i])) {
			is_sow = TRUE;
			continue;
		}
		if (!is_sow)
			continue;
		if (g_ascii_strncasecmp (text + i, search, search_sz) == 0)
			return TRUE;
		/* no longer the start of the word */
		is_sow = FALSE;
	}
	return FALSE;
}

/**
 * xb_string_search: (skip)
 * @text: The source string
 * @search: The text to search for
 *
 * Searches for a fuzzy search match, ignoring search matches that are not at
 * the start of the token.
 *
 * Returns: %TRUE if the string @search is contained in @text.
 **/
gboolean
xb_string_searchv (const gchar **text, const gchar **search)
{
	if (text == NULL || text[0] == NULL || text[0][0] == '\0')
		return FALSE;
	if (search == NULL || search[0] == NULL || search[0][0] == '\0')
		return FALSE;
	for (guint j = 0; text[j] != NULL; j++) {
		for (guint i = 0; search[i] != NULL; i++) {
			if (g_str_has_prefix (text[j], search[i]))
				return TRUE;
		}
	}
	return FALSE;
}

/**
 * xb_string_token_valid: (skip)
 * @text: The potential token
 *
 * Checks if the token is valid.
 *
 * Returns: %TRUE if the token should be used.
 **/
gboolean
xb_string_token_valid (const gchar *text)
{
	if (text == NULL)
		return FALSE;
	if (text[0] == '\0' ||
	    text[1] == '\0' ||
	    text[2] == '\0')
		return FALSE;
	return TRUE;
}

/**
 * xb_string_escape:
 * @str: string, e.g. `app/org.gnome.ghex/x86_64/stable`
 *
 * Escapes XPath control sequences such as newlines, tabs, and forward slashes.
 *
 * Returns: (transfer full): new string that is safe to use for queries
 *
 * Since: 0.1.2
 **/
gchar *
xb_string_escape (const gchar *str)
{
	GString *tmp = g_string_new (str);
	xb_string_replace (tmp, "/", "\\/");
	xb_string_replace (tmp, "\t", "\\t");
	xb_string_replace (tmp, "\n", "\\n");
	return g_string_free (tmp, FALSE);
}

gchar *
xb_string_xml_escape (const gchar *str)
{
	GString *tmp = g_string_new (str);
	xb_string_replace (tmp, "&", "&amp;");
	xb_string_replace (tmp, "<", "&lt;");
	xb_string_replace (tmp, ">", "&gt;");
	xb_string_replace (tmp, "\"", "&quot;");
	return g_string_free (tmp, FALSE);
}

/* private */
gboolean
xb_string_isspace (const gchar *str, gssize strsz)
{
	gsize strsz_safe;
	if (str == NULL)
		return TRUE;
	strsz_safe = strsz >= 0 ? (gsize) strsz : strlen (str);
	for (gsize i = 0; i < strsz_safe; i++) {
		if (!g_ascii_isspace (str[i]))
			return FALSE;
	}
	return TRUE;
}

void
xb_guid_compute_for_data (XbGuid *out, const guint8 *buf, gsize bufsz)
{
	guint8 buf_tmp[20] = { 0x0 };
	gsize buf_tmpsz = sizeof(buf_tmp);
	g_autoptr(GChecksum) checksum = g_checksum_new (G_CHECKSUM_SHA1);
	if (buf != NULL && bufsz != 0)
		g_checksum_update (checksum, (const guchar *) buf, bufsz);
	g_checksum_get_digest (checksum, buf_tmp, &buf_tmpsz);
	memcpy (out, buf_tmp, sizeof(XbGuid));
}

gchar *
xb_guid_to_string (XbGuid *guid)
{
	return g_strdup_printf ("%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x",
				(guint) GUINT32_TO_BE (guid->tlo),
				(guint) GUINT16_TO_BE (guid->tmi),
				(guint) GUINT16_TO_BE (guid->thi),
				(guint) GUINT16_TO_BE (guid->clo),
				guid->nde[0], guid->nde[1],
				guid->nde[2], guid->nde[3],
				guid->nde[4], guid->nde[5]);
}
