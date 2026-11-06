/*
 * Copyright 2020 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define G_LOG_DOMAIN "XbCommon"

#include "config.h"

#include <string.h>

#include "xb-common-private.h"

static const gchar *
xb_content_type_guess_from_fn(const gchar *filename)
{
	gchar *ext; /* no ownership */

	g_return_val_if_fail(filename != NULL, NULL);

	/* get file extension with dot */
	ext = g_strrstr(filename, ".");
	if (ext == NULL)
		return NULL;

	/* map Windows "mime-type" to a content type */
	if (g_strcmp0(ext, ".gz") == 0)
		return "application/gzip";
	if (g_strcmp0(ext, ".xz") == 0)
		return "application/x-xz";
	if (g_strcmp0(ext, ".zst") == 0)
		return "application/zstd";
	if (g_strcmp0(ext, ".xml") == 0)
		return "application/xml";
	if (g_strcmp0(ext, ".desktop") == 0)
		return "application/x-desktop";
	if (g_strcmp0(ext, ".quirk") == 0)
		return "text/plain";
	return NULL;
}

static gboolean
xb_content_type_match(const guchar *buf,
		      gsize bufsz,
		      gsize offset,
		      const gchar *magic,
		      gsize magic_size)
{
	/* document too small */
	if (offset + magic_size > bufsz)
		return FALSE;
	return memcmp(buf + offset, magic, magic_size) == 0;
}

/**
 * xb_content_type_guess: (skip)
 * @filename: (nullable): filename
 * @buf: (nullable): file data buffer
 * @bufsz: size of file data buffer
 *
 * Guesses the content type based on example data. Either @filename or @buf may
 * be %NULL, in which case the guess will be based solely on the other argument.
 *
 * Returns: a string indicating a guessed content type
 **/
gchar *
xb_content_type_guess(const gchar *filename, const guchar *buf, gsize bufsz)
{
	g_autofree gchar *content_type = NULL;

	/* check for bad results, e.g. from Chrome OS */
	content_type = g_content_type_guess(filename, buf, bufsz, NULL);
	if (g_strstr_len(content_type, -1, "/") == NULL ||
	    g_strcmp0(content_type, "application/octet-stream") == 0 ||
	    g_strcmp0(content_type, "text/plain") == 0) {
		/* magic */
		if (bufsz > 0) {
			if (xb_content_type_match(buf, bufsz, 0x0, "\x1f\x8b", 2))
				return g_strdup("application/gzip");
			if (xb_content_type_match(buf, bufsz, 0x0, "\xfd\x37\x7a\x58\x5a\x00", 6))
				return g_strdup("application/x-xz");
			if (xb_content_type_match(buf, bufsz, 0x0, "\x28\xb5\x2f\xfd", 4))
				return g_strdup("application/zstd");
			if (xb_content_type_match(buf, bufsz, 0x0, "<?xml", 5))
				return g_strdup("application/xml");
			if (xb_content_type_match(buf, bufsz, 0x0, "[Desktop Entry]", 15))
				return g_strdup("application/x-desktop");
		}

		/* file extensions */
		if (filename != NULL) {
			const gchar *tmp = xb_content_type_guess_from_fn(filename);
			if (tmp != NULL)
				return g_strdup(tmp);
		}
	}

#ifdef _WIN32
	/* fall back harder as there is no mime data at all */
	if (filename != NULL) {
		const gchar *tmp = xb_content_type_guess_from_fn(filename);
		if (tmp != NULL)
			return g_strdup(tmp);
	}
#endif

	return g_steal_pointer(&content_type);
}

/**
 * xb_file_set_contents: (skip)
 * @file: (nullable): file to write
 * @buf: (nullable): data buffer
 * @bufsz: size of @buf
 * @cancellable: (nullable): optional #GCancellable
 * @error: (nullable): optional return location for an error
 *
 * Writes data to a file.
 *
 * Returns: %TRUE for success
 **/
gboolean
xb_file_set_contents(GFile *file,
		     const guint8 *buf,
		     gsize bufsz,
		     GCancellable *cancellable,
		     GError **error)
{
	g_return_val_if_fail(G_IS_FILE(file), FALSE);
	g_return_val_if_fail(buf != NULL, FALSE);
	g_return_val_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
	return g_file_replace_contents(file,
				       (const gchar *)buf,
				       (gsize)bufsz,
				       NULL,
				       FALSE,
				       G_FILE_CREATE_NONE,
				       NULL,
				       cancellable,
				       error);
}

/**
 * xb_strsplit:
 * @str: (not nullable): a string to split
 * @sz: size of @str, which must be more than 0
 * @delimiter: a string which specifies the places at which to split the string
 * @max_tokens: the maximum number of pieces to split @str into
 *
 * Splits a string into a maximum of @max_tokens pieces, using the given
 * delimiter. If @max_tokens is reached, the remainder of string is appended
 * to the last token.
 *
 * Returns: (transfer full): a newly-allocated NULL-terminated array of strings
 **/
gchar **
xb_strsplit(const gchar *str, gsize sz, const gchar *delimiter, gint max_tokens)
{
	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(sz > 0, NULL);
	if (str[sz - 1] != '\0') {
		g_autofree gchar *str2 = g_strndup(str, sz);
		return g_strsplit(str2, delimiter, max_tokens);
	}
	return g_strsplit(str, delimiter, max_tokens);
}

/**
 * xb_locale_ensure_posix:
 * @locale: (nullable): a locale, in BCP47 or POSIX form
 *
 * Ensures the locale is in POSIX form, e.g. `en_GB`.
 *
 * Only the `language_COUNTRY` separator is converted; any codeset or modifier
 * suffix is left alone, so that e.g. the `-` in `fr_FR.UTF-8` is preserved.
 *
 * BCP47 tags are case-insensitive, so the language is made lowercase and a
 * two-letter country uppercase, e.g. `EN-gb` becomes `en_GB`. Without a separator
 * only a two or three letter language is changed, so that e.g. `EN` becomes `en`
 * but `C` and `POSIX` are preserved.
 *
 * A separator with nothing on one side is not converted, e.g. `-` and `-gb` are
 * returned as-is, as they are malformed rather than in BCP47 form.
 *
 * A script subtag is dropped only when POSIX implies it from the country, e.g.
 * `zh-Hans-CN` becomes `zh_CN`. Any other script is kept so that it cannot match
 * a locale written in a different script, e.g. `zh-Hant-CN` is not `zh_CN`.
 * Where POSIX spells the script as a modifier instead, the script is moved
 * there, e.g. `sr-Latn` and `sr-Latn-RS` become `sr@latin` and `sr_RS@latin`,
 * unless a modifier is already present.
 *
 * An empty locale is returned as-is, as the empty and %NULL values mean different
 * things: to g_str_to_ascii() the former is the generic rules and the latter is
 * the current locale.
 *
 * Returns: (transfer full): a POSIX-form locale string, or %NULL if @locale is %NULL
 **/
gchar *
xb_locale_ensure_posix(const gchar *locale)
{
	struct {
		const gchar *tag;
		const gchar *posix;
		const gchar *modifier;
	} map[] = {
	    {"sr_Latn", "sr", "latin"},
	    {"sr_Latn-ME", "sr_ME", "latin"},
	    {"sr_Latn-RS", "sr_RS", "latin"},
	    {"zh_Hans-CN", "zh_CN", NULL},
	    {"zh_Hans-SG", "zh_SG", NULL},
	    {"zh_Hant-HK", "zh_HK", NULL},
	    {"zh_Hant-MO", "zh_MO", NULL},
	    {"zh_Hant-TW", "zh_TW", NULL},
	};
	g_autofree gchar *str = g_strdup(locale);
	gsize langsz;
	gsize subtagsz = 0;

	if (str == NULL)
		return NULL;

	/* POSIX locales have at most one separator, and it always comes before
	 * any `.codeset` or `@modifier` suffix -- it only counts as a separator
	 * when it has something on both sides */
	langsz = strcspn(str, "-_.@");
	if (langsz > 0 && (str[langsz] == '-' || str[langsz] == '_'))
		subtagsz = strcspn(str + langsz + 1, "-_.@");
	if (subtagsz > 0 || langsz == 2 || langsz == 3) {
		for (gsize i = 0; i < langsz; i++)
			str[i] = g_ascii_tolower(str[i]);
	}
	if (subtagsz > 0)
		str[langsz] = '_';
	if (subtagsz == 2) {
		str[langsz + 1] = g_ascii_toupper(str[langsz + 1]);
		str[langsz + 2] = g_ascii_toupper(str[langsz + 2]);
	}

	/* the script may be in any case */
	for (guint i = 0; i < G_N_ELEMENTS(map); i++) {
		gsize tagsz = strlen(map[i].tag);
		const gchar *suffix;
		if (g_ascii_strncasecmp(str, map[i].tag, tagsz) != 0)
			continue;
		suffix = str + tagsz;
		if (suffix[0] != '\0' && suffix[0] != '.' && suffix[0] != '@')
			continue;
		if (map[i].modifier == NULL)
			return g_strdup_printf("%s%s", map[i].posix, suffix);

		/* POSIX only allows one modifier, so do not lose either */
		if (strchr(suffix, '@') != NULL)
			break;
		return g_strdup_printf("%s%s@%s", map[i].posix, suffix, map[i].modifier);
	}
	return g_steal_pointer(&str);
}
