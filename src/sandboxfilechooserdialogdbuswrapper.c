/* SandboxUtils -- Sandbox Utilities DBus Manager
 * Copyright (c) Steve Dodier-Lazaro <sidnioulz@gmail.com>, 2014
 * 
 * Under GPLv3
 * 
 *** 
 * 
 * Class desc here
 * XXX this class describes ...
 * 
 */

#include <syslog.h>

#include "sandboxfilechooserdialogdbuswrapper.h"


/*
 * TODO doc
 */
static SandboxFileChooserDialog *
_sfcd_dbus_wrapper_lookup (SandboxUtilsClient  *cli,
                           const gchar         *dialog_id)
{
  SandboxFileChooserDialog *sfcd = NULL;

  g_return_val_if_fail (cli != NULL, NULL);
  g_return_val_if_fail (dialog_id != NULL, NULL);

  g_mutex_lock (&cli->dialogsMutex);
  sfcd = g_hash_table_lookup (cli->dialogs, dialog_id);

	if (sfcd == NULL)
	{
	  syslog (LOG_WARNING,
	          "SfcdDbusWrapper._Lookup: dialog '%s' was not found.\n",
	          dialog_id);
	}
  else
  {
    // Prevents the object from being deleted by a concurrent thread while in use
    g_object_ref (sfcd);
  }

  g_mutex_unlock (&cli->dialogsMutex);

  return sfcd;
}

/*
 * TODO doc
 */
static SandboxFileChooserDialog *
_sfcd_dbus_wrapper_lookup_and_remove (SandboxUtilsClient  *cli,
                                      const gchar         *dialog_id)
{
  //FIXME adapt to new template
  SandboxFileChooserDialog *sfcd = NULL;

  g_mutex_lock (&cli->dialogsMutex);
  sfcd = g_hash_table_lookup (cli->dialogs, dialog_id);

	if (sfcd == NULL)
	{
	  syslog (LOG_WARNING,
	          "SfcdDbusWrapper._LookupAndRemove: dialog '%s' was not found, cannot be removed.\n",
	          dialog_id);
	}
  else
  {
    // Prevents the object from being deleted by a concurrent thread while in use
    g_object_ref (sfcd);

    if (!g_hash_table_remove (cli->dialogs, dialog_id))
    {
	    syslog (LOG_CRIT,
	            "SfcdDbusWrapper._LookupAndRemove: dialog '%s' was found but could not be removed  (this should never happen, please report a bug).\n",
	            dialog_id);
	    sfcd = NULL;
    }
  }

  g_mutex_unlock (&cli->dialogsMutex);

  return sfcd;
}

/*
 * TODO doc
 */
static void
_sfcd_dbus_wrapper_lookup_finished (GDBusMethodInvocation    *invocation,
                                    SandboxFileChooserDialog *sfcd,
                                    const gchar              *dialog_id)
{
  if (sfcd)
  {
    // Removes the deletion-preventing reference added by our lookup function
    g_object_unref (sfcd);
  }
  else
  {
    // Report the failure of the lookup function
    g_dbus_method_invocation_return_error (invocation,
                                           G_DBUS_ERROR,
                                           SFCD_ERROR_LOOKUP,
                                           "SfcdDbusWrapper._Lookup: dialog '%s' was not found.\n",
                                           dialog_id);
  }
}

/*
 * TODO doc
 */
static void
_sfcd_dbus_wrapper_return_error (GDBusMethodInvocation    *invocation,
                                 GError                   *error)
{
  syslog (LOG_CRIT, "SfcdDbusWrapper.Dbus._Handler: %s", g_error_get_message (error));

  g_dbus_method_invocation_return_error (invocation,
                                         G_DBUS_ERROR,
                                         G_DBUS_ERROR_FAILED,
                                         "SfcdDbusWrapper.Dbus._Handler: %s",
                                         g_error_get_message (error));
  g_error_free (error);
}

//TODO listen to signals on sfcd's and then emit GDBus signals
// run-finished without destroy: forward
// run-finished with destroy: clean-up client's table

static gboolean
on_handle_new (SfcdDbusWrapper        *interface,
               GDBusMethodInvocation  *invocation,
               const gchar            *title,
               const gchar            *parent_id,
               const gint              action,
               GVariant               *button_list,
               gpointer                user_data)
{
  SandboxUtilsClient         *cli        = user_data;
  SandboxFileChooserDialog   *sfcd       = NULL;
  GtkWidget                  *dialog     = NULL;
  gchar                      *dialog_id  = NULL;
  GVariant                   *item       = NULL;
  GVariantIter               *iter       = NULL;
  GError                     *error      = NULL;

  // Create GTK+ dialog
  dialog = gtk_file_chooser_dialog_new (title, NULL, action, NULL, NULL);
	if (dialog==NULL)
	{
		g_set_error (&error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SFCD_ERROR_CREATION,
					"SfcdDbusWrapper.Sfcd.New: could not allocate memory to create GtkFileChooserDialog.\n");
		_sfcd_dbus_wrapper_return_error (invocation, error);

		return TRUE;
	}

	// Populate dialog with buttons
  g_variant_get (button_list, "a{sv}", &iter);
	while ((item = g_variant_iter_next_value (iter)))
  {
    const gchar *key;
    GVariant *value;

    g_variant_get (item, "{sv}", &key, &value);
    gtk_dialog_add_button (GTK_DIALOG (dialog), key, g_variant_get_int32 (value));
  }

  // Create SandboxUtils dialog
  sfcd = sfcd_new (dialog);
  if (sfcd==NULL)
  {
	  g_set_error (&error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SFCD_ERROR_CREATION,
				  "SfcdDbusWrapper.Sfcd.New: could not allocate memory to create SandboxFileChooserDialog.\n");
		_sfcd_dbus_wrapper_return_error (invocation, error);

	  return TRUE;
  }

  // Store dialog in the client's table and return its id
  dialog_id = g_strdup_printf ("%p", sfcd);
  gchar *key = g_strdup (dialog_id);

  g_mutex_lock (&cli->dialogsMutex);
  g_hash_table_insert (cli->dialogs, key, sfcd);
  g_mutex_unlock (&cli->dialogsMutex);

  sfcd_dbus_wrapper__complete_new (interface, invocation, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_state (SfcdDbusWrapper        *interface,
                     GDBusMethodInvocation  *invocation,
                     const gchar            *dialog_id,
                     gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    SfcdState state = sfcd_get_state (sfcd);
    sfcd_dbus_wrapper__complete_get_state (interface, invocation, state);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_destroy (SfcdDbusWrapper        *interface,
                   GDBusMethodInvocation  *invocation,
                   const gchar            *dialog_id,
                   gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;

  if ((sfcd = _sfcd_dbus_wrapper_lookup_and_remove (cli, dialog_id)) != NULL)
  {
    sfcd_destroy (sfcd);
    sfcd_dbus_wrapper__complete_destroy (interface, invocation);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

//TODO destroy

static gboolean
on_handle_run (SfcdDbusWrapper        *interface,
               GDBusMethodInvocation  *invocation,
               const gchar            *dialog_id,
               gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_run (sfcd, &error))
      sfcd_dbus_wrapper__complete_run (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_present (SfcdDbusWrapper        *interface,
                   GDBusMethodInvocation  *invocation,
                   const gchar            *dialog_id,
                   gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_present (sfcd, &error))
      sfcd_dbus_wrapper__complete_present (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_cancel_run (SfcdDbusWrapper        *interface,
                      GDBusMethodInvocation  *invocation,
                      const gchar            *dialog_id,
                      gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_cancel_run (sfcd, &error))
      sfcd_dbus_wrapper__complete_cancel_run (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_set_action (SfcdDbusWrapper        *interface,
                      GDBusMethodInvocation  *invocation,
                      const gchar            *dialog_id,
                      const gint              action,
                      gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_set_action (sfcd, action, &error))
      sfcd_dbus_wrapper__complete_set_action (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_action (SfcdDbusWrapper        *interface,
                      GDBusMethodInvocation  *invocation,
                      const gchar            *dialog_id,
                      gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gint action;

    if (sfcd_get_action (sfcd, &action, &error))
      sfcd_dbus_wrapper__complete_get_action (interface, invocation, action);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_set_local_only (SfcdDbusWrapper        *interface,
                          GDBusMethodInvocation  *invocation,
                          const gchar            *dialog_id,
                          const gboolean          local_only,
                          gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_set_local_only (sfcd, local_only, &error))
      sfcd_dbus_wrapper__complete_set_local_only (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_local_only (SfcdDbusWrapper        *interface,
                          GDBusMethodInvocation  *invocation,
                          const gchar            *dialog_id,
                          gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gboolean local_only;

    if (sfcd_get_local_only (sfcd, &local_only, &error))
      sfcd_dbus_wrapper__complete_get_local_only (interface, invocation, local_only);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_set_select_multiple (SfcdDbusWrapper        *interface,
                               GDBusMethodInvocation  *invocation,
                               const gchar            *dialog_id,
                               const gboolean          select_multiple,
                               gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_set_select_multiple (sfcd, select_multiple, &error))
      sfcd_dbus_wrapper__complete_set_select_multiple (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_select_multiple (SfcdDbusWrapper        *interface,
                               GDBusMethodInvocation  *invocation,
                               const gchar            *dialog_id,
                               gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gboolean select_multiple;

    if (sfcd_get_select_multiple (sfcd, &select_multiple, &error))
      sfcd_dbus_wrapper__complete_get_select_multiple (interface, invocation, select_multiple);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_set_show_hidden (SfcdDbusWrapper        *interface,
                           GDBusMethodInvocation  *invocation,
                           const gchar            *dialog_id,
                           const gboolean          show_hidden,
                           gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_set_show_hidden (sfcd, show_hidden, &error))
      sfcd_dbus_wrapper__complete_set_show_hidden (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_show_hidden (SfcdDbusWrapper        *interface,
                           GDBusMethodInvocation  *invocation,
                           const gchar            *dialog_id,
                           gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gboolean show_hidden;

    if (sfcd_get_show_hidden (sfcd, &show_hidden, &error))
      sfcd_dbus_wrapper__complete_get_show_hidden (interface, invocation, show_hidden);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_set_current_name (SfcdDbusWrapper        *interface,
                           GDBusMethodInvocation  *invocation,
                           const gchar            *dialog_id,
                           const gchar            *current_name,
                           gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_set_current_name (sfcd, current_name, &error))
      sfcd_dbus_wrapper__complete_set_current_name (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_set_filename (SfcdDbusWrapper        *interface,
                        GDBusMethodInvocation  *invocation,
                        const gchar            *dialog_id,
                        const gchar            *filename,
                        gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_set_filename (sfcd, filename, &error))
      sfcd_dbus_wrapper__complete_set_filename (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_set_current_folder (SfcdDbusWrapper        *interface,
                              GDBusMethodInvocation  *invocation,
                              const gchar            *dialog_id,
                              const gchar            *current_folder,
                              gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_set_current_folder (sfcd, current_folder, &error))
      sfcd_dbus_wrapper__complete_set_current_folder (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_set_uri (SfcdDbusWrapper        *interface,
                   GDBusMethodInvocation  *invocation,
                   const gchar            *dialog_id,
                   const gchar            *uri,
                   gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_set_uri (sfcd, uri, &error))
      sfcd_dbus_wrapper__complete_set_uri (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_set_current_folder_uri (SfcdDbusWrapper        *interface,
                                  GDBusMethodInvocation  *invocation,
                                  const gchar            *dialog_id,
                                  const gchar            *current_folder_uri,
                                  gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_set_current_folder_uri (sfcd, current_folder_uri, &error))
      sfcd_dbus_wrapper__complete_set_current_folder_uri (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_add_shortcut_folder (SfcdDbusWrapper        *interface,
                               GDBusMethodInvocation  *invocation,
                               const gchar            *dialog_id,
                               const gchar            *folder,
                               gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_add_shortcut_folder (sfcd, folder, &error))
      sfcd_dbus_wrapper__complete_add_shortcut_folder (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_remove_shortcut_folder (SfcdDbusWrapper        *interface,
                                  GDBusMethodInvocation  *invocation,
                                  const gchar            *dialog_id,
                                  const gchar            *folder,
                                  gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_remove_shortcut_folder (sfcd, folder, &error))
      sfcd_dbus_wrapper__complete_remove_shortcut_folder (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_list_shortcut_folders (SfcdDbusWrapper        *interface,
                                 GDBusMethodInvocation  *invocation,
                                 const gchar            *dialog_id,
                                 gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    GSList *list = NULL;

    if (sfcd_list_shortcut_folders (sfcd, &list, &error))
    {
      // Allocate for the list and a NULL element at the end
      const gchar **dbus_list = g_malloc (sizeof (gchar *) * (g_slist_length (list) + 1));

      GSList *iter = list;
      guint32 ind  = 0;
      while (iter)
      {
        dbus_list[ind++] = iter->data;
        iter = iter->next;
      }
      dbus_list[ind] = NULL;

      sfcd_dbus_wrapper__complete_list_shortcut_folders (interface, invocation, dbus_list);
      g_free (dbus_list);
      g_slist_free_full (list, g_free);
    }
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_add_shortcut_folder_uri (SfcdDbusWrapper        *interface,
                                   GDBusMethodInvocation  *invocation,
                                   const gchar            *dialog_id,
                                   const gchar            *uri,
                                   gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_add_shortcut_folder_uri (sfcd, uri, &error))
      sfcd_dbus_wrapper__complete_add_shortcut_folder_uri (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_remove_shortcut_folder_uri (SfcdDbusWrapper        *interface,
                                      GDBusMethodInvocation  *invocation,
                                      const gchar            *dialog_id,
                                      const gchar            *uri,
                                      gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    if (sfcd_remove_shortcut_folder_uri (sfcd, uri, &error))
      sfcd_dbus_wrapper__complete_remove_shortcut_folder_uri (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_list_shortcut_folder_uris (SfcdDbusWrapper        *interface,
                                     GDBusMethodInvocation  *invocation,
                                     const gchar            *dialog_id,
                                     gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    GSList *list = NULL;

    if (sfcd_list_shortcut_folder_uris (sfcd, &list, &error))
    {
      // Allocate for the list and a NULL element at the end
      const gchar **dbus_list = g_malloc (sizeof (gchar *) * (g_slist_length (list) + 1));

      GSList *iter = list;
      guint32 ind  = 0;
      while (iter)
      {
        dbus_list[ind++] = iter->data;
        iter = iter->next;
      }
      dbus_list[ind] = NULL;

      sfcd_dbus_wrapper__complete_list_shortcut_folder_uris (interface, invocation, dbus_list);
      g_free (dbus_list);
      g_slist_free_full (list, g_free);
    }
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_current_name (SfcdDbusWrapper        *interface,
                            GDBusMethodInvocation  *invocation,
                            const gchar            *dialog_id,
                            gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gchar *name;
    
    if (sfcd_get_current_name (sfcd, &name, &error))
    {
      sfcd_dbus_wrapper__complete_get_current_name (interface, invocation, name);
      g_free (name);
    }
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_filename (SfcdDbusWrapper        *interface,
                        GDBusMethodInvocation  *invocation,
                        const gchar            *dialog_id,
                        gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gchar *filename;
    
    if (sfcd_get_filename (sfcd, &filename, &error))
    {
      sfcd_dbus_wrapper__complete_get_filename (interface, invocation, filename);
      g_free (filename);
    }
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_filenames (SfcdDbusWrapper        *interface,
                         GDBusMethodInvocation  *invocation,
                         const gchar            *dialog_id,
                         gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    GSList *list = NULL;

    if (sfcd_get_filenames (sfcd, &list, &error))
    {
      // Allocate for the list and a NULL element at the end
      const gchar **dbus_list = g_malloc (sizeof (gchar *) * (g_slist_length (list) + 1));

      GSList *iter = list;
      guint32 ind  = 0;
      while (iter)
      {
        dbus_list[ind++] = iter->data;
        iter = iter->next;
      }
      dbus_list[ind] = NULL;

      sfcd_dbus_wrapper__complete_get_filenames (interface, invocation, dbus_list);
      g_free (dbus_list);
      g_slist_free_full (list, g_free);
    }
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_current_folder (SfcdDbusWrapper        *interface,
                              GDBusMethodInvocation  *invocation,
                              const gchar            *dialog_id,
                              gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gchar *current_folder;
    
    if (sfcd_get_current_folder (sfcd, &current_folder, &error))
    {
      sfcd_dbus_wrapper__complete_get_current_folder (interface, invocation, current_folder);
      g_free (current_folder);
    }
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_uri (SfcdDbusWrapper        *interface,
                   GDBusMethodInvocation  *invocation,
                   const gchar            *dialog_id,
                   gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gchar *uri;
    
    if (sfcd_get_uri (sfcd, &uri, &error))
    {
      sfcd_dbus_wrapper__complete_get_uri (interface, invocation, uri);
      g_free (uri);
    }
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_uris (SfcdDbusWrapper        *interface,
                    GDBusMethodInvocation  *invocation,
                    const gchar            *dialog_id,
                    gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    GSList *list = NULL;

    if (sfcd_get_uris (sfcd, &list, &error))
    {
      // Allocate for the list and a NULL element at the end
      const gchar **dbus_list = g_malloc (sizeof (gchar *) * (g_slist_length (list) + 1));

      GSList *iter = list;
      guint32 ind  = 0;
      while (iter)
      {
        dbus_list[ind++] = iter->data;
        iter = iter->next;
      }
      dbus_list[ind] = NULL;

      sfcd_dbus_wrapper__complete_get_uris (interface, invocation, dbus_list);
      g_free (dbus_list);
      g_slist_free_full (list, g_free);
    }
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_current_folder_uri (SfcdDbusWrapper        *interface,
                                  GDBusMethodInvocation  *invocation,
                                  const gchar            *dialog_id,
                                  gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SandboxUtilsClient         *cli        = user_data;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gchar *uri;
    
    if (sfcd_get_current_folder_uri (sfcd, &uri, &error))
    {
      sfcd_dbus_wrapper__complete_get_current_folder_uri (interface, invocation, uri);
      g_free (uri);
    }
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}



static void
sfcd_dbus_on_bus_acquired (GDBusConnection *connection,
                           const gchar     *name,
                           gpointer         user_data)
{
  SfcdDbusWrapperInfo *info  = user_data;
  GError              *error = NULL;

  // TODO replace __get_client by something remotely related to DBus authentication

  info->interface = sfcd_dbus_wrapper__skeleton_new ();

  g_signal_connect (info->interface, "handle-new", G_CALLBACK (on_handle_new), _get_client ());
  g_signal_connect (info->interface, "handle-destroy", G_CALLBACK (on_handle_destroy), _get_client ());
  g_signal_connect (info->interface, "handle-get-state", G_CALLBACK (on_handle_get_state), _get_client ());
  g_signal_connect (info->interface, "handle-run", G_CALLBACK (on_handle_run), _get_client ());
  g_signal_connect (info->interface, "handle-present", G_CALLBACK (on_handle_present), _get_client ());
  g_signal_connect (info->interface, "handle-cancel-run", G_CALLBACK (on_handle_cancel_run), _get_client ());
  g_signal_connect (info->interface, "handle-set-action", G_CALLBACK (on_handle_set_action), _get_client ());
  g_signal_connect (info->interface, "handle-get-action", G_CALLBACK (on_handle_get_action), _get_client ());
  g_signal_connect (info->interface, "handle-set-local-only", G_CALLBACK (on_handle_set_local_only), _get_client ());
  g_signal_connect (info->interface, "handle-get-local-only", G_CALLBACK (on_handle_get_local_only), _get_client ());
  g_signal_connect (info->interface, "handle-set-select-multiple", G_CALLBACK (on_handle_set_select_multiple), _get_client ());
  g_signal_connect (info->interface, "handle-get-select-multiple", G_CALLBACK (on_handle_get_select_multiple), _get_client ());
  g_signal_connect (info->interface, "handle-set-show-hidden", G_CALLBACK (on_handle_set_show_hidden), _get_client ());
  g_signal_connect (info->interface, "handle-get-show-hidden", G_CALLBACK (on_handle_get_show_hidden), _get_client ());
  g_signal_connect (info->interface, "handle-set-current-name", G_CALLBACK (on_handle_set_current_name), _get_client ());
  g_signal_connect (info->interface, "handle-set-filename", G_CALLBACK (on_handle_set_filename), _get_client ());
  g_signal_connect (info->interface, "handle-set-current-folder", G_CALLBACK (on_handle_set_current_folder), _get_client ());
  g_signal_connect (info->interface, "handle-set-uri", G_CALLBACK (on_handle_set_uri), _get_client ());
  g_signal_connect (info->interface, "handle-set-current-folder-uri", G_CALLBACK (on_handle_set_current_folder_uri), _get_client ());
  g_signal_connect (info->interface, "handle-add-shortcut-folder", G_CALLBACK (on_handle_add_shortcut_folder), _get_client ());
  g_signal_connect (info->interface, "handle-remove-shortcut-folder", G_CALLBACK (on_handle_remove_shortcut_folder), _get_client ());
  g_signal_connect (info->interface, "handle-list-shortcut-folders", G_CALLBACK (on_handle_list_shortcut_folders), _get_client ());
  g_signal_connect (info->interface, "handle-add-shortcut-folder-uri", G_CALLBACK (on_handle_add_shortcut_folder_uri), _get_client ());
  g_signal_connect (info->interface, "handle-remove-shortcut-folder-uri", G_CALLBACK (on_handle_remove_shortcut_folder_uri), _get_client ());
  g_signal_connect (info->interface, "handle-list-shortcut-folder-uris", G_CALLBACK (on_handle_list_shortcut_folder_uris), _get_client ());
  g_signal_connect (info->interface, "handle-get-current-name", G_CALLBACK (on_handle_get_current_name), _get_client ());
  g_signal_connect (info->interface, "handle-get-filename", G_CALLBACK (on_handle_get_filename), _get_client ());
  g_signal_connect (info->interface, "handle-get-filenames", G_CALLBACK (on_handle_get_filenames), _get_client ());
  g_signal_connect (info->interface, "handle-get-current-folder", G_CALLBACK (on_handle_get_current_folder), _get_client ());
  g_signal_connect (info->interface, "handle-get-uri", G_CALLBACK (on_handle_get_uri), _get_client ());
  g_signal_connect (info->interface, "handle-get-uris", G_CALLBACK (on_handle_get_uris), _get_client ());
  g_signal_connect (info->interface, "handle-get-current-folder-uri", G_CALLBACK (on_handle_get_current_folder_uri), _get_client ());

  if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (info->interface),
                                         connection,
                                         SANDBOXUTILS_PATH,
                                         &error))
  {
    syslog (LOG_CRIT, "SfcdDbusWrapper.Dbus.OnBusAcquired: %s\n", g_error_get_message (error));
    g_error_free (error);
  }
}

static void
sfcd_dbus_on_name_acquired (GDBusConnection *connection,
                            const gchar     *name,
                            gpointer         user_data)
{
  //TODO syslog sfcd ready
}

static void
sfcd_dbus_on_name_lost (GDBusConnection *connection,
                        const gchar     *name,
                        gpointer         user_data)
{
//  syslog (LOG_INFO, "'%s' was lost, shutting down the SandboxFileChooserDialog interface.\n");

  //TODO finalise my sfcd dbus?
  //FIXME or should I rather reinitialise?

  //  exit (EXIT_FAILURE);
}


static SfcdDbusWrapperInfo *
sfcd_dbus_info_new ()
{
  SfcdDbusWrapperInfo *i = g_malloc (sizeof (SfcdDbusWrapperInfo));

  i->owner_id  = 0;
  i->interface = NULL;

  return i;
}

SfcdDbusWrapperInfo *
sfcd_dbus_wrapper_dbus_init ()
{
  SfcdDbusWrapperInfo *info = sfcd_dbus_info_new ();

  // Register errors to shut DBus's mouth
  // register_dbus_errors (); //TODO

  // Get this bus going
  info->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                   SFCD_IFACE,
                                   G_BUS_NAME_OWNER_FLAGS_NONE,
                                   sfcd_dbus_on_bus_acquired,
                                   sfcd_dbus_on_name_acquired,
                                   sfcd_dbus_on_name_lost,
                                   info,
                                   sfcd_dbus_wrapper_dbus_shutdown);

  g_assert (info->owner_id != 0);

  return info;
}

void
sfcd_dbus_wrapper_dbus_shutdown (gpointer data)
{
  SfcdDbusWrapperInfo *info = data;

  //TODO error checking?

  // Clean up server
  g_bus_unown_name (info->owner_id);
  //FIXME find how to do this with my new interface g_dbus_node_info_unref (info->introspection_data);

  g_free (info);
}

