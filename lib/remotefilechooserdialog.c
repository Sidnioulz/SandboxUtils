/*
 * remotefilechooserdialog.h: SandboxFileChooserDialog over GDBus
 *
 * Copyright (C) 2014 Steve Dodier-Lazaro <sidnioulz@gmail.com>
 * Copyright (C) 2003, Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Steve Dodier-Lazaro <sidnioulz@gmail.com>
 */

/**
 * SECTION:remotefilechooserdialog
 * @Title: RemoteFileChooserDialog
 * @Short_description: A remote implementation of #SandboxFileChooserDialog, this
 * is used to implement #SandboxFileChooserDialog for client applications.
 * @See_also: #SandboxFileChooserDialog
 *
 * #RemoteFileChooserDialog is a private class, that implements the
 * #SandboxFileChooserDialog API. This class redirects all of its methods over
 * GDBus to a remote server that spawns and processes actual GTK+ dialogs.
 * #RemoteFileChooserDialog can only be used if such a server is present, which
 * is normally the case only for sandboxed applications. 
 *
 * #RemoteFileChooserDialog exposes fewer constructors than #LocalFileChooserDialog
 * because it is only meant to be called by #SandboxFileChooserDialog's sfcd_new()
 * in cases where remote dialogs should be used (such as being jailed in a
 * sandbox with a server). 
 *
 * Since: 0.5
 **/

#include <glib.h>
#include <gtk/gtk.h>
#include <syslog.h>

#include "sandboxfilechooserdialogdbusobject.h"
#include "remotefilechooserdialog.h"
#include "sandboxutilsmarshals.h"
#include "sandboxutilscommon.h"

struct _RemoteFileChooserDialogPrivate
{
  GtkWidget             *local_bits;    /* fake container for local widgets */
  GtkWindow             *local_parent;  /* transient parent window (allow-none) */
  gchar                 *remote_id;     /* id of this instance */
  gchar                 *cached_title;  /* cached version of the dialog title */
};

G_DEFINE_TYPE_WITH_PRIVATE (RemoteFileChooserDialog, rfcd, SANDBOX_TYPE_FILE_CHOOSER_DIALOG)

static void                 rfcd_destroy                       (SandboxFileChooserDialog *);
static SfcdState            rfcd_get_state                     (SandboxFileChooserDialog *);
static const gchar *        rfcd_get_state_printable           (SandboxFileChooserDialog *);
static const gchar *        rfcd_get_dialog_title              (SandboxFileChooserDialog *);
static gboolean             rfcd_is_running                    (SandboxFileChooserDialog *);
static const gchar *        rfcd_get_id                        (SandboxFileChooserDialog *);
static void                 rfcd_run                           (SandboxFileChooserDialog *, GError **);
static void                 rfcd_present                       (SandboxFileChooserDialog *, GError **);
static void                 rfcd_cancel_run                    (SandboxFileChooserDialog *, GError **);
static void                 rfcd_set_action                    (SandboxFileChooserDialog *, GtkFileChooserAction, GError **);
static GtkFileChooserAction rfcd_get_action                    (SandboxFileChooserDialog *, GError **);
static void                 rfcd_set_local_only                (SandboxFileChooserDialog *, gboolean, GError **);
static gboolean             rfcd_get_local_only                (SandboxFileChooserDialog *, GError **);
static void                 rfcd_set_select_multiple           (SandboxFileChooserDialog *, gboolean, GError **);
static gboolean             rfcd_get_select_multiple           (SandboxFileChooserDialog *, GError **);
static void                 rfcd_set_show_hidden               (SandboxFileChooserDialog *, gboolean, GError **);
static gboolean             rfcd_get_show_hidden               (SandboxFileChooserDialog *, GError **);
static void                 rfcd_set_do_overwrite_confirmation (SandboxFileChooserDialog *, gboolean, GError **);
static gboolean             rfcd_get_do_overwrite_confirmation (SandboxFileChooserDialog *, GError **);
static void                 rfcd_set_create_folders            (SandboxFileChooserDialog *, gboolean, GError **);
static gboolean             rfcd_get_create_folders            (SandboxFileChooserDialog *, GError **);
static void                 rfcd_set_current_name              (SandboxFileChooserDialog *, const gchar *, GError **);
static void                 rfcd_set_filename                  (SandboxFileChooserDialog *, const gchar *, GError **);
static void                 rfcd_set_current_folder            (SandboxFileChooserDialog *, const gchar *, GError **);
static void                 rfcd_set_uri                       (SandboxFileChooserDialog *, const gchar *, GError **);
static void                 rfcd_set_current_folder_uri        (SandboxFileChooserDialog *, const gchar *, GError **);
static gboolean             rfcd_add_shortcut_folder           (SandboxFileChooserDialog *, const gchar *, GError **);
static gboolean             rfcd_remove_shortcut_folder        (SandboxFileChooserDialog *, const gchar *, GError **);
static GSList *             rfcd_list_shortcut_folders         (SandboxFileChooserDialog *, GError **);
static gboolean             rfcd_add_shortcut_folder_uri       (SandboxFileChooserDialog *, const gchar *, GError **);
static gboolean             rfcd_remove_shortcut_folder_uri    (SandboxFileChooserDialog *, const gchar *, GError **);
static GSList *             rfcd_list_shortcut_folder_uris     (SandboxFileChooserDialog *, GError **);
static gchar *              rfcd_get_current_name              (SandboxFileChooserDialog *, GError **);
static gchar *              rfcd_get_filename                  (SandboxFileChooserDialog *, GError **);
static GSList *             rfcd_get_filenames                 (SandboxFileChooserDialog *, GError **);
static gchar *              rfcd_get_current_folder            (SandboxFileChooserDialog *, GError **);
static gchar *              rfcd_get_uri                       (SandboxFileChooserDialog *, GError **);
static GSList *             rfcd_get_uris                      (SandboxFileChooserDialog *, GError **);
static gchar *              rfcd_get_current_folder_uri        (SandboxFileChooserDialog *, GError **);

static gboolean
_rfcd_class_proxy_init (RemoteFileChooserDialogClass *klass)
{
  g_return_val_if_fail (REMOTE_IS_FILE_CHOOSER_DIALOG_CLASS (klass), FALSE);

  GError *error = NULL;
  klass->proxy = (GDBusProxy *) sfcd_dbus_wrapper__proxy_new_for_bus_sync (
                                  G_BUS_TYPE_SESSION,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  SFCD_IFACE,
                                  SANDBOXUTILS_PATH,
                                  NULL,
                                  &error);
  if (error)
  {
    klass->proxy = NULL;

    syslog (LOG_ALERT, "SandboxFileChooserDialog._ClassProxyInit: could not create proxy (%s).",
            g_error_get_message (error));
    g_error_free (error);

    return FALSE;
  }

  return TRUE;
}

static void
_rfcd_class_proxy_clear (RemoteFileChooserDialogClass *klass)
{
  g_return_if_fail (REMOTE_IS_FILE_CHOOSER_DIALOG_CLASS (klass));

  //TODO diagnose issues on the proxy for debug
  //TODO free and close proxy as gracefully as possible

  klass->proxy = NULL;
}

static SfcdDbusWrapper *
_rfcd_get_proxy (RemoteFileChooserDialog *self)
{
  g_return_val_if_fail (REMOTE_IS_FILE_CHOOSER_DIALOG (self), NULL);
  RemoteFileChooserDialogClass *klass = REMOTE_FILE_CHOOSER_DIALOG_GET_CLASS (self);

  if (!klass->proxy)
  {
    if (!_rfcd_class_proxy_init (klass))
    {
      syslog (LOG_ALERT, "SandboxFileChooserDialog._GetProxy: failed to get proxy, and then failed to reinitialize it.");
      return NULL;
    }
  }

  return (SFCD_DBUS_WRAPPER_ (klass->proxy));
}

static SfcdDbusWrapper *
_rfcd_reset_proxy (RemoteFileChooserDialog *self)
{
  g_return_val_if_fail (REMOTE_IS_FILE_CHOOSER_DIALOG (self), NULL);
  RemoteFileChooserDialogClass *klass = REMOTE_FILE_CHOOSER_DIALOG_GET_CLASS (self);

  _rfcd_class_proxy_clear (klass);
  return _rfcd_get_proxy (self);
}


static void
rfcd_init (RemoteFileChooserDialog *self)
{
  self->priv = rfcd_get_instance_private (self);

  self->priv->local_bits    = NULL;
  self->priv->local_parent  = NULL;
  self->priv->remote_id     = NULL;
  self->priv->cached_title  = NULL;
}

static void
rfcd_dispose (GObject* object)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (object);

  if (self->priv->local_parent)
  {
    //TODO detach/unref?
  }

  if (self->priv->local_bits)
  {
    gtk_widget_destroy (self->priv->local_bits);
  }

  if (self->priv->cached_title)
    g_free (self->priv->cached_title);

  syslog (LOG_DEBUG, "SandboxFileChooserDialog.Dispose: dialog '%s' was disposed.\n",
              self->priv->remote_id);

  if (self->priv->remote_id)
    g_free (self->priv->remote_id);
}

static void
rfcd_finalize (GObject* object)
{
}

/**
 * rfcd_new_valist:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL
 * @action: Open or save mode for the dialog (see #GtkFileChooserAction)
 * @first_button_text: (allow-none): stock ID or text to go in the first button, or %NULL
 * @varargs: response ID for the first button, then additional (button, id) pairs, ending with %NULL
 *
 * Creates a new #RemoteFileChooserDialog. You usually should not use this
 * function. In a normal application, you almost always want to use sfcd_new().
 * 
 * The @parent parameter will not be made transient out of the box in a
 * #RemoteFileChooserDialog, because the actual GTK+ dialog is controlled by 
 * a remote process. As of 0.5.0, there is no
 * convention or standard on what this parameter should represent, and no
 * servers or compositors implementing it. It will be used in future releases.
 *
 * This is equivalent to gtk_file_chooser_dialog_new() in the GTK+ API.
 *
 * Return value: a new #SansboxFileChooserDialog
 *
 * See also: rfcd_new() to create a #RemoteFileChooserDialog, and sfcd_new() to 
 * create a #SandboxFileChooserDialog regardless of whether local or remote. 
 *
 * Since: 0.5
 **/
SandboxFileChooserDialog *
rfcd_new_valist (const gchar          *title,
                 GtkWindow            *parent,
                 GtkFileChooserAction  action,
                 const gchar          *first_button_text,
                 va_list               varargs)
{
  RemoteFileChooserDialog *rfcd = g_object_new (REMOTE_TYPE_FILE_CHOOSER_DIALOG, NULL);
  g_return_val_if_fail (rfcd != NULL, NULL);

  // Remember local parent's identity
  rfcd->priv->local_parent = parent;
  //TODO get from parent
  const gchar *parentWinId = "(null)";//NULL;

  // Build button list
  GVariantBuilder builder;
  g_variant_builder_init (&builder, (const GVariantType *) "a{sv}");

  const char *button_text = first_button_text;
  gint response_id;

  while (button_text)
  {
    response_id = va_arg (varargs, gint);
    g_variant_builder_add (&builder, "{sv}", button_text, g_variant_new_int32 (response_id));
    button_text = va_arg (varargs, const gchar *);
  }

  GVariant *button_list = g_variant_builder_end (&builder);
  g_variant_ref_sink (button_list);

  GError *error = NULL;
  sfcd_dbus_wrapper__call_new_sync (_rfcd_get_proxy (rfcd),
                                    title,
                                    parentWinId, //TODO
                                    action,
                                    button_list,
                                    &rfcd->priv->remote_id,
                                    NULL,
                                    &error);
  g_variant_unref (button_list);

  if (error)
  {
    g_object_unref (rfcd);
    rfcd = NULL;
    syslog (LOG_ALERT, "SandboxFileChooserDialog.New: error when creating dialog -- %s",
            g_error_get_message (error));
    g_error_free (error);
  }
  else
  {
    rfcd->priv->cached_title = g_strdup (title);

    // Local window, if we want to display a custom widget
    //FIXME should I make the parent transient for this?
    //TODO take ownership?
    rfcd->priv->local_bits  = gtk_window_new (GTK_WINDOW_POPUP);
    gtk_widget_show (rfcd->priv->local_bits);

    syslog (LOG_DEBUG, "SandboxFileChooserDialog.New: dialog '%s' ('%s') has just been created.\n",
            rfcd->priv->remote_id, title);
  }

  return SANDBOX_FILE_CHOOSER_DIALOG (rfcd);
}

/**
 * rfcd_new:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL
 * @action: Open or save mode for the dialog (see #GtkFileChooserAction)
 * @first_button_text: (allow-none): stock ID or text to go in the first button, or %NULL
 * @...: response ID for the first button, then additional (button, id) pairs, ending with %NULL
 *
 * Creates a new #RemoteFileChooserDialog. You usually should not use this
 * function. In a normal application, you almost always want to use sfcd_new().
 * 
 * The @parent parameter will not be made transient out of the box in a
 * #RemoteFileChooserDialog, because the actual GTK+ dialog is controlled by 
 * a remote process. As of 0.5.0, there is no
 * convention or standard on what this parameter should represent, and no
 * servers or compositors implementing it. It will be used in future releases.
 *
 * This is equivalent to gtk_file_chooser_dialog_new() in the GTK+ API.
 *
 * Return value: a new #SansboxFileChooserDialog
 *
 * See also: rfcd_new() to create a #RemoteFileChooserDialog, and sfcd_new() to 
 * create a #SandboxFileChooserDialog regardless of whether local or remote. 
 *
 * Since: 0.5
 **/
SandboxFileChooserDialog *
rfcd_new (const gchar          *title,
          GtkWindow            *parent,
          GtkFileChooserAction  action,
          const gchar          *first_button_text,
          ...)
{
  va_list varargs;
  va_start(varargs, first_button_text);
  SandboxFileChooserDialog *rfcd = rfcd_new_valist (title,
                                                    parent,
                                                    action,
                                                    first_button_text,
                                                    varargs);
  va_end(varargs);

  return rfcd;
}

static void
rfcd_destroy (SandboxFileChooserDialog *sfcd)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (REMOTE_IS_FILE_CHOOSER_DIALOG (self));

  GError *error = NULL;
  if (!sfcd_dbus_wrapper__call_destroy_sync (_rfcd_get_proxy (self),
                                             self->priv->remote_id,
                                             NULL,
                                             &error))
  {
    syslog (LOG_ALERT, "SandboxFileChooserDialog.Destroy: error when destroying dialog %s -- %s",
            sfcd_get_id (sfcd), g_error_get_message (error));
  }
  else
  {
    syslog (LOG_DEBUG, "SandboxFileChooserDialog.Destroy: dialog '%s' ('%s')'s reference count has been decreased by one.\n",
              sfcd_get_id (sfcd), sfcd_get_dialog_title (sfcd));
  }

  g_object_unref (self);
}

SfcdState
rfcd_get_state (SandboxFileChooserDialog *sfcd)
{
  SfcdState state = SFCD_WRONG_STATE;
  gint stateHolder = SFCD_WRONG_STATE;
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (REMOTE_IS_FILE_CHOOSER_DIALOG (self), state);

  GError *error = NULL;
  if (!sfcd_dbus_wrapper__call_get_state_sync (_rfcd_get_proxy (self),
                                             self->priv->remote_id,
                                             &stateHolder,
                                             NULL,
                                             &error))
  {
    syslog (LOG_ALERT, "SandboxFileChooserDialog.Destroy: error when destroying dialog %s -- %s",
            sfcd_get_id (sfcd), g_error_get_message (error));
  }
  else
  {
    syslog (LOG_DEBUG, "SandboxFileChooserDialog.Destroy: dialog '%s' ('%s')'s reference count has been decreased by one.\n",
              sfcd_get_id (sfcd), sfcd_get_dialog_title (sfcd));
  }

  g_object_unref (self);

  // In lack of a better option...
  state = max (SFCD_WRONG_STATE, min (SFCD_LAST_STATE, stateHolder));

  return state;
}

const gchar *
rfcd_get_state_printable  (SandboxFileChooserDialog *sfcd)
{
  return SfcdStatePrintable [sfcd_get_state (sfcd)];
}

static const gchar *
rfcd_get_dialog_title (SandboxFileChooserDialog *sfcd)
{
  gchar *title = NULL;

  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (REMOTE_IS_FILE_CHOOSER_DIALOG (self), title);

  /* TODO not yet implemented
  GError *error = NULL;
  if (!sfcd_dbus_wrapper__call_get_dialog_title_sync (_rfcd_get_proxy (self),
                                             self->priv->remote_id,
                                             &title,
                                             NULL,
                                             &error))
  {
    syslog (LOG_ALERT, "SandboxFileChooserDialog.GetDialogTitle: error when querying dialog %s -- %s",
            sfcd_get_id (sfcd), g_error_get_message (error));
  }
  else
  {
    syslog (LOG_DEBUG, "SandboxFileChooserDialog.GetDialogTitle: dialog '%s' is titled '%s'.\n",
              sfcd_get_id (sfcd), title);
  }
  */

  return title;
}

gboolean
rfcd_is_running (SandboxFileChooserDialog *sfcd)
{
  return sfcd_get_state (sfcd) == SFCD_RUNNING;
}

const gchar *
rfcd_get_id (SandboxFileChooserDialog *sfcd)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (REMOTE_IS_FILE_CHOOSER_DIALOG (self), NULL);

  return self->priv->remote_id;
}

static gboolean
_rfcd_entry_sanity_check (RemoteFileChooserDialog    *self,
                          GError                  **error)
{
  // At the very least this is needed for error reporting
  g_return_val_if_fail (error != NULL, FALSE);

  // If the callee forgot to clean their error before calling us...
  if (*error != NULL)
  {
    g_prefix_error (error, 
                    "%s",
                    "SandboxFileChooserDialog._SanityCheck failed because an error "
                    "was already set (this should never happen, please report a bug).\n");

    return FALSE;
  }

  // If the callee gave us a non-valid RemoteFileChooserDialog
  if (!REMOTE_IS_FILE_CHOOSER_DIALOG (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_UNKNOWN,
                 "SandboxFileChooserDialog._SanityCheck: the object at '%p' is not a SandboxFileChooserDialog.\n",
                 self);

    return FALSE;
  }

  return TRUE;
}


/* RUNNING METHODS */
//TODO handlers for GDBus signals

static void
rfcd_run (SandboxFileChooserDialog *sfcd,
          GError                  **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));

}

void
rfcd_present (SandboxFileChooserDialog  *sfcd,
              GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));

}

static void
rfcd_cancel_run (SandboxFileChooserDialog  *sfcd,
                 GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));

}

static void
rfcd_set_action (SandboxFileChooserDialog  *sfcd,
                 GtkFileChooserAction       action,
                 GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));

}

static GtkFileChooserAction
rfcd_get_action (SandboxFileChooserDialog *sfcd,
                 GError                  **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  GtkFileChooserAction result = GTK_FILE_CHOOSER_ACTION_OPEN;

  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), result);


  return result;
}

static void
rfcd_set_local_only (SandboxFileChooserDialog  *sfcd,
                     gboolean                   local_only,
                     GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));

}

static gboolean
rfcd_get_local_only (SandboxFileChooserDialog  *sfcd,
                     GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  gboolean result = FALSE;

  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), result);


  return result;
}

static void
rfcd_set_select_multiple (SandboxFileChooserDialog *sfcd,
                          gboolean                   select_multiple,
                          GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));

}

static gboolean
rfcd_get_select_multiple (SandboxFileChooserDialog  *sfcd,
                          GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  gboolean result = FALSE;

  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), result);


  return result;
}

static void
rfcd_set_show_hidden (SandboxFileChooserDialog  *sfcd,
                      gboolean                   show_hidden,
                      GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));

}

static gboolean
rfcd_get_show_hidden (SandboxFileChooserDialog *sfcd,
                      GError                  **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  gboolean result = FALSE;

  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), result);


  return result;
}

static void
rfcd_set_do_overwrite_confirmation (SandboxFileChooserDialog  *sfcd,
                                    gboolean                   do_overwrite_confirmation,
                                    GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));

}

static gboolean
rfcd_get_do_overwrite_confirmation (SandboxFileChooserDialog  *sfcd,
                                    GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  gboolean result = FALSE;

  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), result);


  return result;
}

static void
rfcd_set_create_folders (SandboxFileChooserDialog  *sfcd,
                         gboolean                   create_folders,
                         GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));

}

static gboolean
rfcd_get_create_folders (SandboxFileChooserDialog  *sfcd,
                         GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  gboolean result = FALSE;

  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), result);


  return result;
}

static void
rfcd_set_current_name (SandboxFileChooserDialog  *sfcd,
                       const gchar               *name,
                       GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));

}

static void
rfcd_set_filename (SandboxFileChooserDialog  *sfcd,
                   const gchar               *filename,
                   GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));

}

static void
rfcd_set_current_folder (SandboxFileChooserDialog  *sfcd,
                         const gchar               *filename,
                         GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));

}

static void
rfcd_set_uri (SandboxFileChooserDialog  *sfcd,
              const gchar               *uri,
              GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));

}

static void
rfcd_set_current_folder_uri (SandboxFileChooserDialog  *sfcd,
                             const gchar               *uri,
                             GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_rfcd_entry_sanity_check (self, error));
}

static gboolean
rfcd_add_shortcut_folder (SandboxFileChooserDialog  *sfcd,
                          const gchar               *folder,
                          GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), FALSE);

  gboolean succeeded = FALSE;


  return succeeded;
}

static gboolean
rfcd_remove_shortcut_folder (SandboxFileChooserDialog  *sfcd,
                             const gchar               *folder,
                             GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), FALSE);

  gboolean succeeded = FALSE;


  return succeeded;
}

static GSList *
rfcd_list_shortcut_folders (SandboxFileChooserDialog *sfcd,
                            GError                    **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), NULL);

  GSList *list = NULL;

  return list;
}

static gboolean
rfcd_add_shortcut_folder_uri (SandboxFileChooserDialog  *sfcd,
                              const gchar               *uri,
                              GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), FALSE);

  gboolean succeeded = FALSE;


  return succeeded;
}

static gboolean
rfcd_remove_shortcut_folder_uri (SandboxFileChooserDialog *sfcd,
                                 const gchar               *uri,
                                 GError                   **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), FALSE);

  gboolean succeeded = FALSE;


  return succeeded;
}

static GSList *
rfcd_list_shortcut_folder_uris (SandboxFileChooserDialog *sfcd,
                                GError                    **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), NULL);

  GSList *list = NULL;

  return list;
}

static gchar *
rfcd_get_current_name (SandboxFileChooserDialog *sfcd,
                       GError                    **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), NULL);

  gchar *name = NULL;


  return name;
}

static gchar *
rfcd_get_filename (SandboxFileChooserDialog *sfcd,
                   GError                    **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), NULL);

  gchar *name = NULL;


  return name;
}

static GSList *
rfcd_get_filenames (SandboxFileChooserDialog *sfcd,
                    GError                    **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), NULL);

  GSList *list = NULL;

  return list;
}

static gchar *
rfcd_get_current_folder (SandboxFileChooserDialog *sfcd,
                         GError                    **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), NULL);

  gchar *folder = NULL;

  return folder;
}

static gchar *
rfcd_get_uri (SandboxFileChooserDialog *sfcd,
              GError                    **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), NULL);

  gchar *uri = NULL;

  return uri;
}

static GSList *
rfcd_get_uris (SandboxFileChooserDialog *sfcd,
               GError                    **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), NULL);

  GSList *list = NULL;

  return list;
}

static gchar *
rfcd_get_current_folder_uri (SandboxFileChooserDialog *sfcd,
                             GError                    **error)
{
  RemoteFileChooserDialog *self = REMOTE_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_rfcd_entry_sanity_check (self, error), NULL);

  gchar *uri = NULL;

  return uri;
}

static void
rfcd_class_init (RemoteFileChooserDialogClass *klass)
{
  SandboxFileChooserDialogClass *sfcd_class = SANDBOX_FILE_CHOOSER_DIALOG_CLASS (klass);

  //FIXME maybe reintroduce signals here?

  /* Hook finalization functions */
  sfcd_class->dispose = rfcd_dispose; /* instance destructor, reverse of init */
  sfcd_class->finalize = rfcd_finalize; /* class finalization, reverse of class init */

  /* Hook SandboxFileChooserDialog API functions */
  sfcd_class->get_state = rfcd_get_state;
  sfcd_class->get_state_printable = rfcd_get_state_printable;
  sfcd_class->is_running = rfcd_is_running;
  sfcd_class->destroy = rfcd_destroy;
  sfcd_class->get_dialog_title = rfcd_get_dialog_title;
  sfcd_class->get_id = rfcd_get_id;
  sfcd_class->run = rfcd_run;
  sfcd_class->present = rfcd_present;
  sfcd_class->cancel_run = rfcd_cancel_run;
  sfcd_class->set_action = rfcd_set_action;
  sfcd_class->get_action = rfcd_get_action;
  sfcd_class->set_local_only = rfcd_set_local_only;
  sfcd_class->get_local_only = rfcd_get_local_only;
  sfcd_class->set_select_multiple  = rfcd_set_select_multiple ;
  sfcd_class->get_select_multiple  = rfcd_get_select_multiple ;
  sfcd_class->set_show_hidden = rfcd_set_show_hidden;
  sfcd_class->get_show_hidden = rfcd_get_show_hidden;
  sfcd_class->set_do_overwrite_confirmation = rfcd_set_do_overwrite_confirmation;
  sfcd_class->get_do_overwrite_confirmation = rfcd_get_do_overwrite_confirmation;
  sfcd_class->set_create_folders = rfcd_set_create_folders;
  sfcd_class->get_create_folders = rfcd_get_create_folders;
  sfcd_class->set_current_name = rfcd_set_current_name;
  sfcd_class->set_filename = rfcd_set_filename;
  sfcd_class->set_current_folder = rfcd_set_current_folder;
  sfcd_class->set_uri = rfcd_set_uri;
  sfcd_class->set_current_folder_uri = rfcd_set_current_folder_uri;
  sfcd_class->add_shortcut_folder = rfcd_add_shortcut_folder;
  sfcd_class->remove_shortcut_folder = rfcd_remove_shortcut_folder;
  sfcd_class->list_shortcut_folders = rfcd_list_shortcut_folders;
  sfcd_class->add_shortcut_folder_uri = rfcd_add_shortcut_folder_uri;
  sfcd_class->remove_shortcut_folder_uri = rfcd_remove_shortcut_folder_uri;
  sfcd_class->list_shortcut_folder_uris = rfcd_list_shortcut_folder_uris;
  sfcd_class->get_current_name = rfcd_get_current_name;
  sfcd_class->get_filename = rfcd_get_filename;
  sfcd_class->get_filenames = rfcd_get_filenames;
  sfcd_class->get_current_folder = rfcd_get_current_folder;
  sfcd_class->get_uri = rfcd_get_uri;
  sfcd_class->get_uris = rfcd_get_uris;
  sfcd_class->get_current_folder_uri = rfcd_get_current_folder_uri;

  klass->proxy = NULL;
  _rfcd_class_proxy_init (klass);
}
