/* SandboxUtils -- Sandbox Utils common functions
 * Copyright (c) Steve Dodier-Lazaro <sidnioulz@gmail.com>, 2014
 * 
 * Under GPLv3
 * 
 *** 
 * 
 * TODO
 */

#include <syslog.h>

#include "sandboxutilscommon.h"

/**
 * sandboxutils_error_get_message:
 * @err: a GError or %NULL
 *
 * Retrieves the message inside a GError in a safe way, testing for
 * %NULL-pointers beforehand. This function will always return a string, so you
 * can use it for logging error messages.
 *
 * Returns: the message of a GError if it is not %NULL, or returns "(no error)"
 * otherwise.
 *
 * Since: 0.3
 */
const gchar *
sandboxutils_error_get_message (GError *err)
{
  if (err)
    return err->message;
  else
    return "(no error)";
}

/**
 * sandboxutils_min:
 * @a: an integer
 * @b: another integer
 *
 * Returns the smallest of two signed integers.
 *
 * Returns: the smallest integer between a and b.
 *
 * Since: 0.3
 */
const int
sandboxutils_min (int a, int b)
{
  return a<b? a:b;
}

/**
 * sandboxutils_max:
 * @a: an integer
 * @b: another integer
 *
 * Returns the largest of two signed integers.
 *
 * Returns: the largest integer between a and b.
 *
 * Since: 0.3
 */
const int
sandboxutils_max (int a, int b)
{
  return a>b? a:b;
}

static gboolean _option_sandbox = TRUE;

static GOptionEntry entries[] =
{
  {
    "no-sandbox", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &_option_sandbox,
    "Disable sandbox-compatible file choosers, which will be replaced by vanilla GTK+ widgets running with the app's privileges", NULL
  },
  {
    "sandbox", 0, 0, G_OPTION_ARG_NONE, &_option_sandbox,
    "Enables sandbox-compatible file choosers, running with higher privileges than the app", NULL
  },
  {
    NULL, ' ', 0, 0, NULL,
    NULL, NULL
  }
};

/**
 * sandboxutils_get_option_group:
 *
 * Returns a #GOptionGroup for the commandline arguments recognized
 * by Sandbox Utils.
 *
 * You should add this group to your #GOptionContext
 * with g_option_context_add_group(), if you are using
 * g_option_context_parse() to parse your commandline arguments.
 *
 * Returns: a #GOptionGroup for the commandline arguments recognized
 * by SandboxUtils. It should be added to a #GOptionContext or freed with 
 * g_option_group_free().
 *
 * Since: 0.6
 */
GOptionGroup *
sandboxutils_get_option_group ()
{
  GOptionGroup *group;

  group = g_option_group_new ("sandboxutils", "Sandbox Utilities Options", "Show Sandbox Utilities Options", NULL, NULL);

  g_option_group_add_entries (group, entries);
  g_option_group_set_translation_domain (group, NULL);
  
  return group;
}

/**
 * sandboxutils_parse_args:
 * @argc: (inout): a pointer to the number of command line arguments
 * @argv: (array length=argc) (inout): a pointer to the array of
 *     command line arguments
 *
 * Parses command line arguments, and initializes global
 * attributes of Sandbox Utils.
 *
 * Any arguments used by Sandbox Utils are removed from the array and
 * @argc and @argv are updated accordingly.
 *
 * In most GTK+ applications, you already have a #GOptionContext set to parse
 * your own application's options and GTK+ options. In such a case, it is
 * preferable to retrieve the Sandbox Utils #GOptionGroup (see
 * sandboxutils_get_option_group()) and attach it to your #GOptionContext (see
 * g_option_context_add_group() and g_option_context_parse()) just like you
 * would do for GTK+ options (see gtk_get_option_group()).
 *
 * Return value: %TRUE if initialization succeeded, otherwise %FALSE
 *
 * Since: 0.6
 */
gboolean
sandboxutils_parse_args (int    *argc,
                         char  **argv[])
{
  GOptionContext  *context   = g_option_context_new (NULL);
  GOptionGroup    *group     = sandboxutils_get_option_group ();
  GError          *error     = NULL;
  gboolean         succeeded = TRUE;

  g_option_context_set_ignore_unknown_options (context, TRUE);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_main_group (context, group);

  if (!g_option_context_parse (context, argc, argv, &error))
  {
    syslog (LOG_WARNING, "SandboxUtils.Init: could not parse arguments (%s)", 
            error->message);
    g_error_free (error);
    succeeded = FALSE;
  }

  // Will free option group automatically as well.
  g_option_context_free (context);

  return succeeded;
}

/**
 * sandboxutils_get_sandboxed:
 *
 * Gets whether Sandbox Utils widgets will be remote sandbox-traversing widgets
 * or local unprivileged GTK+ widgets.
 *
 * Return value: %TRUE if Sandbox Utils will use sandbox-compatible widgets,
 * %FALSE otherwise.
 *
 * Since: 0.6
 */
gboolean
sandboxutils_get_sandboxed ()
{
  return _option_sandbox;
}

/**
 * sandboxutils_set_sandboxed:
 * @sandbox: %TRUE if widgets should be sandbox-traversing, %FALSE otherwise
 *
 * Sets whether Sandbox Utils widgets will be remote sandbox-traversing widgets
 * or local unprivileged GTK+ widgets.
 *
 * Since: 0.6
 */
void
sandboxutils_set_sandboxed (gboolean sandbox)
{
  _option_sandbox = sandbox;
}

/**
 * sandboxutils_init:
 * @argc: (inout): a pointer to the number of command line arguments
 * @argv: (array length=argc) (inout): a pointer to the array of
 *     command line arguments
 *
 * This is an alias to sandboxutils_parse_args().
 *
 * Since: 0.6
 */
void
sandboxutils_init (int *argc, char **argv[])
{
  if (!sandboxutils_parse_args (argc, argv))
    sandboxutils_set_sandboxed (TRUE);
}

