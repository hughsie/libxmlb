/*
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_GIO_UNIX
#include <glib-unix.h>
#endif
#include <gio/gio.h>
#include <locale.h>

#include "xb-builder.h"
#include "xb-silo-export.h"
#include "xb-silo-query.h"
#include "xb-node.h"

typedef struct {
	GCancellable		*cancellable;
	GMainLoop		*loop;
	GPtrArray		*cmd_array;
	gboolean		 force;
	gboolean		 wait;
	gboolean		 profile;
	gchar			**tokenize;
} XbToolPrivate;

static void
xb_tool_private_free (XbToolPrivate *priv)
{
	if (priv == NULL)
		return;
	if (priv->cmd_array != NULL)
		g_ptr_array_unref (priv->cmd_array);
	g_main_loop_unref (priv->loop);
	g_object_unref (priv->cancellable);
	g_strfreev (priv->tokenize);
	g_free (priv);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
G_DEFINE_AUTOPTR_CLEANUP_FUNC(XbToolPrivate, xb_tool_private_free)
#pragma clang diagnostic pop

typedef gboolean (*FuUtilPrivateCb)	(XbToolPrivate	*util,
					 gchar		**values,
					 GError		**error);

typedef struct {
	gchar		*name;
	gchar		*arguments;
	gchar		*description;
	FuUtilPrivateCb	 callback;
} FuUtilItem;

static void
xb_tool_item_free (FuUtilItem *item)
{
	g_free (item->name);
	g_free (item->arguments);
	g_free (item->description);
	g_free (item);
}

static gint
xb_tool_sort_command_name_cb (FuUtilItem **item1, FuUtilItem **item2)
{
	return g_strcmp0 ((*item1)->name, (*item2)->name);
}

static void
xb_tool_add (GPtrArray *array,
	     const gchar *name,
	     const gchar *arguments,
	     const gchar *description,
	     FuUtilPrivateCb callback)
{
	g_auto(GStrv) names = NULL;

	g_return_if_fail (name != NULL);
	g_return_if_fail (description != NULL);
	g_return_if_fail (callback != NULL);

	/* add each one */
	names = g_strsplit (name, ",", -1);
	for (guint i = 0; names[i] != NULL; i++) {
		FuUtilItem *item = g_new0 (FuUtilItem, 1);
		item->name = g_strdup (names[i]);
		if (i == 0) {
			item->description = g_strdup (description);
		} else {
			/* TRANSLATORS: this is a command alias, e.g. 'get-devices' */
			item->description = g_strdup_printf ("Alias to %s",
							     names[0]);
		}
		item->arguments = g_strdup (arguments);
		item->callback = callback;
		g_ptr_array_add (array, item);
	}
}

static void
xb_tool_cancelled_cb (GCancellable *cancellable, gpointer user_data)
{
	XbToolPrivate *priv = (XbToolPrivate *) user_data;
	g_print ("Cancelled!\n");
	g_main_loop_quit (priv->loop);
}

static gchar *
xb_tool_get_descriptions (GPtrArray *array)
{
	gsize len;
	const gsize max_len = 31;
	FuUtilItem *item;
	GString *string;

	/* print each command */
	string = g_string_new ("");
	for (guint i = 0; i < array->len; i++) {
		item = g_ptr_array_index (array, i);
		g_string_append (string, "  ");
		g_string_append (string, item->name);
		len = strlen (item->name) + 2;
		if (item->arguments != NULL) {
			g_string_append (string, " ");
			g_string_append (string, item->arguments);
			len += strlen (item->arguments) + 1;
		}
		if (len < max_len) {
			for (guint j = len; j < max_len + 1; j++)
				g_string_append_c (string, ' ');
			g_string_append (string, item->description);
			g_string_append_c (string, '\n');
		} else {
			g_string_append_c (string, '\n');
			for (guint j = 0; j < max_len + 1; j++)
				g_string_append_c (string, ' ');
			g_string_append (string, item->description);
			g_string_append_c (string, '\n');
		}
	}

	/* remove trailing newline */
	if (string->len > 0)
		g_string_set_size (string, string->len - 1);

	return g_string_free (string, FALSE);
}

static gboolean
xb_tool_run (XbToolPrivate *priv,
	     const gchar *command,
	     gchar **values,
	     GError **error)
{
	/* find command */
	for (guint i = 0; i < priv->cmd_array->len; i++) {
		FuUtilItem *item = g_ptr_array_index (priv->cmd_array, i);
		if (g_strcmp0 (item->name, command) == 0)
			return item->callback (priv, values, error);
	}

	/* not found */
	g_set_error_literal (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "Command not found");
	return FALSE;
}

static gboolean
xb_tool_dump (XbToolPrivate *priv, gchar **values, GError **error)
{
	XbSiloLoadFlags flags = XB_SILO_LOAD_FLAG_NONE;

	/* check args */
	if (g_strv_length (values) < 1) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "Invalid arguments, expected "
				     "FILENAME"
				     " -- e.g. `example.xmlb`");
		return FALSE;
	}

	/* don't check the magic to make fuzzing easier */
	if (priv->force)
		flags |= XB_SILO_LOAD_FLAG_NO_MAGIC;

	/* load blobs */
	for (guint i = 0; values[i] != NULL; i++) {
		g_autofree gchar *str = NULL;
		g_autoptr(GFile) file = g_file_new_for_path (values[0]);
		g_autoptr(XbSilo) silo = xb_silo_new ();
		if (!xb_silo_load_from_file (silo, file, flags, NULL, error))
			return FALSE;
		str = xb_silo_to_string (silo, error);
		if (str == NULL)
			return FALSE;
		g_print ("%s", str);
	}
	return TRUE;
}

static gboolean
xb_tool_export (XbToolPrivate *priv, gchar **values, GError **error)
{
	XbSiloLoadFlags flags = XB_SILO_LOAD_FLAG_NONE;

	/* check args */
	if (g_strv_length (values) < 1) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "Invalid arguments, expected "
				     "FILENAME"
				     " -- e.g. `example.xmlb`");
		return FALSE;
	}

	/* don't check the magic to make fuzzing easier */
	if (priv->force)
		flags |= XB_SILO_LOAD_FLAG_NO_MAGIC;

	/* load blobs */
	for (guint i = 0; values[i] != NULL; i++) {
		g_autofree gchar *str = NULL;
		g_autoptr(GFile) file = g_file_new_for_path (values[0]);
		g_autoptr(XbSilo) silo = xb_silo_new ();
		if (!xb_silo_load_from_file (silo, file, flags, NULL, error))
			return FALSE;
		str = xb_silo_export (silo,
				      XB_NODE_EXPORT_FLAG_ADD_HEADER |
				      XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE |
				      XB_NODE_EXPORT_FLAG_FORMAT_INDENT |
				      XB_NODE_EXPORT_FLAG_INCLUDE_SIBLINGS,
				      error);
		if (str == NULL)
			return FALSE;
		g_print ("%s", str);
	}
	return TRUE;
}

static gboolean
xb_tool_query (XbToolPrivate *priv, gchar **values, GError **error)
{
	guint limit = 0;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GPtrArray) results = NULL;
	g_autoptr(XbSilo) silo = xb_silo_new ();

	/* check args */
	if (g_strv_length (values) < 2) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "Invalid arguments, expected "
				     "FILENAME QUERY [LIMIT]"
				     " -- e.g. `example.xmlb`");
		return FALSE;
	}

	/* load blob */
	file = g_file_new_for_path (values[0]);
	if (priv->profile) {
		xb_silo_set_profile_flags (silo,
					   XB_SILO_PROFILE_FLAG_XPATH |
					   XB_SILO_PROFILE_FLAG_APPEND);
	}
	if (!xb_silo_load_from_file (silo, file, XB_SILO_LOAD_FLAG_NONE, NULL, error))
		return FALSE;

	/* parse optional limit */
	if (g_strv_length (values) == 3)
		limit = g_ascii_strtoull (values[2], NULL, 10);

	/* query */
	results = xb_silo_query (silo, values[1], limit, error);
	if (results == NULL)
		return FALSE;
	for (guint i = 0; i < results->len; i++) {
		XbNode *n = g_ptr_array_index (results, i);
		g_autofree gchar *xml = NULL;
		xml = xb_node_export (n,
				      XB_NODE_EXPORT_FLAG_FORMAT_MULTILINE |
				      XB_NODE_EXPORT_FLAG_FORMAT_INDENT,
				      error);
		if (xml == NULL)
			return FALSE;
		g_print ("RESULT: %s\n", xml);
	}

	/* profile */
	if (priv->profile)
		g_print ("%s", xb_silo_get_profile_string (silo));

	return TRUE;
}

static gboolean
xb_tool_query_file (XbToolPrivate *priv, gchar **values, GError **error)
{
	g_autoptr(GFile) file = NULL;
	g_autoptr(XbSilo) silo = xb_silo_new ();

	/* check args */
	if (g_strv_length (values) < 2) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "Invalid arguments, expected "
				     "FILENAME FILENAME");
		return FALSE;
	}

	/* load blob */
	file = g_file_new_for_path (values[0]);
	if (!xb_silo_load_from_file (silo, file, XB_SILO_LOAD_FLAG_NONE, NULL, error))
		return FALSE;

	/* optionally load file */
	for (guint i = 1; values[i] != NULL; i++) {
		g_autofree gchar *xpath = NULL;
		g_autoptr(GPtrArray) results = NULL;
		g_autoptr(GError) error_local = NULL;

		/* load XPath from file */
		if (!g_file_get_contents (values[i], &xpath, NULL, error))
			return FALSE;
		g_strdelimit (xpath, "\n", '\0');

		/* query */
		results = xb_silo_query (silo, xpath, 0, &error_local);
		if (results == NULL) {
			g_print ("FAILED: %s\n", error_local->message);
			continue;
		}
		for (guint j = 0; j < results->len; j++) {
			XbNode *n = g_ptr_array_index (results, j);
			g_autofree gchar *xml = NULL;
			xml = xb_node_export (n, XB_NODE_EXPORT_FLAG_NONE, error);
			if (xml == NULL)
				return FALSE;
			g_print ("RESULT: %s\n", xml);
		}
	}

	/* profile */
	if (priv->profile)
		g_print ("%s", xb_silo_get_profile_string (silo));

	return TRUE;
}

static void
xb_tool_silo_invalidated_cb (XbSilo *silo, GParamSpec *pspec, gpointer user_data)
{
	XbToolPrivate *priv = (XbToolPrivate *) user_data;
	g_main_loop_quit (priv->loop);
}

static gboolean
xb_tool_builder_tokenize_cb (XbBuilderFixup *self,
			     XbBuilderNode *bn,
			     gpointer user_data,
			     GError **error)
{
	XbToolPrivate *priv = (XbToolPrivate *) user_data;
	for (guint i = 0; priv->tokenize != NULL && priv->tokenize[i] != NULL; i++) {
		if (g_strcmp0 (xb_builder_node_get_element (bn), priv->tokenize[i]) == 0) {
			xb_builder_node_tokenize_text (bn);
			break;
		}
	}
	return TRUE;
}

static gboolean
xb_tool_compile (XbToolPrivate *priv, gchar **values, GError **error)
{
	const gchar *const *locales = g_get_language_names ();
	g_autoptr(XbBuilder) builder = xb_builder_new ();
	g_autoptr(XbSilo) silo = NULL;
	g_autoptr(GFile) file_dst = NULL;

	/* check args */
	if (g_strv_length (values) < 2) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "Invalid arguments, expected "
				     "FILE-OUT FILE [FILE]"
				     " -- e.g. `example.xmlb example.xml`");
		return FALSE;
	}

	/* load file */
	for (guint i = 0; locales[i] != NULL; i++)
		xb_builder_add_locale (builder, locales[i]);

	for (guint i = 1; values[i] != NULL; i++) {
		g_autoptr(GFile) file = g_file_new_for_path (values[i]);
		g_autoptr(XbBuilderSource) source = xb_builder_source_new ();
		if (priv->tokenize != NULL) {
			g_autoptr(XbBuilderFixup) fixup = NULL;
			fixup = xb_builder_fixup_new ("TextTokenize",
						      xb_tool_builder_tokenize_cb,
						      priv, NULL);
			xb_builder_source_add_fixup (source, fixup);
		}
		if (!xb_builder_source_load_file (source, file,
						  XB_BUILDER_SOURCE_FLAG_WATCH_FILE |
						  XB_BUILDER_SOURCE_FLAG_LITERAL_TEXT,
						  NULL, error))
			return FALSE;
		xb_builder_import_source (builder, source);
	}
	file_dst = g_file_new_for_path (values[0]);
	xb_builder_set_profile_flags (builder,
				      priv->profile ? XB_SILO_PROFILE_FLAG_APPEND :
						      XB_SILO_PROFILE_FLAG_NONE);
	silo = xb_builder_ensure (builder, file_dst,
				  XB_BUILDER_COMPILE_FLAG_WATCH_BLOB |
				  XB_BUILDER_COMPILE_FLAG_IGNORE_INVALID |
				  XB_BUILDER_COMPILE_FLAG_NATIVE_LANGS,
				  NULL, error);
	if (silo == NULL)
		return FALSE;

	/* wait for invalidation */
	if (priv->wait) {
		g_print ("Waiting for invalidation…\n");
		g_signal_connect (silo, "notify::valid",
				  G_CALLBACK (xb_tool_silo_invalidated_cb),
				  priv);
		g_main_loop_run (priv->loop);
	}

	/* profile */
	if (priv->profile)
		g_print ("%s", xb_silo_get_profile_string (silo));

	/* success */
	return TRUE;
}

#ifdef HAVE_GIO_UNIX
static gboolean
xb_tool_sigint_cb (gpointer user_data)
{
	XbToolPrivate *priv = (XbToolPrivate *) user_data;
	g_debug ("Handling SIGINT");
	g_cancellable_cancel (priv->cancellable);
	return FALSE;
}
#endif

int
main (int argc, char *argv[])
{
	gboolean ret;
	gboolean verbose = FALSE;
	g_autofree gchar *cmd_descriptions = NULL;
	g_autoptr(XbToolPrivate) priv = g_new0 (XbToolPrivate, 1);
	g_autoptr(GError) error = NULL;
	g_autoptr(GOptionContext) context = NULL;
	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			"Print verbose debug statements", NULL },
		{ "force", 'v', 0, G_OPTION_ARG_NONE, &priv->force,
			"Force parsing of invalid files", NULL },
		{ "wait", 'w', 0, G_OPTION_ARG_NONE, &priv->wait,
			"Return only when the silo is no longer valid", NULL },
		{ "profile", 'p', 0, G_OPTION_ARG_NONE, &priv->profile,
			"Show profiling information", NULL },
		{ "tokenize", 'p', 0, G_OPTION_ARG_STRING_ARRAY, &priv->tokenize,
			"Tokenize elements for faster search", NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	/* do not let GIO start a session bus */
	g_setenv ("GIO_USE_VFS", "local", 1);

	/* add commands */
	priv->cmd_array = g_ptr_array_new_with_free_func ((GDestroyNotify) xb_tool_item_free);
	xb_tool_add (priv->cmd_array,
		     "dump",
		     "XMLBFILE",
		     /* TRANSLATORS: command description */
		     "Dumps a XMLb file",
		     xb_tool_dump);
	xb_tool_add (priv->cmd_array,
		     "export",
		     "XMLFILE",
		     /* TRANSLATORS: command description */
		     "Exports a XMLb file",
		     xb_tool_export);
	xb_tool_add (priv->cmd_array,
		     "query",
		     "XMLBFILE XPATH [LIMIT]",
		     /* TRANSLATORS: command description */
		     "Queries a XMLb file",
		     xb_tool_query);
	xb_tool_add (priv->cmd_array,
		     "query-file",
		     "XMLBFILE [FILE] [FILE]",
		     /* TRANSLATORS: command description */
		     "Queries a XMLb file using an external XPath query",
		     xb_tool_query_file);
	xb_tool_add (priv->cmd_array,
		     "compile",
		     "XMLBFILE XMLFILE [XMLFILE]",
		     /* TRANSLATORS: command description */
		     "Compile XML to XMLb",
		     xb_tool_compile);

	/* do stuff on ctrl+c */
	priv->loop = g_main_loop_new (NULL, FALSE);
	priv->cancellable = g_cancellable_new ();
	g_signal_connect (priv->cancellable, "cancelled",
			  G_CALLBACK (xb_tool_cancelled_cb), priv);
#ifdef HAVE_GIO_UNIX
	g_unix_signal_add_full (G_PRIORITY_DEFAULT,
				SIGINT, xb_tool_sigint_cb,
				priv, NULL);
#endif

	/* sort by command name */
	g_ptr_array_sort (priv->cmd_array,
			  (GCompareFunc) xb_tool_sort_command_name_cb);

	/* get a list of the commands */
	context = g_option_context_new (NULL);
	cmd_descriptions = xb_tool_get_descriptions (priv->cmd_array);
	g_option_context_set_summary (context, cmd_descriptions);

	/* TRANSLATORS: DFU stands for device firmware update */
	g_set_application_name ("Binary XML Utility");
	g_option_context_add_main_entries (context, options, NULL);
	ret = g_option_context_parse (context, &argc, &argv, &error);
	if (!ret) {
		g_print ("%s: %s\n", "Failed to parse arguments", error->message);
		return EXIT_FAILURE;
	}

	/* set verbose? */
	if (verbose)
		g_setenv ("G_MESSAGES_DEBUG", "all", FALSE);

	/* run the specified command */
	ret = xb_tool_run (priv, argv[1], (gchar**) &argv[2], &error);
	if (!ret) {
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_FAILED)) {
			g_autofree gchar *tmp = NULL;
			tmp = g_option_context_get_help (context, TRUE, NULL);
			g_print ("%s\n\n%s", error->message, tmp);
		} else {
			g_print ("%s\n", error->message);
		}
		return EXIT_FAILURE;
	}

	/* success/ */
	return EXIT_SUCCESS;
}
