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

#include <gtk/gtkx.h>
#include <syslog.h>
#include <sandboxutils.h>

#include "sandboxfilechooserdialogdbuswrapper.h"

static void on_handle_response_signal (SandboxFileChooserDialog *, gint, gint, gpointer);
static void on_handle_destroy_signal (SandboxFileChooserDialog *, gpointer);
       
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

  g_signal_handlers_disconnect_matched (sfcd, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_handle_destroy_signal, NULL);
  g_signal_handlers_disconnect_matched (sfcd, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, on_handle_response_signal, NULL);

  g_mutex_unlock (&cli->dialogsMutex);

  return sfcd;
}

/**
 * TODO doc
 * @invocation (allow-none): .... or %NULL as a convenience to signal handlers
 * who don't have an invocation available
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

static void
on_handle_response_signal (SandboxFileChooserDialog *sfcd,
                           gint                      response_id,
                           gint                      state,
                           gpointer                  user_data)
{
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  const gchar                *dialog_id  = sfcd_get_id (sfcd);

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_dbus_wrapper__emit_response (info->interface,
                                     dialog_id,
                                     response_id,
                                     state);
  }
  _sfcd_dbus_wrapper_lookup_finished (NULL, sfcd, dialog_id);

  return;
}

// This method is called only when the user destroys the dialog via the WM,
// which causes the local dialog being destroyed. For when the client app calls
// the destroy method, see on_handle_destroy.
static void
on_handle_destroy_signal (SandboxFileChooserDialog *sfcd,
                         gpointer                   user_data)
{
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  const gchar                *dialog_id  = sfcd_get_id (sfcd);

//FIXME  g_hash_table_remove (cli->dialogs, dialog_id);

  if ((sfcd = _sfcd_dbus_wrapper_lookup_and_remove (cli, dialog_id)) != NULL)
  {
    sfcd_dbus_wrapper__emit_destroy (info->interface, dialog_id);
    g_object_unref (sfcd);
  }
  _sfcd_dbus_wrapper_lookup_finished (NULL, sfcd, dialog_id);

  return;
}

static gboolean
on_handle_new (SfcdDbusWrapper        *interface,
               GDBusMethodInvocation  *invocation,
               const gchar            *title,
               const gchar            *parent_id,
               const gint              action,
               GVariant               *button_list,
               gpointer                user_data)
{
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  SandboxFileChooserDialog   *sfcd       = NULL;
  GtkWidget                  *dialog     = NULL;
  gchar                      *dialog_id  = NULL;
  GVariant                   *item       = NULL;
  GVariantIter               *iter       = NULL;
  GError                     *error      = NULL;

  // Create a new Local SandboxUtils dialog
  sfcd = lfcd_new_variant (title,
                           parent_id,
                           NULL,
                           action,
                           button_list);

  if (sfcd==NULL)
  {
	  g_set_error (&error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SFCD_ERROR_CREATION,
				  "SfcdDbusWrapper.Sfcd.New: could not allocate memory to create SandboxFileChooserDialog.\n");
		_sfcd_dbus_wrapper_return_error (invocation, error);

	  return TRUE;
  }

  // Connect to signals
  g_signal_connect (sfcd, "destroy", (GCallback) on_handle_destroy_signal, info);
  g_signal_connect (sfcd, "response", (GCallback) on_handle_response_signal, info);

  // Store dialog in the client's table and return its id
  gchar *key = g_strdup (sfcd_get_id (sfcd));

  g_mutex_lock (&cli->dialogsMutex);
  g_object_ref (sfcd);
  g_hash_table_insert (cli->dialogs, key, sfcd);
  g_mutex_unlock (&cli->dialogsMutex);

  sfcd_dbus_wrapper__complete_new (interface, invocation, key);

  return TRUE;
}

static gboolean
on_handle_get_state (SfcdDbusWrapper        *interface,
                     GDBusMethodInvocation  *invocation,
                     const gchar            *dialog_id,
                     gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    SfcdState state = sfcd_get_state (sfcd);
    sfcd_dbus_wrapper__complete_get_state (interface, invocation, state);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

// This method is called only when the client app calls the destroy method. We
// send the destroy signal ourselves to the client because we need to remove the
// dialog from the hash table (to prevent new methods being called on an object
// being destroyed). Our signal handler to the local dialog's "destroy" will be
// removed at the same time we remove the dialog from the hash table. For when
// the user destroys the dialog via the WM, see on_handle_destroy_signal.
static gboolean
on_handle_destroy (SfcdDbusWrapper        *interface,
                   GDBusMethodInvocation  *invocation,
                   const gchar            *dialog_id,
                   gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;

  if ((sfcd = _sfcd_dbus_wrapper_lookup_and_remove (cli, dialog_id)) != NULL)
  {
    sfcd_destroy (sfcd);
    sfcd_dbus_wrapper__complete_destroy (interface, invocation);
    sfcd_dbus_wrapper__emit_destroy (info->interface, dialog_id);
    g_object_unref (sfcd);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_run (SfcdDbusWrapper        *interface,
               GDBusMethodInvocation  *invocation,
               const gchar            *dialog_id,
               gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_run (sfcd, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_present (sfcd, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_cancel_run (sfcd, &error);

    if (!error)
      sfcd_dbus_wrapper__complete_cancel_run (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_set_extra_widget (SfcdDbusWrapper        *interface,
                            GDBusMethodInvocation  *invocation,
                            const gchar            *dialog_id,
                            const gulong            widget_id,
                            gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    GtkWidget *widget = NULL;

    // There is a plug id, create a socket to embed it
    if (widget_id != 0)
    {
      // Set the extra widget through GtkSocket/GtkPlug
      widget = gtk_socket_new ();
      sfcd_set_extra_widget (sfcd, widget, &error);
      
      //TODO connect to plug signals?

      // Set the remote parent's id to whatever was passed to us
      gtk_socket_add_id (GTK_SOCKET (widget), widget_id);
      gtk_widget_show_all (widget);
    }
    // Else just unset the widget once and for all
    else
    {
      sfcd_set_extra_widget (sfcd, NULL, &error);
    }

    if (!error)
      sfcd_dbus_wrapper__complete_set_extra_widget (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_extra_widget (SfcdDbusWrapper        *interface,
                            GDBusMethodInvocation  *invocation,
                            const gchar            *dialog_id,
                            gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    GtkWidget *widget = sfcd_get_extra_widget (sfcd, &error);
    gulong widget_id = 0;

    if (widget)
    {
      if (GTK_IS_SOCKET (widget))
      {
        GdkWindow *win = gtk_socket_get_plug_window (GTK_SOCKET (widget));
        if (win)
          widget_id = GDK_WINDOW_XID (win);
      }
      else
        syslog (LOG_DEBUG, "SfcdDbusWrapper.Sfcd.OnHandleGetExtraWidget: dialog '%s' does not contain a socket, cannot proceed.\n",
                dialog_id);
    }

    if (!error)
      sfcd_dbus_wrapper__complete_get_extra_widget (interface, invocation, widget_id);
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_set_action (sfcd, action, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gint action = sfcd_get_action (sfcd, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_set_local_only (sfcd, local_only, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gboolean local_only = sfcd_get_local_only (sfcd, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_set_select_multiple (sfcd, select_multiple, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gboolean select_multiple = sfcd_get_select_multiple (sfcd, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_set_show_hidden (sfcd, show_hidden, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gboolean show_hidden = sfcd_get_show_hidden (sfcd, &error);

    if (!error)
      sfcd_dbus_wrapper__complete_get_show_hidden (interface, invocation, show_hidden);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_set_do_overwrite_confirmation (SfcdDbusWrapper        *interface,
                                         GDBusMethodInvocation  *invocation,
                                         const gchar            *dialog_id,
                                         const gboolean          do_overwrite_confirmation,
                                         gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_set_do_overwrite_confirmation (sfcd, do_overwrite_confirmation, &error);

    if (!error)
      sfcd_dbus_wrapper__complete_set_do_overwrite_confirmation (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_do_overwrite_confirmation (SfcdDbusWrapper        *interface,
                                         GDBusMethodInvocation  *invocation,
                                         const gchar            *dialog_id,
                                         gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gboolean do_overwrite_confirmation = sfcd_get_do_overwrite_confirmation (sfcd, &error);

    if (!error)
      sfcd_dbus_wrapper__complete_get_do_overwrite_confirmation (interface, invocation, do_overwrite_confirmation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_set_create_folders (SfcdDbusWrapper        *interface,
                              GDBusMethodInvocation  *invocation,
                              const gchar            *dialog_id,
                              const gboolean          create_folders,
                              gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_set_create_folders (sfcd, create_folders, &error);

    if (!error)
      sfcd_dbus_wrapper__complete_set_create_folders (interface, invocation);
    else
      _sfcd_dbus_wrapper_return_error (invocation, error);
  }
  _sfcd_dbus_wrapper_lookup_finished (invocation, sfcd, dialog_id);

  return TRUE;
}

static gboolean
on_handle_get_create_folders (SfcdDbusWrapper        *interface,
                              GDBusMethodInvocation  *invocation,
                              const gchar            *dialog_id,
                              gpointer                user_data)
{
  SandboxFileChooserDialog   *sfcd       = NULL;
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gboolean create_folders = sfcd_get_create_folders (sfcd, &error);

    if (!error)
      sfcd_dbus_wrapper__complete_get_create_folders (interface, invocation, create_folders);
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_set_current_name (sfcd, current_name, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_set_filename (sfcd, filename, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_set_current_folder (sfcd, current_folder, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_set_uri (sfcd, uri, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_set_current_folder_uri (sfcd, current_folder_uri, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_add_shortcut_folder (sfcd, folder, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_remove_shortcut_folder (sfcd, folder, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    GSList *list = sfcd_list_shortcut_folders (sfcd, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_add_shortcut_folder_uri (sfcd, uri, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    sfcd_remove_shortcut_folder_uri (sfcd, uri, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    GSList *list = sfcd_list_shortcut_folder_uris (sfcd, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gchar *name = sfcd_get_current_name (sfcd, &error);
    
    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gchar *filename = sfcd_get_filename (sfcd, &error);
    
    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    GSList *list = sfcd_get_filenames (sfcd, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gchar *current_folder = sfcd_get_current_folder (sfcd, &error);
    
    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gchar *uri = sfcd_get_uri (sfcd, &error);
    
    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    GSList *list = sfcd_get_uris (sfcd, &error);

    if (!error)
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
  SfcdDbusWrapperInfo        *info       = user_data;
  SandboxUtilsClient         *cli        = info->client;
  GError                     *error      = NULL;

  if ((sfcd = _sfcd_dbus_wrapper_lookup (cli, dialog_id)) != NULL)
  {
    gchar *uri = sfcd_get_current_folder_uri (sfcd, &error);
    
    if (!error)
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

  g_signal_connect (info->interface, "handle-new", G_CALLBACK (on_handle_new), info);
  g_signal_connect (info->interface, "handle-destroy", G_CALLBACK (on_handle_destroy), info);
  g_signal_connect (info->interface, "handle-get-state", G_CALLBACK (on_handle_get_state), info);
  g_signal_connect (info->interface, "handle-run", G_CALLBACK (on_handle_run), info);
  g_signal_connect (info->interface, "handle-present", G_CALLBACK (on_handle_present), info);
  g_signal_connect (info->interface, "handle-cancel-run", G_CALLBACK (on_handle_cancel_run), info);
  g_signal_connect (info->interface, "handle-set-extra-widget", G_CALLBACK (on_handle_set_extra_widget), info);
  g_signal_connect (info->interface, "handle-get-extra-widget", G_CALLBACK (on_handle_get_extra_widget), info);
  g_signal_connect (info->interface, "handle-set-action", G_CALLBACK (on_handle_set_action), info);
  g_signal_connect (info->interface, "handle-get-action", G_CALLBACK (on_handle_get_action), info);
  g_signal_connect (info->interface, "handle-set-local-only", G_CALLBACK (on_handle_set_local_only), info);
  g_signal_connect (info->interface, "handle-get-local-only", G_CALLBACK (on_handle_get_local_only), info);
  g_signal_connect (info->interface, "handle-set-select-multiple", G_CALLBACK (on_handle_set_select_multiple), info);
  g_signal_connect (info->interface, "handle-get-select-multiple", G_CALLBACK (on_handle_get_select_multiple), info);
  g_signal_connect (info->interface, "handle-set-show-hidden", G_CALLBACK (on_handle_set_show_hidden), info);
  g_signal_connect (info->interface, "handle-get-show-hidden", G_CALLBACK (on_handle_get_show_hidden), info);
  g_signal_connect (info->interface, "handle-set-do-overwrite-confirmation", G_CALLBACK (on_handle_set_do_overwrite_confirmation), info);
  g_signal_connect (info->interface, "handle-get-do-overwrite-confirmation", G_CALLBACK (on_handle_get_do_overwrite_confirmation), info);
  g_signal_connect (info->interface, "handle-set-create-folders", G_CALLBACK (on_handle_set_create_folders), info);
  g_signal_connect (info->interface, "handle-get-create-folders", G_CALLBACK (on_handle_get_create_folders), info);
  g_signal_connect (info->interface, "handle-set-current-name", G_CALLBACK (on_handle_set_current_name), info);
  g_signal_connect (info->interface, "handle-set-filename", G_CALLBACK (on_handle_set_filename), info);
  g_signal_connect (info->interface, "handle-set-current-folder", G_CALLBACK (on_handle_set_current_folder), info);
  g_signal_connect (info->interface, "handle-set-uri", G_CALLBACK (on_handle_set_uri), info);
  g_signal_connect (info->interface, "handle-set-current-folder-uri", G_CALLBACK (on_handle_set_current_folder_uri), info);
  g_signal_connect (info->interface, "handle-add-shortcut-folder", G_CALLBACK (on_handle_add_shortcut_folder), info);
  g_signal_connect (info->interface, "handle-remove-shortcut-folder", G_CALLBACK (on_handle_remove_shortcut_folder), info);
  g_signal_connect (info->interface, "handle-list-shortcut-folders", G_CALLBACK (on_handle_list_shortcut_folders), info);
  g_signal_connect (info->interface, "handle-add-shortcut-folder-uri", G_CALLBACK (on_handle_add_shortcut_folder_uri), info);
  g_signal_connect (info->interface, "handle-remove-shortcut-folder-uri", G_CALLBACK (on_handle_remove_shortcut_folder_uri), info);
  g_signal_connect (info->interface, "handle-list-shortcut-folder-uris", G_CALLBACK (on_handle_list_shortcut_folder_uris), info);
  g_signal_connect (info->interface, "handle-get-current-name", G_CALLBACK (on_handle_get_current_name), info);
  g_signal_connect (info->interface, "handle-get-filename", G_CALLBACK (on_handle_get_filename), info);
  g_signal_connect (info->interface, "handle-get-filenames", G_CALLBACK (on_handle_get_filenames), info);
  g_signal_connect (info->interface, "handle-get-current-folder", G_CALLBACK (on_handle_get_current_folder), info);
  g_signal_connect (info->interface, "handle-get-uri", G_CALLBACK (on_handle_get_uri), info);
  g_signal_connect (info->interface, "handle-get-uris", G_CALLBACK (on_handle_get_uris), info);
  g_signal_connect (info->interface, "handle-get-current-folder-uri", G_CALLBACK (on_handle_get_current_folder_uri), info);

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
  i->client = NULL;

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
  
  info->client = _get_client ();

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
  
  //TODO notify client of interface shutdown
  info->client = NULL;

  g_free (info);
}

