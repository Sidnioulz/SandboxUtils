/*
 * localfilechooserdialog.c: locally-running SandboxFileChooserDialog
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
 * SECTION:localfilechooserdialog
 * @Title: LocalFileChooserDialog
 * @Short_description: A local implementation of #SandboxFileChooserDialog, this
 * is used by #sandboxutilsd to provide an actual GTK+ dialog through D-Bus.
 * @See_also: #SandboxFileChooserDialog
 *
 * #LocalFileChooserDialog is a private class, that implements the
 * #SandboxFileChooserDialog API. This class exposes multiple constructors,
 * which are convenience functions for people to implement servers or wrappers 
 * (where one may store the button list of the dialog in a GVariant or va_list).
 *
 * In a normal application, you almost always want to use sfcd_new() which will
 * detect whether to use local GTK+ dialogs or remote dialogs over D-Bus. Users
 * and application launchers can pass a parameter to your application which will
 * cause sandboxutils_init() to detect what backend to use.
 *
 * Bear in mind local dialogs do not pass through the sandboxing container that
 * your application may be contained in, so using #LocalFileChooserDialog 
 * directly may not be a good idea.
 *
 * Since: 0.5
 **/

#include <glib.h>
#include <gtk/gtk.h>
#include <syslog.h>

#include "localfilechooserdialog.h"
#include "sandboxutilsmarshals.h"

struct _LocalFileChooserDialogPrivate
{
  GtkWidget             *dialog;        /* pointer to the #GtkFileChooserDialog */
  SfcdState              state;         /* state of the instance */
  GMutex                 stateMutex;    /* a mutex to provide thread-safety */
  gchar                 *remote_parent; /* id of a remote parent's window */
  gchar                 *id;            /* id of this instace */
};

G_DEFINE_TYPE_WITH_PRIVATE (LocalFileChooserDialog, lfcd, SANDBOX_TYPE_FILE_CHOOSER_DIALOG)

static guint64 __lfcd_instance_counter = 0;

static void                 lfcd_destroy                       (SandboxFileChooserDialog *);
static SfcdState            lfcd_get_state                     (SandboxFileChooserDialog *);
static const gchar *        lfcd_get_state_printable           (SandboxFileChooserDialog *);
static const gchar *        lfcd_get_dialog_title              (SandboxFileChooserDialog *);
static gboolean             lfcd_is_running                    (SandboxFileChooserDialog *);
static const gchar *        lfcd_get_id                        (SandboxFileChooserDialog *);
static void                 lfcd_run                           (SandboxFileChooserDialog *, GError **);
static void                 lfcd_present                       (SandboxFileChooserDialog *, GError **);
static void                 lfcd_cancel_run                    (SandboxFileChooserDialog *, GError **);
static void                 lfcd_set_action                    (SandboxFileChooserDialog *, GtkFileChooserAction, GError **);
static GtkFileChooserAction lfcd_get_action                    (SandboxFileChooserDialog *, GError **);
static void                 lfcd_set_local_only                (SandboxFileChooserDialog *, gboolean, GError **);
static gboolean             lfcd_get_local_only                (SandboxFileChooserDialog *, GError **);
static void                 lfcd_set_select_multiple           (SandboxFileChooserDialog *, gboolean, GError **);
static gboolean             lfcd_get_select_multiple           (SandboxFileChooserDialog *, GError **);
static void                 lfcd_set_show_hidden               (SandboxFileChooserDialog *, gboolean, GError **);
static gboolean             lfcd_get_show_hidden               (SandboxFileChooserDialog *, GError **);
static void                 lfcd_set_do_overwrite_confirmation (SandboxFileChooserDialog *, gboolean, GError **);
static gboolean             lfcd_get_do_overwrite_confirmation (SandboxFileChooserDialog *, GError **);
static void                 lfcd_set_create_folders            (SandboxFileChooserDialog *, gboolean, GError **);
static gboolean             lfcd_get_create_folders            (SandboxFileChooserDialog *, GError **);
static void                 lfcd_set_current_name              (SandboxFileChooserDialog *, const gchar *, GError **);
static void                 lfcd_set_filename                  (SandboxFileChooserDialog *, const gchar *, GError **);
static void                 lfcd_set_current_folder            (SandboxFileChooserDialog *, const gchar *, GError **);
static void                 lfcd_set_uri                       (SandboxFileChooserDialog *, const gchar *, GError **);
static void                 lfcd_set_current_folder_uri        (SandboxFileChooserDialog *, const gchar *, GError **);
static gboolean             lfcd_add_shortcut_folder           (SandboxFileChooserDialog *, const gchar *, GError **);
static gboolean             lfcd_remove_shortcut_folder        (SandboxFileChooserDialog *, const gchar *, GError **);
static GSList *             lfcd_list_shortcut_folders         (SandboxFileChooserDialog *, GError **);
static gboolean             lfcd_add_shortcut_folder_uri       (SandboxFileChooserDialog *, const gchar *, GError **);
static gboolean             lfcd_remove_shortcut_folder_uri    (SandboxFileChooserDialog *, const gchar *, GError **);
static GSList *             lfcd_list_shortcut_folder_uris     (SandboxFileChooserDialog *, GError **);
static gchar *              lfcd_get_current_name              (SandboxFileChooserDialog *, GError **);
static gchar *              lfcd_get_filename                  (SandboxFileChooserDialog *, GError **);
static GSList *             lfcd_get_filenames                 (SandboxFileChooserDialog *, GError **);
static gchar *              lfcd_get_current_folder            (SandboxFileChooserDialog *, GError **);
static gchar *              lfcd_get_uri                       (SandboxFileChooserDialog *, GError **);
static GSList *             lfcd_get_uris                      (SandboxFileChooserDialog *, GError **);
static gchar *              lfcd_get_current_folder_uri        (SandboxFileChooserDialog *, GError **);

static void
lfcd_init (LocalFileChooserDialog *self)
{
  self->priv = lfcd_get_instance_private (self);

  self->priv->dialog        = NULL;
  self->priv->state         = SFCD_CONFIGURATION;
  self->priv->remote_parent = NULL;

  self->priv->id            = g_strdup_printf ("%lu", __lfcd_instance_counter++);

  g_mutex_init (&self->priv->stateMutex);
}

static void
lfcd_dispose (GObject* object)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (object);
  SandboxFileChooserDialog *sfcd = SANDBOX_FILE_CHOOSER_DIALOG (self);
  SandboxFileChooserDialogClass *klass = SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (sfcd);

  // Clean up signal handlers and loop
  g_signal_handlers_disconnect_matched (self, G_SIGNAL_MATCH_ID, klass->close_signal,
                                        0, NULL, NULL, NULL);
  g_signal_handlers_disconnect_matched (self, G_SIGNAL_MATCH_ID, klass->destroy_signal,
                                        0, NULL, NULL, NULL);
  g_signal_handlers_disconnect_matched (self, G_SIGNAL_MATCH_ID, klass->hide_signal,
                                        0, NULL, NULL, NULL);
  g_signal_handlers_disconnect_matched (self, G_SIGNAL_MATCH_ID, klass->response_signal,
                                        0, NULL, NULL, NULL);
  g_signal_handlers_disconnect_matched (self, G_SIGNAL_MATCH_ID, klass->show_signal,
                                        0, NULL, NULL, NULL);

  g_mutex_clear (&self->priv->stateMutex);

  if (self->priv->dialog)
  {
    // Remove our own ref and then destroy the dialog
    g_object_unref (self->priv->dialog);
    gtk_widget_destroy (self->priv->dialog);
  }

  syslog (LOG_DEBUG, "SandboxFileChooserDialog.Dispose: dialog '%s' was disposed.\n",
              self->priv->id);

  g_free (self->priv->id);
}

static void
lfcd_finalize (GObject* object)
{
}

static void
_lfcd_on_hide (SandboxFileChooserDialog *sfcd,
                gpointer ignore)
{
  SandboxFileChooserDialogClass *klass = SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (sfcd);
  g_signal_emit (sfcd,
                 klass->hide_signal,
                 0);
}

static void
_lfcd_on_show (SandboxFileChooserDialog *sfcd,
                gpointer ignore)
{
  SandboxFileChooserDialogClass *klass = SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (sfcd);
  g_signal_emit (sfcd,
                 klass->show_signal,
                 0);
}

static gboolean
_lfcd_is_stock_accept_response_id (int response_id)
{
  return (response_id == GTK_RESPONSE_ACCEPT
	  || response_id == GTK_RESPONSE_OK
	  || response_id == GTK_RESPONSE_YES
	  || response_id == GTK_RESPONSE_APPLY);
}

/**
 * lfcd_new_valist:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parentWinId: (allow-none): Window Identifier of a remote transient parent, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL
 * @action: Open or save mode for the dialog (see #GtkFileChooserAction)
 * @first_button_text: (allow-none): stock ID or text to go in the first button, or %NULL
 * @varargs: response ID for the first button, then additional (button, id) pairs, ending with %NULL
 *
 * Creates a new #LocalFileChooserDialog. You usually should not use this
 * function, unless implementing a server or a wrapper for #LocalFileChooserDialog.
 * In a normal application, you almost always want to use sfcd_new().
 * 
 * @parentWinId refers to a unique window identifier that a privileged server may
 * pass onto the compositor of your session, so that the compositor treats the
 * #LocalFileChooserDialog created by this function as a transient child of the
 * window identified by @parentWinId. Bear in mind that the compositor needs to
 * recognise your application as a legitimate privileged server for it to attach
 * this dialog to other processes' windows. If you specify both @parent and
 * @parentWinId at the same time, this function will return NULL. 
 *
 * This is equivalent to gtk_file_chooser_dialog_new() in the GTK+ API.
 *
 * Return value: a new #SansboxFileChooserDialog
 *
 * See also: lfcd_new() and lfcd_new_with_remote_parent() to create a
 * #LocalFileChooserDialog, and sfcd_new() to create a #SandboxFileChooserDialog
 * regardless of whether local or remote. 
 *
 * Since: 0.5
 **/
SandboxFileChooserDialog *
lfcd_new_valist (const gchar          *title,
                 const gchar          *parentWinId,
                 GtkWindow            *parent,
                 GtkFileChooserAction  action,
                 const gchar          *first_button_text,
                 va_list               varargs)
{
  g_return_val_if_fail (parent == NULL || parentWinId == NULL, NULL);

  LocalFileChooserDialog *lfcd = g_object_new (LOCAL_TYPE_FILE_CHOOSER_DIALOG, NULL);
  g_return_val_if_fail (lfcd != NULL, NULL);

  lfcd->priv->dialog = gtk_file_chooser_dialog_new (title,
                                                    parent,
                                                    action,
                                                    NULL, NULL);

  const char *button_text = first_button_text;
  gint response_id;

  while (button_text)
  {
    response_id = va_arg (varargs, gint);
    if (!_lfcd_is_stock_accept_response_id (response_id) || sfcd_is_accept_label (button_text))
      gtk_dialog_add_button (GTK_DIALOG (lfcd->priv->dialog), button_text, response_id);
    else
      syslog (LOG_CRIT, "SandboxFileChooserDialog.New: dialog '%s' will not contain button '%s':'%d' for security reasons (acceptance state with label not known to convey acceptance meaning). If you think this is a bug, please report it indicating the application used and your current locale settings.",
              lfcd->priv->id, button_text, response_id);
    button_text = va_arg (varargs, const gchar *);
  }

  g_object_ref_sink (lfcd->priv->dialog);

  // Set the remote parent's id to whatever was passed to us
  if (parentWinId)
    lfcd->priv->remote_parent = g_strdup (parentWinId);

  // Connecting signals here - we do not connect to signals like close or destroy
  // that occur only when we are already cleaning up the dialog. Instead we emit
  // them ourselves.
  SandboxFileChooserDialog *sfcd = SANDBOX_FILE_CHOOSER_DIALOG (lfcd);
  g_signal_connect_swapped (lfcd->priv->dialog, "hide", (GCallback) _lfcd_on_hide, sfcd);
  g_signal_connect_swapped (lfcd->priv->dialog, "show", (GCallback) _lfcd_on_show, sfcd);
  
  //TODO manage close

  syslog (LOG_DEBUG, "SandboxFileChooserDialog.New: dialog '%s' ('%s') has just been created.\n",
            lfcd->priv->id, title);

  return SANDBOX_FILE_CHOOSER_DIALOG (lfcd);
}

/**
 * lfcd_new_variant:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parentWinId: (allow-none): Window Identifier of a remote transient parent, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL
 * @action: Open or save mode for the dialog (see #GtkFileChooserAction)
 * @button_list: an array of (gchar *button, #GtkResponseType id) pairs stored in a GVariant
 *
 * Creates a new #LocalFileChooserDialog. You usually should not use this
 * function, unless implementing a server or a wrapper for #LocalFileChooserDialog.
 * In a normal application, you almost always want to use sfcd_new().
 * 
 * @parentWinId refers to a unique window identifier that a privileged server may
 * pass onto the compositor of your session, so that the compositor treats the
 * #LocalFileChooserDialog created by this function as a transient child of the
 * window identified by @parentWinId. Bear in mind that the compositor needs to
 * recognise your application as a legitimate privileged server for it to attach
 * this dialog to other processes' windows. If you specify both @parent and
 * @parentWinId at the same time, this function will return NULL. 
 *
 * This is equivalent to gtk_file_chooser_dialog_new() in the GTK+ API.
 *
 * Return value: a new #SansboxFileChooserDialog
 *
 * See also: lfcd_new() and lfcd_new_with_remote_parent() to create a
 * #LocalFileChooserDialog, and sfcd_new() to create a #SandboxFileChooserDialog
 * regardless of whether local or remote. 
 *
 * Since: 0.5
 **/
SandboxFileChooserDialog *
lfcd_new_variant (const gchar          *title,
                  const gchar          *parentWinId,
                  GtkWindow            *parent,
                  GtkFileChooserAction  action,
                  GVariant             *button_list)
{
  g_return_val_if_fail (parent == NULL || parentWinId == NULL, NULL);

  LocalFileChooserDialog *lfcd =
        LOCAL_FILE_CHOOSER_DIALOG (lfcd_new_valist (title,
                                                    parentWinId,
                                                    parent,
                                                    action,
                                                    NULL,
                                                    NULL));

	// Populate dialog with buttons
  GVariant               *item = NULL;
  GVariantIter           *iter = NULL;

  g_variant_get (button_list, "a{sv}", &iter);
	while ((item = g_variant_iter_next_value (iter)))
  {
    const gchar *key;
    GVariant *value;

    g_variant_get (item, "{sv}", &key, &value);
    if (!_lfcd_is_stock_accept_response_id (g_variant_get_int32 (value)) || sfcd_is_accept_label (key))
      gtk_dialog_add_button (GTK_DIALOG (lfcd->priv->dialog), key, g_variant_get_int32 (value));
    else
      syslog (LOG_CRIT, "SandboxFileChooserDialog.New: dialog '%s' will not contain button '%s':'%d' for security reasons (acceptance state with label not known to convey acceptance meaning). If you think this is a bug, please report it indicating the application used and your current locale settings.",
              lfcd->priv->id, key, g_variant_get_int32 (value));
  }

  return SANDBOX_FILE_CHOOSER_DIALOG (lfcd);
}

/**
 * lfcd_new:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL
 * @action: Open or save mode for the dialog (see #GtkFileChooserAction)
 * @first_button_text: (allow-none): stock ID or text to go in the first button, or %NULL
 * @...: response ID for the first button, then additional (button, id) pairs, ending with %NULL
 *
 * Creates a new #LocalFileChooserDialog. You usually do not need to use this
 * function. See sfcd_new() instead.
 * 
 * This is equivalent to gtk_file_chooser_dialog_new() in the GTK+ API.
 *
 * Return value: a new #SansboxFileChooserDialog
 *
 * See also: lfcd_new_with_remote_parent() to create a #LocalFileChooserDialog
 * with a remote parent, and sfcd_new() to create a #SandboxFileChooserDialog
 * regardless of whether local or remote.
 *
 * Since: 0.5
 **/
SandboxFileChooserDialog *
lfcd_new (const gchar *title,
          GtkWindow *parent,
          GtkFileChooserAction action,
          const gchar *first_button_text,
          ...)
{
  va_list varargs;
  va_start(varargs, first_button_text);
  SandboxFileChooserDialog *lfcd = lfcd_new_valist (title,
                                                    NULL,
                                                    parent,
                                                    action,
                                                    first_button_text,
                                                    varargs);
  va_end(varargs);

  return lfcd;
}

/**
 * lfcd_new_with_remote_parent:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parentWinId: (allow-none): Window Identifier of a remote transient parent, or %NULL
 * @action: Open or save mode for the dialog (see #GtkFileChooserAction)
 * @first_button_text: (allow-none): stock ID or text to go in the first button, or %NULL
 * @...: response ID for the first button, then additional (button, id) pairs, ending with %NULL
 *
 * Creates a new #LocalFileChooserDialog, whose transient parent is owned by a
 * remote process.  You usually should not use this function, unless implementing
 * a server. In a normal application, you almost always want to use sfcd_new().
 * 
 * @parentWinId refers to a unique window identifier that a privileged server may
 * pass onto the compositor of your session, so that the compositor treats the
 * #LocalFileChooserDialog created by this function as a transient child of the
 * window identified by @parentWinId. Bear in mind that the compositor needs to
 * recognise your application as a legitimate privileged server for it to attach
 * this dialog to other processes' windows. 
 *
 * This is equivalent to gtk_file_chooser_dialog_new() in the GTK+ API.
 *
 * Return value: a new #SansboxFileChooserDialog
 *
 * See also: lfcd_new() to create a #LocalFileChooserDialog with a local parent,
 * and sfcd_new() to create a #SandboxFileChooserDialog regardless of whether
 * local or remote.
 *
 * Since: 0.5
 **/
SandboxFileChooserDialog *
lfcd_new_with_remote_parent (const gchar *title,
                             const gchar *parentWinId,
                             GtkFileChooserAction action,
                             const gchar *first_button_text,
                             ...)
{
  va_list varargs;
  va_start(varargs, first_button_text);
  SandboxFileChooserDialog *lfcd = lfcd_new_valist (title,
                                                    parentWinId,
                                                    NULL,
                                                    action,
                                                    first_button_text,
                                                    varargs);
  va_end(varargs);

  return lfcd;
}

static void
lfcd_destroy (SandboxFileChooserDialog *sfcd)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (LOCAL_IS_FILE_CHOOSER_DIALOG (self));

  syslog (LOG_DEBUG, "SandboxFileChooserDialog.Destroy: dialog '%s' ('%s')'s reference count has been decreased by one.\n",
              sfcd_get_id (sfcd), sfcd_get_dialog_title (sfcd));
  SandboxFileChooserDialogClass *klass = SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (sfcd);

  g_signal_emit (self,
                 klass->destroy_signal,
                 0);

  g_object_unref (self);
}

SfcdState
lfcd_get_state (SandboxFileChooserDialog *sfcd)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (LOCAL_IS_FILE_CHOOSER_DIALOG (self), SFCD_WRONG_STATE);
  
  return self->priv->state;
}

const gchar *
lfcd_get_state_printable  (SandboxFileChooserDialog *sfcd)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (LOCAL_IS_FILE_CHOOSER_DIALOG (self), SfcdStatePrintable[SFCD_WRONG_STATE]);
  g_return_val_if_fail (self->priv->state > SFCD_WRONG_STATE, SfcdStatePrintable[SFCD_WRONG_STATE]);
  g_return_val_if_fail (self->priv->state < SFCD_LAST_STATE, SfcdStatePrintable[SFCD_WRONG_STATE]);

  return SfcdStatePrintable [self->priv->state];
}

static const gchar *
lfcd_get_dialog_title (SandboxFileChooserDialog *sfcd)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (LOCAL_IS_FILE_CHOOSER_DIALOG (self), NULL);
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_DIALOG (self->priv->dialog), NULL);

  return gtk_window_get_title (GTK_WINDOW (self->priv->dialog));
}

gboolean
lfcd_is_running (SandboxFileChooserDialog *sfcd)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (LOCAL_IS_FILE_CHOOSER_DIALOG (self), FALSE);

  return self->priv->state == SFCD_RUNNING;
}

//TODO use an ID that does not inform of the memory layout!
const gchar *
lfcd_get_id (SandboxFileChooserDialog *sfcd)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (LOCAL_IS_FILE_CHOOSER_DIALOG (self), NULL);

  return self->priv->id;
}

static gboolean
_lfcd_entry_sanity_check (LocalFileChooserDialog    *self,
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

  // If the callee gave us a non-valid LocalFileChooserDialog
  if (!LOCAL_IS_FILE_CHOOSER_DIALOG (self))
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
/* Struct to transfer data to the running function and its handlers */
typedef struct _LfcdRunFuncData{
  LocalFileChooserDialog     *lfcd;
  GMainLoop                  *loop;
  gint                        response_id;
  gboolean                    was_modal;
  gulong                      response_handler;
  gulong                      unmap_handler;
  gulong                      destroy_handler;
  gulong                      delete_handler;
} LfcdRunFuncData;


// Handlers below taken from GtkDialog.c (GTK+ 3.10) and then modified
static inline void
shutdown_loop (LfcdRunFuncData *d)
{
  if (g_main_loop_is_running (d->loop))
    g_main_loop_quit (d->loop);
}

static void
run_unmap_handler (GtkDialog *dialog,
                   gpointer   data)
{
  LfcdRunFuncData *d = data;

  shutdown_loop (d);
}

static void
run_response_handler (GtkDialog *dialog,
                      gint       response_id,
                      gpointer   data)
{
  LfcdRunFuncData *d = data;

  d->response_id = response_id;

  shutdown_loop (d);
}

static gint
run_delete_handler (GtkDialog   *dialog,
                    GdkEventAny *event,
                    gpointer     data)
{
  LfcdRunFuncData *d = data;

  shutdown_loop (d);

  return TRUE; /* Do not destroy */
}

static void
run_destroy_handler (GtkDialog *dialog,
                     gpointer   data)
{
  /* shutdown_loop will be called by run_unmap_handler */

  // This should never happen, noone should destroy the dialog on our behalf.
  // We wouldn't recover from the dialog being missing anyway.
  g_assert_not_reached ();
}

gboolean
_lfcd_run_func (gpointer data)
{
  LfcdRunFuncData *d = (LfcdRunFuncData *) data;

  g_return_val_if_fail (d != NULL, G_SOURCE_REMOVE);
  g_return_val_if_fail (d->lfcd != NULL, G_SOURCE_REMOVE);

  LocalFileChooserDialog        *self  = d->lfcd;
  SandboxFileChooserDialog      *sfcd  = SANDBOX_FILE_CHOOSER_DIALOG (self);
  SandboxFileChooserDialogClass *klass = SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (sfcd);


G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_threads_leave ();
  g_main_loop_run (d->loop);
  gdk_threads_enter ();
G_GNUC_END_IGNORE_DEPRECATIONS

  syslog (LOG_DEBUG, "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') has finished running (return code is %d).\n",
          sfcd_get_id (sfcd), gtk_window_get_title (GTK_WINDOW (self->priv->dialog)), d->response_id);

  g_mutex_lock (&self->priv->stateMutex);

  // Clean up signal handlers and loop
  g_signal_handler_disconnect (self->priv->dialog, d->response_handler);
  g_signal_handler_disconnect (self->priv->dialog, d->unmap_handler);
  g_signal_handler_disconnect (self->priv->dialog, d->delete_handler);
  g_signal_handler_disconnect (self->priv->dialog, d->destroy_handler);

  // Free the loop after handlers have been disconnected
  g_main_loop_unref (d->loop);
  d->loop = NULL;

  // Destroying the dialog (via Destroy or the WM) results in a GTK_RESPONSE_DELETE_EVENT.
  // In such a case, we destroy the dialog and notify the client process.
  if (d->response_id == GTK_RESPONSE_DELETE_EVENT)
  {
    syslog (LOG_DEBUG, "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') was marked for deletion while running, will be deleted.\n",
            sfcd_get_id (sfcd), gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

    // We drop our own reference to get the object destroyed. If no other method
    // is being called, then the object will reach a refcount of 0 and dispose.
    // sfcd_destroy() will signal deletion back to owners of this dialog.
    sfcd_destroy (sfcd);
  }
  else
  {
    // Well, should still be running at that point!
    g_assert (sfcd_is_running (sfcd));

    // Dialog clean-up. TODO: have the compo manage this
    if (!d->was_modal)
      gtk_window_set_modal (GTK_WINDOW(self->priv->dialog), FALSE);

    // Client can now query and destroy the dialog -- hide it in the meantime
    gtk_widget_hide (self->priv->dialog);

    // Negative answers should not lead to data retrieval
    if (!_lfcd_is_stock_accept_response_id (d->response_id))
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') ran and the user picked a negative response, returning to '%s' state.\n",
              sfcd_get_id (sfcd),
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
              SfcdStatePrintable [SFCD_CONFIGURATION]);

      self->priv->state = SFCD_CONFIGURATION;
    }
    else
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') will now switch to '%s' state.\n",
              sfcd_get_id (sfcd),
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
              SfcdStatePrintable [SFCD_DATA_RETRIEVAL]);

      self->priv->state = SFCD_DATA_RETRIEVAL;
    }

    g_signal_emit (sfcd,
                   klass->response_signal,
                   0,
                   d->response_id,
                   self->priv->state);
  }

  g_mutex_unlock (&self->priv->stateMutex);

  // Done running, we can relax that extra reference that protected our dialog
  g_object_unref (self);

  g_free (d); // should move to priv -- code here will look better

  return G_SOURCE_REMOVE;
}

static void
lfcd_run (SandboxFileChooserDialog *sfcd,
          GError                  **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  // It doesn't make sense to call run when the dialog's already running
  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.Run: dialog '%s' ('%s') is already running.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

    syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.Run: dialog '%s' ('%s') switching state from '%s' to '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            sfcd_get_state_printable (sfcd),
            SfcdStatePrintable [SFCD_RUNNING]);

    // Now running, prevent destruction - refs are used to allow keeping the 
    // dialog alive until the very end!
    self->priv->state = SFCD_RUNNING;
    g_object_ref (self);
            
    // Data shared between Run call and the idle func running the dialog
    LfcdRunFuncData *d = g_malloc (sizeof (LfcdRunFuncData));
    d->lfcd = self;
    d->loop = g_main_loop_new (NULL, FALSE);
    d->response_id = GTK_RESPONSE_NONE;
    d->was_modal = gtk_window_get_modal (GTK_WINDOW (self->priv->dialog));

    d->response_handler = g_signal_connect (self->priv->dialog,
                                            "response",
                                            G_CALLBACK (run_response_handler),
                                            d);

    d->unmap_handler = g_signal_connect (self->priv->dialog,
                                         "unmap",
                                         G_CALLBACK (run_unmap_handler),
                                         d);

    d->delete_handler = g_signal_connect (self->priv->dialog,
                                          "delete-event",
                                          G_CALLBACK (run_delete_handler),
                                          d);

    d->destroy_handler = g_signal_connect (self->priv->dialog,
                                           "destroy",
                                           G_CALLBACK (run_destroy_handler),
                                           d);

    // TODO tell the compo to make the dialog modal, if requested by the client
    if (!d->was_modal)
      gtk_window_set_modal (GTK_WINDOW (self->priv->dialog), TRUE);

    // TODO tell the compo to show the dialog as if it were the client's child
    if (!gtk_widget_get_visible (GTK_WIDGET (self->priv->dialog)))
      gtk_widget_show (GTK_WIDGET (self->priv->dialog));    

    syslog (LOG_DEBUG, "SandboxFileChooserDialog.Run: dialog '%s' ('%s') is about to run.\n",
          sfcd_get_id (sfcd), sfcd_get_dialog_title (sfcd));

    gdk_threads_add_idle_full (G_PRIORITY_DEFAULT, _lfcd_run_func, d, NULL);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

void
lfcd_present (SandboxFileChooserDialog  *sfcd,
              GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (!sfcd_is_running (sfcd))
  {
    g_set_error (error,
                g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                SFCD_ERROR_FORBIDDEN_CHANGE,
                "SandboxFileChooserDialog.Present: dialog '%s' ('%s') is not running and cannot be presented.\n",
                sfcd_get_id (sfcd),
                sfcd_get_dialog_title (sfcd));

    syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.Present: dialog '%s' ('%s') being presented.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd));

    gtk_window_present (GTK_WINDOW (self->priv->dialog));
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

static void
lfcd_cancel_run (SandboxFileChooserDialog  *sfcd,
                 GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (!sfcd_is_running (sfcd))
  {
    g_set_error (error,
                g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                SFCD_ERROR_FORBIDDEN_CHANGE,
                "SandboxFileChooserDialog.CancelRun: dialog '%s' ('%s') is not running and cannot be cancelled.\n",
                sfcd_get_id (sfcd),
                sfcd_get_dialog_title (sfcd));

    syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.CancelRun: dialog '%s' ('%s') is about to be cancelled.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd));

    // This is enough to cause the run method to issue a GTK_RESPONSE_NONE
    gtk_widget_hide (self->priv->dialog);  
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

static void
lfcd_set_action (SandboxFileChooserDialog  *sfcd,
                 GtkFileChooserAction       action,
                 GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetAction: dialog '%s' ('%s') is already running and cannot be modified (parameter was %d).\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 action);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetAction: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_action (GTK_FILE_CHOOSER (self->priv->dialog), action);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetAction: dialog '%s' ('%s') now has action '%d'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            action);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

static GtkFileChooserAction
lfcd_get_action (SandboxFileChooserDialog *sfcd,
                 GError                  **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  GtkFileChooserAction result = GTK_FILE_CHOOSER_ACTION_OPEN;

  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), result);

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetAction: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    result = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetAction: dialog '%s' ('%s') has action '%d'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            result);
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return result;
}

static void
lfcd_set_local_only (SandboxFileChooserDialog  *sfcd,
                     gboolean                   local_only,
                     GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetLocalOnly: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 _B (local_only));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetLocalOnly: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (self->priv->dialog), local_only);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetLocalOnly: dialog '%s' ('%s') now has local-only '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            _B (local_only));
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

static gboolean
lfcd_get_local_only (SandboxFileChooserDialog  *sfcd,
                     GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  gboolean result = FALSE;

  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), result);

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetLocalOnly: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    result = gtk_file_chooser_get_local_only (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetLocalOnly: dialog '%s' ('%s') has local-only '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            _B (result));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return result;
}

static void
lfcd_set_select_multiple (SandboxFileChooserDialog *sfcd,
                          gboolean                   select_multiple,
                          GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetSelectMultiple: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 _B (select_multiple));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetSelectMultiple: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (self->priv->dialog), select_multiple);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetSelectMultiple: dialog '%s' ('%s') now has select-multiple '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            _B (select_multiple));
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

static gboolean
lfcd_get_select_multiple (SandboxFileChooserDialog  *sfcd,
                          GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  gboolean result = FALSE;

  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), result);

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetSelectMultiple: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    result = gtk_file_chooser_get_select_multiple (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetSelectMultiple: dialog '%s' ('%s') has select-multiple '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            _B (result));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return result;
}

static void
lfcd_set_show_hidden (SandboxFileChooserDialog  *sfcd,
                      gboolean                   show_hidden,
                      GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetShowHidden: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 _B (show_hidden));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetShowHidden: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_show_hidden (GTK_FILE_CHOOSER (self->priv->dialog), show_hidden);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetShowHidden: dialog '%s' ('%s') now has show-hidden '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            _B (show_hidden));
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

static gboolean
lfcd_get_show_hidden (SandboxFileChooserDialog *sfcd,
                      GError                  **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  gboolean result = FALSE;

  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), result);

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetShowHidden: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    result = gtk_file_chooser_get_show_hidden (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetShowHidden: dialog '%s' ('%s') has show-hidden '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            _B (result));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return result;
}

static void
lfcd_set_do_overwrite_confirmation (SandboxFileChooserDialog  *sfcd,
                                    gboolean                   do_overwrite_confirmation,
                                    GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetDoOverwriteConfirmation: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 _B (do_overwrite_confirmation));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetDoOverwriteConfirmation: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (self->priv->dialog), do_overwrite_confirmation);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetDoOverwriteConfirmation: dialog '%s' ('%s') now has show-hidden '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            _B (do_overwrite_confirmation));
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

static gboolean
lfcd_get_do_overwrite_confirmation (SandboxFileChooserDialog  *sfcd,
                                    GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  gboolean result = FALSE;

  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), result);

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetDoOverwriteConfirmation: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    result = gtk_file_chooser_get_do_overwrite_confirmation (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetDoOverwriteConfirmation: dialog '%s' ('%s') has show-hidden '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            _B (result));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return result;
}

static void
lfcd_set_create_folders (SandboxFileChooserDialog  *sfcd,
                         gboolean                   create_folders,
                         GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetCreateFolders: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 _B (create_folders));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetCreateFolders: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER (self->priv->dialog), create_folders);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetCreateFolders: dialog '%s' ('%s') now has show-hidden '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            _B (create_folders));
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

static gboolean
lfcd_get_create_folders (SandboxFileChooserDialog  *sfcd,
                         GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  gboolean result = FALSE;

  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), result);

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetCreateFolders: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    result = gtk_file_chooser_get_create_folders (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetCreateFolders: dialog '%s' ('%s') has show-hidden '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            _B (result));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return result;
}

static void
lfcd_set_current_name (SandboxFileChooserDialog  *sfcd,
                       const gchar               *name,
                       GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetCurrentName: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 name);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetCurrentName: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (self->priv->dialog), name);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetCurrentName: dialog '%s' ('%s')'s typed name is now '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            name);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

static void
lfcd_set_filename (SandboxFileChooserDialog  *sfcd,
                   const gchar               *filename,
                   GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetFilename: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 filename);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetFilename: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (self->priv->dialog), filename);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetFilename: dialog '%s' ('%s')'s current file name now is '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            filename);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

static void
lfcd_set_current_folder (SandboxFileChooserDialog  *sfcd,
                         const gchar               *filename,
                         GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetCurrentFolder: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 filename);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetCurrentFolder: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (self->priv->dialog), filename);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetCurrentFolder: dialog '%s' ('%s')'s current folder now is '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            filename);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

static void
lfcd_set_uri (SandboxFileChooserDialog  *sfcd,
              const gchar               *uri,
              GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetUri: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 uri);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetUri: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (self->priv->dialog), uri);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetUri: dialog '%s' ('%s')'s current file uri now is '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            uri);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

static void
lfcd_set_current_folder_uri (SandboxFileChooserDialog  *sfcd,
                             const gchar               *uri,
                             GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_if_fail (_lfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetCurrentFolderUri: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 uri);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetCurrentFolderUri: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (self->priv->dialog), uri);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetCurrentFolderUri: dialog '%s' ('%s')'s current folder uri now is '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            uri);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

static gboolean
lfcd_add_shortcut_folder (SandboxFileChooserDialog  *sfcd,
                          const gchar               *folder,
                          GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.AddShortcutFolder: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 folder);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.AddShortcutFolder: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    succeeded = gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (self->priv->dialog), folder, error);

    if (succeeded)
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.AddShortcutFolder: dialog '%s' ('%s') has been added a shortcut folder named '%s'.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd),
              folder);
    else
    {
      g_prefix_error (error,
                      "SandboxFileChooserDialog.AddShortcutFolder: dialog '%s' ('%s') did not allow adding a shortcut folder named '%s').\n",
                      sfcd_get_id (sfcd),
                      sfcd_get_dialog_title (sfcd),
                      folder);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
    }
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

static gboolean
lfcd_remove_shortcut_folder (SandboxFileChooserDialog  *sfcd,
                             const gchar               *folder,
                             GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.RemoveShortcutFolder: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 folder);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.RemoveShortcutFolder: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    succeeded = gtk_file_chooser_remove_shortcut_folder (GTK_FILE_CHOOSER (self->priv->dialog), folder, error);

    if (succeeded)
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.RemoveShortcutFolder: dialog '%s' ('%s')'s shortcut folder named '%s' has been removed.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd),
              folder);
    else
    {
      g_prefix_error (error,
                      "SandboxFileChooserDialog.RemoveShortcutFolder: dialog '%s' ('%s') did not allow removing a shortcut folder named '%s').\n",
                      sfcd_get_id (sfcd),
                      sfcd_get_dialog_title (sfcd),
                      folder);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
    }
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

static GSList *
lfcd_list_shortcut_folders (SandboxFileChooserDialog *sfcd,
                            GError                    **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);
  GSList *list = NULL;

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.ListShortcutFolders: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    list = gtk_file_chooser_list_shortcut_folders (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.ListShortcutFolders: dialog '%s' ('%s')'s list of shortcuts contains %u elements.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            (list == NULL? 0:g_slist_length (list)));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return list;
}

static gboolean
lfcd_add_shortcut_folder_uri (SandboxFileChooserDialog  *sfcd,
                              const gchar               *uri,
                              GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.AddShortcutFolderUri: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 uri);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.AddShortcutFolderUri: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    succeeded = gtk_file_chooser_add_shortcut_folder_uri (GTK_FILE_CHOOSER (self->priv->dialog), uri, error);

    if (succeeded)
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.AddShortcutFolderUri: dialog '%s' ('%s') has been added a shortcut folder named '%s'.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd),
              uri);
    else
    {
      g_prefix_error (error,
                      "SandboxFileChooserDialog.AddShortcutFolderUri: dialog '%s' ('%s') did not allow adding a shortcut folder named '%s').\n",
                      sfcd_get_id (sfcd),
                      sfcd_get_dialog_title (sfcd),
                      uri);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
    }
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

static gboolean
lfcd_remove_shortcut_folder_uri (SandboxFileChooserDialog *sfcd,
                                 const gchar               *uri,
                                 GError                   **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.RemoveShortcutFolderUri: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd),
                 uri);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.RemoveShortcutFolderUri: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd));
    }

    self->priv->state = SFCD_CONFIGURATION;
    succeeded = gtk_file_chooser_remove_shortcut_folder_uri (GTK_FILE_CHOOSER (self->priv->dialog), uri, error);

    if (succeeded)
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.RemoveShortcutFolderUri: dialog '%s' ('%s')'s shortcut folder named '%s' has been removed.\n",
              sfcd_get_id (sfcd),
              sfcd_get_dialog_title (sfcd),
              uri);
    else
    {
      g_prefix_error (error,
                      "SandboxFileChooserDialog.RemoveShortcutFolderUri: dialog '%s' ('%s') did not allow removing a shortcut folder named '%s').\n",
                      sfcd_get_id (sfcd),
                      sfcd_get_dialog_title (sfcd),
                      uri);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
    }
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

static GSList *
lfcd_list_shortcut_folder_uris (SandboxFileChooserDialog *sfcd,
                                GError                    **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);
  GSList *list = NULL;

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.ListShortcutFoldersUri: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    list = gtk_file_chooser_list_shortcut_folder_uris (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.ListShortcutFoldersUri: dialog '%s' ('%s')'s list of shortcuts contains %u elements.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            (list == NULL? 0:g_slist_length (list)));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return list;
}

static gchar *
lfcd_get_current_name (SandboxFileChooserDialog *sfcd,
                       GError                    **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), NULL);

  gchar *name = NULL;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_get_state (sfcd) != SFCD_DATA_RETRIEVAL)
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetCurrentName: dialog '%s' ('%s') is being configured or running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    name = gtk_file_chooser_get_current_name (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetCurrentName: dialog '%s' ('%s')'s typed name currently is '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            name);
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return name;
}

static gchar *
lfcd_get_filename (SandboxFileChooserDialog *sfcd,
                   GError                    **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), NULL);

  gchar *name = NULL;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_get_state (sfcd) != SFCD_DATA_RETRIEVAL)
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetFilename: dialog '%s' ('%s') is being configured or running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetFilename: dialog '%s' ('%s')'s current file name is '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            name);
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return name;
}

static GSList *
lfcd_get_filenames (SandboxFileChooserDialog *sfcd,
                    GError                    **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);

  GSList *list = NULL;

  if (sfcd_get_state (sfcd) != SFCD_DATA_RETRIEVAL)
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetFilenames: dialog '%s' ('%s') is being configured or running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    list = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetFilenames: dialog '%s' ('%s')'s list of current file names contains %u elements.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            (list == NULL? 0:g_slist_length (list)));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return list;
}

static gchar *
lfcd_get_current_folder (SandboxFileChooserDialog *sfcd,
                         GError                    **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);
  
  gchar *folder = NULL;

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetCurrentFolder: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetCurrentFolder: dialog '%s' ('%s')'s current folder is '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            folder);
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return folder;
}

static gchar *
lfcd_get_uri (SandboxFileChooserDialog *sfcd,
              GError                    **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);
  gchar *uri = NULL;

  if (sfcd_get_state (sfcd) != SFCD_DATA_RETRIEVAL)
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetUri: dialog '%s' ('%s') is being configured or running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetUri: dialog '%s' ('%s')'s current file uri is '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            uri);
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return uri;
}

static GSList *
lfcd_get_uris (SandboxFileChooserDialog *sfcd,
               GError                    **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);
  
  GSList *list = NULL;

  if (sfcd_get_state (sfcd) != SFCD_DATA_RETRIEVAL)
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetUris: dialog '%s' ('%s') is being configured or running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 sfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    list = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetUris: dialog '%s' ('%s')'s list of current file uris contains %u elements.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            (list == NULL? 0:g_slist_length (list)));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return list;
}

static gchar *
lfcd_get_current_folder_uri (SandboxFileChooserDialog *sfcd,
                             GError                    **error)
{
  LocalFileChooserDialog *self = LOCAL_FILE_CHOOSER_DIALOG (sfcd);
  g_return_val_if_fail (_lfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);
  
  gchar *uri = NULL;

  if (sfcd_is_running (sfcd))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetCurrentFolderUri: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 sfcd_get_id (sfcd),
                 lfcd_get_dialog_title (sfcd));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  
  else
  {
    uri = gtk_file_chooser_get_current_folder_uri (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetCurrentFolderUri: dialog '%s' ('%s')'s current folder uri is '%s'.\n",
            sfcd_get_id (sfcd),
            sfcd_get_dialog_title (sfcd),
            uri);
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return uri;
}

static void
lfcd_class_init (LocalFileChooserDialogClass *klass)
{
  SandboxFileChooserDialogClass *sfcd_class = SANDBOX_FILE_CHOOSER_DIALOG_CLASS (klass);
  GObjectClass  *g_object_class = G_OBJECT_CLASS(klass);

  //FIXME maybe reintroduce signals here?

  /* Hook finalization functions */
  g_object_class->dispose = lfcd_dispose; /* instance destructor, reverse of init */
  g_object_class->finalize = lfcd_finalize; /* class finalization, reverse of class init */

  /* Hook SandboxFileChooserDialog API functions */
  sfcd_class->get_state = lfcd_get_state;
  sfcd_class->get_state_printable = lfcd_get_state_printable;
  sfcd_class->is_running = lfcd_is_running;
  sfcd_class->destroy = lfcd_destroy;
  sfcd_class->get_dialog_title = lfcd_get_dialog_title;
  sfcd_class->get_id = lfcd_get_id;
  sfcd_class->run = lfcd_run;
  sfcd_class->present = lfcd_present;
  sfcd_class->cancel_run = lfcd_cancel_run;
  sfcd_class->set_action = lfcd_set_action;
  sfcd_class->get_action = lfcd_get_action;
  sfcd_class->set_local_only = lfcd_set_local_only;
  sfcd_class->get_local_only = lfcd_get_local_only;
  sfcd_class->set_select_multiple  = lfcd_set_select_multiple ;
  sfcd_class->get_select_multiple  = lfcd_get_select_multiple ;
  sfcd_class->set_show_hidden = lfcd_set_show_hidden;
  sfcd_class->get_show_hidden = lfcd_get_show_hidden;
  sfcd_class->set_do_overwrite_confirmation = lfcd_set_do_overwrite_confirmation;
  sfcd_class->get_do_overwrite_confirmation = lfcd_get_do_overwrite_confirmation;
  sfcd_class->set_create_folders = lfcd_set_create_folders;
  sfcd_class->get_create_folders = lfcd_get_create_folders;
  sfcd_class->set_current_name = lfcd_set_current_name;
  sfcd_class->set_filename = lfcd_set_filename;
  sfcd_class->set_current_folder = lfcd_set_current_folder;
  sfcd_class->set_uri = lfcd_set_uri;
  sfcd_class->set_current_folder_uri = lfcd_set_current_folder_uri;
  sfcd_class->add_shortcut_folder = lfcd_add_shortcut_folder;
  sfcd_class->remove_shortcut_folder = lfcd_remove_shortcut_folder;
  sfcd_class->list_shortcut_folders = lfcd_list_shortcut_folders;
  sfcd_class->add_shortcut_folder_uri = lfcd_add_shortcut_folder_uri;
  sfcd_class->remove_shortcut_folder_uri = lfcd_remove_shortcut_folder_uri;
  sfcd_class->list_shortcut_folder_uris = lfcd_list_shortcut_folder_uris;
  sfcd_class->get_current_name = lfcd_get_current_name;
  sfcd_class->get_filename = lfcd_get_filename;
  sfcd_class->get_filenames = lfcd_get_filenames;
  sfcd_class->get_current_folder = lfcd_get_current_folder;
  sfcd_class->get_uri = lfcd_get_uri;
  sfcd_class->get_uris = lfcd_get_uris;
  sfcd_class->get_current_folder_uri = lfcd_get_current_folder_uri;
}
