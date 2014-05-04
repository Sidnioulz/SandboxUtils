/*
 * sandboxfilechooserdialog.c: file chooser dialog for sandboxed apps
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
 * SECTION:sandboxfilechooserdialog
 * @Title: SandboxFileChooserDialog
 * @Short_description: A dialog for choosing files meant to be manipulated over
 * IPC by a sandboxed application
 * @See_also: #GAppInfo
 *
 * #SandboxFileChooserDialog is an API for a file chooser dialog, based on the
 * #GtkFileChooserDialog class and #GtkFileChooser interface. This dialog can be
 * used to provide an interface to open files or directories, or to save files.
 * 
 * #SandboxFileChooserDialog is designed to be wrapped and exposed over an IPC
 * mechanism. The process handling the class on behalf of the sandboxed app is
 * in charge of managing access control to make sure the app can read and write
 * files that were selected in the dialog. This is best done from within the IPC
 * wrapper rather than by modifying this class.
 *
 * This class is thread-safe so long as you make sure to return from all of an
 * instance's methods before destroying it.
 *
 * It differs with other dialogs in that it has a #SfcdState among:
 * 
 * %SFCD_CONFIGURATION: initial state, in which the dialog's behavior, current
 *   file and directory, etc. can be modified to match your application's needs
 * %SFCD_RUNNING: switched to by calling sfcd_run(), in which the dialog cannot
 *   be modified or queried and can only be cancelled (see sfcd_cancel_run()),
 *   presented (sfcd_present()) or destroyed (sfcd_destroy())
 * %SFCD_DATA_RETRIEVAL: retrieval: switched to automatically after a successful
 *   run, in which the dialog cannot be modified but the file(s) currently
 *   selected can be queried as in the original #GtkFileChooserDialog
 *
 * States allow preventing attacks where an app would modify the dialog after
 * the user made a selection and before they clicked on the dialog's appropriate
 * button. If the dialog is modified or reused, it switches back to a previous 
 * state in a rather conservative way, both to provide consistency on the
 * restrictions it imposes and to allow some level of object reuse.
 *
 * The %SFCD_CONFIGURATION state is how a dialog starts. In this mode, you can
 * call most setter methods to prepare the dialog before displaying it to the 
 * user. You cannot yet retrieve any chosen file since the user hasn't seen the
 * dialog! You cannot call any configuration method while the dialog is running
 * (to prevent you from forcing the user into accepting a certain file), and 
 * calling configuration methods after the dialog has run will cause it to
 * return to this state, so you can reconfigure and rerun the dialog easily. You
 * should use sfcd_run() to switch to the next state.
 *
 * The %SFCD_RUNNING state occurs from the moment you call sfcd_run() until the
 * moment the #SandboxFileChooserDialog::response signal is emitted. In this
 * mode, you cannot modify or query the dialog at all. All you can do is cancel
 * or destroy it, and present it to the user if need be. The dialog will either
 * switch back to the %SFCD_CONFIGURATION mode if it was cancelled or destroyed
 * (by you or by the user) or will transition to the %SFCD_DATA_RETRIEVAL mode
 * if the user successfuly selected a file/folder to open or filename to save to.
 * 
 * The %SFCD_DATA_RETRIEVAL state is for when the dialog has been successfully
 * run. If the action of the dialog was @GTK_FILE_CHOOSER_ACTION_OPEN or
 * @GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, retrieving the filenames selected by
 * the user will grant your application access to these files (see sfcd_get_uri()).
 * In the case of @GTK_FILE_CHOOSER_ACTION_SAVE or @GTK_FILE_CHOOSER_CREATE_FOLDER,
 * you will be granted write access to the current name of the dialog upon 
 * retrieving it (see sfcd_get_current_name ()).
 *
 * As of 0.4, it has not yet been decided whether your application will receive
 * file paths or file descriptors or both in the %SFCD_DATA_RETRIEVAL state.
 *
 * Since: 0.3
 **/

//TODO copy extra sections from gtkfilechooser such as gtkfilechooserdialog-setting-up

#include <glib.h>
#include <gtk/gtk.h>
#include <syslog.h>

#include "sandboxfilechooserdialog.h"
#include "sandboxutilsmarshals.h"

// Needed when -O0 is turned on
extern const gchar * g_error_get_message (GError *err);

struct _SandboxFileChooserDialogPrivate
{
  GtkWidget             *dialog;      /* pointer to the #GtkFileChooserDialog */
  SfcdState              state;       /* state of the instance */
  GMutex                 stateMutex;  /* a mutex to provide thread-safety */
};


G_DEFINE_TYPE_WITH_PRIVATE (SandboxFileChooserDialog, sfcd, G_TYPE_OBJECT)

static void
sfcd_init (SandboxFileChooserDialog *self)
{
  self->priv = sfcd_get_instance_private (self);

  self->priv->dialog      = NULL;
  self->priv->state       = SFCD_CONFIGURATION;

  g_mutex_init (&self->priv->stateMutex);

  self->id = g_strdup_printf ("%p", self);
}

static void
sfcd_dispose (GObject* object)
{
  SandboxFileChooserDialog *self = SANDBOX_FILE_CHOOSER_DIALOG (object);

  g_mutex_clear (&self->priv->stateMutex);

  if (self->priv->dialog)
  {
    // Remove our own ref and then destroy the dialog
    g_object_unref (self->priv->dialog);
    gtk_widget_destroy (self->priv->dialog);
  }

  syslog (LOG_DEBUG, "SandboxFileChooserDialog.Dispose: dialog '%s' was disposed.\n",
              self->id);

  g_free (self->id);

  G_OBJECT_CLASS (sfcd_parent_class)->dispose (object);
}

static void
sfcd_finalize (GObject* object)
{
  G_OBJECT_CLASS (sfcd_parent_class)->finalize (object);
}

static void
sfcd_class_init (SandboxFileChooserDialogClass *klass)
{
  GObjectClass  *g_object_class = G_OBJECT_CLASS(klass);

  /**
   * SandboxFileChooserDialog::run-finished:
   * @object: the object which will receive the signal.
   * @dialog_id: the id of the dialog that ran and sent the signal
   * @response_id: the response id obtained when the dialog finished running
   * @return_state: the state of the dialog after running
   * @destroyed: whether the dialog should be considered destroyed or not
   */
  klass->run_finished_signal =
      g_signal_new ("run-finished",
                    G_TYPE_FROM_CLASS (g_object_class),
                    G_SIGNAL_RUN_FIRST,
                    0,
                    NULL, NULL,
                    sandboxutils_marshal_VOID__STRING_INT_INT_BOOLEAN,
                     G_TYPE_NONE, 4,
                    G_TYPE_STRING,
                    G_TYPE_INT,
                    G_TYPE_INT,
                    G_TYPE_BOOLEAN);

  /* Hook finalization functions */
  g_object_class->dispose = sfcd_dispose; /* instance destructor, reverse of init */
  g_object_class->finalize = sfcd_finalize; /* class finalization, reverse of class init */
}

/**
 * sfcd_new:
 * @dialog: a #GtkFileChooserDialog
 * 
 * Creates a new instance of #SandboxFileChooserDialog and associates @dialog
 * with it. Fails if @dialog is %NULL. Free with sfcd_destroy(). FIXME
 *
 * This is equivalent to gtk_file_chooser_dialog_new() in the GTK+ API.
 *
 * Since: 0.3
 *
 * Returns: the new dialog as a #SandboxFileChooserDialog
 *
 FIXME change the API to make it not use a GtkWidget
 FIXME where are the properties? the ctor must handle and dispatch them
 **/
SandboxFileChooserDialog *
sfcd_new (GtkWidget *dialog)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_DIALOG (dialog), NULL);

  SandboxFileChooserDialog *sfcd = g_object_new (SANDBOX_TYPE_FILE_CHOOSER_DIALOG, NULL);
  g_assert (sfcd != NULL);

  sfcd->priv->dialog = dialog;

  // Reference the dialog to make sure it stays with us until the end
  g_object_ref (dialog);

  syslog (LOG_DEBUG, "SandboxFileChooserDialog.New: dialog '%s' ('%s') has just been created.\n",
              sfcd->id, gtk_window_get_title (GTK_WINDOW (dialog)));
  
  return sfcd;
}

/**
 * sfcd_destroy:
 * @dialog: a #SandboxFileChooserDialog
 * 
 * Destroys the given instance of #SandboxFileChooserDialog. Internally, this
 * method only removes one reference to the dialog, so it may continue existing
 * if you still have other references to it.
 * 
 * This method can be called from any #SfcdState. It is equivalent to
 * gtk_widget_destroy() in the GTK+ API.
 *
 * Since: 0.3
 **/
void
sfcd_destroy (SandboxFileChooserDialog *self)
{
  g_return_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self));

  syslog (LOG_DEBUG, "SandboxFileChooserDialog.Destroy: dialog '%s' ('%s')'s reference count has been decreased by one.\n",
              self->id, sfcd_get_dialog_title (self));

  g_object_unref (self);
}

/**
 * sfcd_get_state:
 * @dialog: a #SandboxFileChooserDialog
 * 
 * Gets the current #SfcdState of an instance of #SandboxFileChooserDialog.
 *
 * This method can be called from any #SfcdState. It has no GTK+ equivalent.
 *
 * Returns: the state of the @dialog
 *
 * Since: 0.3
 **/
SfcdState
sfcd_get_state (SandboxFileChooserDialog *self)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), SFCD_WRONG_STATE);

  return self->priv->state;
}

/**
 * sfcd_get_state_printable:
 * @dialog: a #SandboxFileChooserDialog
 * 
 * Gets the current #SfcdState of an instance of #SandboxFileChooserDialog,
 * in the form of a string that can be displayed. The string is not localized.
 *
 * This method can be called from any #SfcdState. It has no GTK+ equivalent.
 *
 * Returns: the state of the @dialog in the form of a #G_TYPE_STRING
 *
 * Since: 0.3
 **/
const gchar *
sfcd_get_state_printable  (SandboxFileChooserDialog *self)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), SfcdStatePrintable[SFCD_WRONG_STATE]);

  return SfcdStatePrintable [self->priv->state];
}

/**
 * sfcd_get_dialog_title:
 * @dialog: a #SandboxFileChooserDialog
 * 
 * Gets the title of the #GtkFileChooserDialog embedded in an instance of
 * #SandboxFileChooserDialog. 
 *
 * This method can be called from any #SfcdState. It is equivalent to
 * gtk_window_get_title() in the GTK+ API.
 *
 * Returns: the title of the dialog embedded within the @dialog
 *
 * Since: 0.3
 **/
const gchar *
sfcd_get_dialog_title (SandboxFileChooserDialog *self)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), NULL);
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_DIALOG (self->priv->dialog), NULL);

  return gtk_window_get_title (GTK_WINDOW (self->priv->dialog));
}

/**
 * sfcd_get_state:
 * @dialog: a #SandboxFileChooserDialog
 * 
 * Returns whether an instance of #SandboxFileChooserDialog is currently running,
 * e.g., it is in the %SFCD_RUNNING #SfcdState.
 *
 * This method can be called from any #SfcdState. It has no GTK+ equivalent.
 * 
 * Returns: whether the @dialog is running
 *
 * Since: 0.3
 **/
gboolean
sfcd_is_running (SandboxFileChooserDialog *self)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);

  return self->priv->state == SFCD_RUNNING;
}

/**
 * _sfcd_entry_sanity_check:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placehoder to register a #GError
 * 
 * Performs basic sanity checks and tries to fill the #GError if these fail. In
 * the current API, this allows warning the callee if the @dialog is not correct
 * or @error points to something else than NULL. This is useful because we can
 * propagate the issue all the way back to the client and have them send a more
 * meaningful bug report than "the API did not complain but did nothing".
 *
 * Returns: True if the sanity check succeeds, False if the @dialog or @error
 * were not as expected
 *
 * Since: 0.4
 */
static gboolean
_sfcd_entry_sanity_check (SandboxFileChooserDialog *self,
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

  // If the callee gave us a non-valid SandboxFileChooserDialog
  if (!SANDBOX_IS_FILE_CHOOSER_DIALOG (self))
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
typedef struct _SfcdRunFuncData{
  SandboxFileChooserDialog   *sfcd;
  GMainLoop                  *loop;
  gint                        response_id;
  gboolean                    was_modal;
  gulong                      response_handler;
  gulong                      unmap_handler;
  gulong                      destroy_handler;
  gulong                      delete_handler;
} SfcdRunFuncData;


// Handlers below taken from GtkDialog.c (GTK+ 3.10) and then modified
static inline void
shutdown_loop (SfcdRunFuncData *d)
{
  if (g_main_loop_is_running (d->loop))
    g_main_loop_quit (d->loop);
}

static void
run_unmap_handler (GtkDialog *dialog,
                   gpointer   data)
{
  SfcdRunFuncData *d = data;

  shutdown_loop (d);
}

static void
run_response_handler (GtkDialog *dialog,
                      gint       response_id,
                      gpointer   data)
{
  SfcdRunFuncData *d = data;

  d->response_id = response_id;

  shutdown_loop (d);
}

static gint
run_delete_handler (GtkDialog   *dialog,
                    GdkEventAny *event,
                    gpointer     data)
{
  SfcdRunFuncData *d = data;

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
_sfcd_run_func (gpointer data)
{
  SfcdRunFuncData *d = (SfcdRunFuncData *) data;

  g_return_val_if_fail (d != NULL, G_SOURCE_REMOVE);
  g_return_val_if_fail (d->sfcd != NULL, G_SOURCE_REMOVE);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_threads_leave ();
  g_main_loop_run (d->loop);
  gdk_threads_enter ();
G_GNUC_END_IGNORE_DEPRECATIONS

  syslog (LOG_DEBUG, "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') has finished running (return code is %d).\n",
          d->sfcd->id, gtk_window_get_title (GTK_WINDOW (d->sfcd->priv->dialog)), d->response_id);

  g_mutex_lock (&d->sfcd->priv->stateMutex);

  // Clean up signal handlers and loop
  g_signal_handler_disconnect (d->sfcd->priv->dialog, d->response_handler);
  g_signal_handler_disconnect (d->sfcd->priv->dialog, d->unmap_handler);
  g_signal_handler_disconnect (d->sfcd->priv->dialog, d->delete_handler);
  g_signal_handler_disconnect (d->sfcd->priv->dialog, d->destroy_handler);

  // Free the loop after handlers have been disconnected
  g_main_loop_unref (d->loop);
  d->loop = NULL;

  // Needed to emit signals
  SandboxFileChooserDialogClass *klass = SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (d->sfcd);

  // Destroying the dialog (via Destroy or the WM) results in a GTK_RESPONSE_DELETE_EVENT.
  // In such a case, we destroy the dialog and notify the client process.
  if (d->response_id == GTK_RESPONSE_DELETE_EVENT)
  {
    syslog (LOG_DEBUG, "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') was marked for deletion while running, will be deleted.\n",
            d->sfcd->id, gtk_window_get_title (GTK_WINDOW (d->sfcd->priv->dialog)));

    // Emit a signal that informs of the deletion
    // TODO "destroy/ed" signal?
    g_signal_emit (d->sfcd,
                   klass->run_finished_signal,
                   0, 
                   d->sfcd->id,
                   d->response_id,
                   SFCD_WRONG_STATE,
                   TRUE);
    
    // We drop our own reference to get the object destroyed. If no other method
    // is being called, then the object will reach a refcount of 0 and dispose.
    sfcd_destroy (d->sfcd);
  }
  else
  {
    // Well, should still be running at that point!
    g_assert (sfcd_is_running (d->sfcd));

    // Dialog clean-up. TODO: have the compo manage this
    if (!d->was_modal)
      gtk_window_set_modal (GTK_WINDOW(d->sfcd->priv->dialog), FALSE);

    // Client can now query and destroy the dialog -- hide it in the meantime
    gtk_widget_hide (d->sfcd->priv->dialog);

    // According to the doc, could happen if dialog was destroyed. Ours can only be
    // destroyed via a delete-event though, so NONE shouldn't mean it's destroyed
    if (d->response_id == GTK_RESPONSE_NONE)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') ran and returned no response, will return to '%s' state.\n",
              d->sfcd->id,
              gtk_window_get_title (GTK_WINDOW (d->sfcd->priv->dialog)),
              SfcdStatePrintable [SFCD_CONFIGURATION]);

      d->sfcd->priv->state = SFCD_CONFIGURATION;
    }
    else
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') will now switch to '%s' state.\n",
              d->sfcd->id,
              gtk_window_get_title (GTK_WINDOW (d->sfcd->priv->dialog)),
              SfcdStatePrintable [SFCD_DATA_RETRIEVAL]);

      d->sfcd->priv->state = SFCD_DATA_RETRIEVAL;
    }

    //TODO rename signal to match original API better
    g_signal_emit (d->sfcd,
                   klass->run_finished_signal,
                   0, 
                   d->sfcd->id,
                   d->response_id,
                   d->sfcd->priv->state,
                   FALSE);
  }

  g_mutex_unlock (&d->sfcd->priv->stateMutex);

  // Done running, we can relax that extra reference that protected our dialog
  g_object_unref (d->sfcd);

  g_free (d); // should move to priv -- code here will look better

  return G_SOURCE_REMOVE;
}


/**
 * sfcd_run:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Starts running the @dialog in a separate #GMainLoop. Contrarily to the GTK+
 * gtk_dialog_run() method advocated to run a #GtkFileChooserDialog, this method
 * does not block until a response has been received by the dialog. One should
 * register a callback on the #SandboxFileChooserDialog::response signal instead.
 * For instance, consider the following code taken from the GTK+ documentation:
 *
 * <example id="gtkfilechooser-typical-usage">
 * <title>Typical #GtkFileChooserDialog usage</title>
 * <para>
 * <informalexample><programlisting>
 * GtkWidget *dialog;
 *
 * dialog = gtk_file_chooser_dialog_new ("Open File",
 *                                       parent_window,
 *                                       GTK_FILE_CHOOSER_ACTION_OPEN,
 *                                       _("_Cancel"), GTK_RESPONSE_CANCEL,
 *                                       _("_Open"), GTK_RESPONSE_ACCEPT,
 *                                       NULL);
 *
 * if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
 *   {
 *     char *filename;
 *
 *     filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
 *     open_file (filename);
 *     g_free (filename);
 *   }
 *
 * gtk_widget_destroy (dialog);
 * </programlisting></informalexample>
 * </para>
 * With #SandboxUtils, you would instead do the following:
 * <para>
 * <informalexample><programlisting>
 * static void
 * on_open_dialog_response (SandboxFileChooserDialog *dialog,
 *                          gint                      response_id,
 *                          gpointer                  user_data)
 * {
 *   if (response_id == GTK_RESPONSE_OK)
 *   {
 *     GError *error = NULL;
 *     gchar *filename = sfcd_get_filename (dialog, error);
 * 
 *     // Check for errors - the server might've crashed, or a security policy
 *     // could be getting in the way.
 *     if (!error)
 *     {
 *       my_app_open_file (filename);
 *       g_free (filename);
 *     }
 *     else
 *     {
 *       my_app_report_error (error);
 *       g_error_free (error);
 *     }
 *   }
 * 
 *   sfcd_destroy (dialog);
 * }
 * 
 * [...]
 * 
 * void
 * my_open_function ()
 * {
 *   SandboxFileChooserDialog *dialog;
 * 
 *   dialog = sfcd_new (TRUE, // use the remote dialog that passes through sandboxing
 *                      "Open File",
 *                      parent_window,
 *                      GTK_FILE_CHOOSER_ACTION_OPEN,
 *                      _("_Cancel"), GTK_RESPONSE_CANCEL,
 *                      _("_Open"), GTK_RESPONSE_ACCEPT,
 *                      NULL);
 * 
 *   // Ask the dialog to tell us when the user picked a file
 *   g_signal_connect (dialog,
 *                     "response",
 *                     G_CALLBACK (on_open_dialog_response),
 *                     user_data);
 * 
 *   GError *error = NULL;
 *   sfcd_run (dialog, error);
 * 
 *   // Make sure to verify that the dialog is running
 *   if (error)
 *   {
 *     my_app_report_error (error);
 *     g_error_free (filename);
 *   }
 * }
 * </programlisting></informalexample>
 * </para>
 * </example>
 *
 * Note that if the dialog was destroyed, you will never hear from it again. If
 * you kept an extra reference to the dialog via g_object_ref(), make sure to
 * also connect to #SandboxFileChooserDialog::destroy and to drop that reference
 * when receiving this signal (with g_object_unref()). 
 *
 * This method can be called from the %SFCD_CONFIGURATION or %SFCD_DATA_RETRIEVAL
 * states. It is roughly to gtk_dialog_run() in the GTK+ API. Do remember to
 * check if @error is set after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_run (SandboxFileChooserDialog *self,
          GError                  **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  // It doesn't make sense to call run when the dialog's already running
  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.Run: dialog '%s' ('%s') is already running.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

    syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.Run: dialog '%s' ('%s') switching state from '%s' to '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            sfcd_get_state_printable (self),
            SfcdStatePrintable [SFCD_RUNNING]);

    // Now running, prevent destruction - refs are used to allow keeping the 
    // dialog alive until the very end!
    self->priv->state = SFCD_RUNNING;
    g_object_ref (self);
            
    // Data shared between Run call and the idle func running the dialog
    SfcdRunFuncData *d = g_malloc (sizeof (SfcdRunFuncData));
    d->sfcd = self;
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
          self->id, gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

    gdk_threads_add_idle_full (G_PRIORITY_DEFAULT, _sfcd_run_func, d, NULL);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

/**
 * sfcd_present:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Presents a currently running instance of #SandboxFileChooserDialog. This
 * method fails if the @dialog is not running.
 *
 * This method belongs to the %SFCD_RUNNING state. It is equivalent to
 * gtk_window_present() in the GTK+ API. Do remember to check if @error is set
 * after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_present (SandboxFileChooserDialog  *self,
              GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (!sfcd_is_running (self))
  {
    g_set_error (error,
                g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                SFCD_ERROR_FORBIDDEN_CHANGE,
                "SandboxFileChooserDialog.Present: dialog '%s' ('%s') is not running and cannot be presented.\n",
                self->id,
                gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

    syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.Present: dialog '%s' ('%s') being presented.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

    gtk_window_present (GTK_WINDOW (self->priv->dialog));
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

/**
 * sfcd_cancel_run:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Cancels a currently running instance of #SandboxFileChooserDialog. This
 * method fails if the @dialog is not running. Internally, it hides the dialog's
 * window which causes it to unmap, and the running loop will catch the unmap
 * signal and emit a #SandboxFileChooserDialog::response signal. The
 * @response_id will be set to GTK_RESPONSE_NONE, which you should ignore in
 * the callbacks you connected to this signal. the @dialog will be back into a
 * %SFCD_CONFIGURATION #SfcdState.
 *
 * After being cancelled, the @dialog still exists. If you no longer need it,
 * destroy it with sfcd_destroy(). Note that you can also directly destroy the
 * dialog while running.
 *
 * This method belongs to the %SFCD_RUNNING state. It is equivalent to
 * gtk_window_hide() in the GTK+ API. Do remember to check if @error is set 
 * after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_cancel_run (SandboxFileChooserDialog  *self,
                 GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (!sfcd_is_running (self))
  {
    g_set_error (error,
                g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                SFCD_ERROR_FORBIDDEN_CHANGE,
                "SandboxFileChooserDialog.CancelRun: dialog '%s' ('%s') is not running and cannot be cancelled.\n",
                self->id,
                gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

    syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.CancelRun: dialog '%s' ('%s') is about to be cancelled.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

    // This is enough to cause the run method to issue a GTK_RESPONSE_NONE
    gtk_widget_hide (self->priv->dialog);  
  }

  g_mutex_unlock (&self->priv->stateMutex);
}


/* RUNNING METHODS */
/**
 * sfcd_set_action:
 * @dialog: a #SandboxFileChooserDialog
 * @action: the #GtkFileChooserAction that the dialog is performing
 * @error: a placeholder for a #GError
 * 
 * Sets the #GtkFileChooserAction that the @dialog is performing. This influences
 * whether the @dialog will allow your user to open or save files, or to select
 * or create folders. Look up the #GtkFileChooser documentation for information
 * on which types of are available.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_set_action() in the GTK+ API. Do remember to check if @error
 * is set after running this method.
 *
 * Since: 0.4
 **/
void
sfcd_set_action (SandboxFileChooserDialog  *self,
                 GtkFileChooserAction       action,
                 GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetAction: dialog '%s' ('%s') is already running and cannot be modified (parameter was %d).\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 action);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetAction: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_action (GTK_FILE_CHOOSER (self->priv->dialog), action);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetAction: dialog '%s' ('%s') now has action '%d'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            action);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

/**
 * sfcd_get_action:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Gets the #GtkFileChooserAction that the @dialog is performing; See
 * sfcd_set_action() for more information.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_get_action() in the GTK+ API. Do remember to check if @error
 * is set after running this method. If set, the return value is undefined.
 *
 * Return value: the action that the dialog is performing
 *
 * Since: 0.4
 **/
GtkFileChooserAction
sfcd_get_action (SandboxFileChooserDialog *self,
                 GError                  **error)
{
  GtkFileChooserAction result = GTK_FILE_CHOOSER_ACTION_OPEN;

  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), result);

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetAction: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    result = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetAction: dialog '%s' ('%s') has action '%d'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            result);
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return result;
}

/**
 * sfcd_set_local_only:
 * @dialog: a #SandboxFileChooserDialog
 * @local_only: %TRUE if only local files can be selected
 * @error: a placeholder for a #GError
 * 
 * Sets whether only local files can be selected in the dialog. If @local_only
 * is %TRUE (the default), then the selected files are guaranteed to be
 * accessible through the operating systems native file system and therefore
 * the application only needs to worry about the filename functions in
 * #SandboxFileChooserDialog, like sfcd_get_filename(), rather than the URI
 * functions like sfcd_get_uri(),
 *
 * On some systems non-native files may still be available using the native
 * filesystem via a userspace filesystem (FUSE).
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_set_local_only() in the GTK+ API. Do remember to check if
 * @error is set after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_set_local_only (SandboxFileChooserDialog  *self,
                     gboolean                   local_only,
                     GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetLocalOnly: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 _B (local_only));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetLocalOnly: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (self->priv->dialog), local_only);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetLocalOnly: dialog '%s' ('%s') now has local-only '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            _B (local_only));
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

/**
 * sfcd_get_local_only:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Gets whether only local files can be selected in the dialog; See
 * sfcd_set_local_only() for more information.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_get_local_only() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is
 * undefined.
 *
 * Return value: %TRUE if only local files can be selected
 *
 * Since: 0.3
 **/
gboolean
sfcd_get_local_only (SandboxFileChooserDialog  *self,
                     GError                   **error)
{
  gboolean result = FALSE;

  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), result);

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetLocalOnly: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    result = gtk_file_chooser_get_local_only (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetLocalOnly: dialog '%s' ('%s') has local-only '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            _B (result));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return result;
}

/**
 * sfcd_set_select_multiple:
 * @dialog: a #SandboxFileChooserDialog
 * @select_multiple: %TRUE if multiple files can be selected
 * @error: a placeholder for a #GError
 * 
 * Sets whether multiple files can be selected in the dialog. This is
 * only relevant if the action is set to be %GTK_FILE_CHOOSER_ACTION_OPEN or
 * %GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_set_select_multiple() in the GTK+ API. Do remember to check
 * if @error is set after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_set_select_multiple (SandboxFileChooserDialog  *self,
                          gboolean                   select_multiple,
                          GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetSelectMultiple: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 _B (select_multiple));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetSelectMultiple: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (self->priv->dialog), select_multiple);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetSelectMultiple: dialog '%s' ('%s') now has select-multiple '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            _B (select_multiple));
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

/**
 * sfcd_get_select_multiple:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Gets whether multiple files can be selected in the dialog; See
 * sfcd_set_select_multiple() for more information.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_get_select_multiple() in the GTK+ API. Do remember to check
 * if @error is set after running this method. If set, the return value is
 * undefined.
 *
 * Return value: %TRUE if multiple files can be selected
 *
 * Since: 0.3
 **/
gboolean
sfcd_get_select_multiple (SandboxFileChooserDialog  *self,
                          GError                   **error)
{
  gboolean result = FALSE;

  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), result);

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetSelectMultiple: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    result = gtk_file_chooser_get_select_multiple (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetSelectMultiple: dialog '%s' ('%s') has select-multiple '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            _B (result));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return result;
}

/**
 * sfcd_set_show_hidden:
 * @dialog: a #SandboxFileChooserDialog
 * @show_hidden: %TRUE if hidden files and folders should be displayed
 * @error: a placeholder for a #GError
 * 
 * Sets whether hidden files and folders are displayed in the file dialog.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_set_show_hidden() in the GTK+ API. Do remember to check
 * if @error is set after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_set_show_hidden (SandboxFileChooserDialog  *self,
                      gboolean                   show_hidden,
                      GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetShowHidden: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 _B (show_hidden));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetShowHidden: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_show_hidden (GTK_FILE_CHOOSER (self->priv->dialog), show_hidden);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetShowHidden: dialog '%s' ('%s') now has show-hidden '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            _B (show_hidden));
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

/**
 * sfcd_get_show_hidden:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Gets whether hidden files and folders are displayed in the dialog; See
 * sfcd_set_show_hidden() for more information.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_get_show_hidden() in the GTK+ API. Do remember to check
 * if @error is set after running this method. If set, the return value is
 * undefined.
 *
 * Return value: %TRUE if hidden files and folders are displayed
 *
 * Since: 0.3
 **/
gboolean
sfcd_get_show_hidden (SandboxFileChooserDialog *self,
                     GError                  **error)
{
  gboolean result = FALSE;

  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), result);

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetShowHidden: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    result = gtk_file_chooser_get_show_hidden (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetShowHidden: dialog '%s' ('%s') has show-hidden '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            _B (result));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return result;
}

/**
 * sfcd_set_do_overwrite_confirmation:
 * @dialog: a #SandboxFileChooserDialog
 * @do_overwrite_confirmation: %TRUE to confirm overwriting in save mode
 * @error: a placeholder for a #GError
 * 
 * Sets whether a dialog in %GTK_FILE_CHOOSER_ACTION_SAVE mode will present a
 * confirmation dialog if the user types a file name that already exists. This
 * is %FALSE by default.
 *
 * If set to %TRUE, the @dialog will emit the
 * #GtkFileChooser::confirm-overwrite signal when appropriate.
 *
 * If all you need is the stock confirmation dialog, set this property to %TRUE.
 * You can override the way confirmation is done by actually handling the
 * #GtkFileChooser::confirm-overwrite signal; please refer to its documentation
 * for the details. Please mind that you are only granted access to the actual
 * filename or uri currently selected by the user. You need to use this API to
 * obtain a new filename or uri that does not override an existing file (see
 * sfcd_NOT_IMPLEMENTED_YET(). TODO
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_set_do_overwrite_confirmation() in the GTK+ API. Do remember
 * to check if @error is set after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_set_do_overwrite_confirmation (SandboxFileChooserDialog  *self,
                                    gboolean                   do_overwrite_confirmation,
                                    GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetDoOverwriteConfirmation: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 _B (do_overwrite_confirmation));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetDoOverwriteConfirmation: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (self->priv->dialog), do_overwrite_confirmation);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetDoOverwriteConfirmation: dialog '%s' ('%s') now has show-hidden '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            _B (do_overwrite_confirmation));
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

/**
 * sfcd_get_do_overwrite_confirmation:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Gets whether a dialog is set to confirm for overwriting when the user types
 * a file name that already exists; See sfcd_set_do_overwrite_confirmation() for
 * more information.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_get_do_overwrite_confirmation() in the GTK+ API. Do remember
 * to check if @error is set after running this method. If set, the return value
 * is undefined.
 *
 * Return value: %TRUE if the dialog will present a confirmation dialog;
 * %FALSE otherwise
 *
 * Since: 0.3
 **/
gboolean
sfcd_get_do_overwrite_confirmation (SandboxFileChooserDialog  *self,
                                    GError                   **error)
{
  gboolean result = FALSE;

  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), result);

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetDoOverwriteConfirmation: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    result = gtk_file_chooser_get_do_overwrite_confirmation (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetDoOverwriteConfirmation: dialog '%s' ('%s') has show-hidden '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            _B (result));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return result;
}

/**
 * sfcd_set_create_folders:
 * @dialog: a #SandboxFileChooserDialog
 * @create_folders: %TRUE if the Create Folder button should be displayed
 * @error: a placeholder for a #GError
 * 
 * Sets whether the dialog will offer to create new folders. This is only 
 * relevant if the action is not set to be %GTK_FILE_CHOOSER_ACTION_OPEN.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_set_create_folders() in the GTK+ API. Do remember to check
 * if @error is set after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_set_create_folders (SandboxFileChooserDialog  *self,
                         gboolean                   create_folders,
                         GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetCreateFolders: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 _B (create_folders));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetCreateFolders: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER (self->priv->dialog), create_folders);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetCreateFolders: dialog '%s' ('%s') now has show-hidden '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            _B (create_folders));
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

/**
 * sfcd_get_create_folders:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Gets whether the dialog will offer to create new folders; See
 * sfcd_set_create_folders() for more information.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_get_create_folders() in the GTK+ API. Do remember to check
 * if @error is set after running this method. If set, the return value is
 * undefined.
 *
 * Return value: %TRUE if the Create Folder button should be displayed
 *
 * Since: 0.3
 **/
gboolean
sfcd_get_create_folders (SandboxFileChooserDialog  *self,
                         GError                   **error)
{
  gboolean result = FALSE;

  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), result);

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetCreateFolders: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    result = gtk_file_chooser_get_create_folders (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetCreateFolders: dialog '%s' ('%s') has show-hidden '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            _B (result));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return result;
}

/**
 * sfcd_set_current_name:
 * @dialog: a #SandboxFileChooserDialog
 * @name: (type filename): the filename to use, as a UTF-8 string
 * @error: a placeholder for a #GError
 * 
 * Sets the current name in the dialog, as if entered by the user. Note that the
 * name passed in here is a UTF-8 string rather than a filename. This function
 * is meant for such uses as a suggested name in a "Save As..." dialog. You can
 * pass "Untitled.odt" or a similarly suitable suggestion for the @name.
 *
 * If you want to preselect a particular existing file, you should use
 * sfcd_set_filename() or sfcd_set_uri() instead.
 * Please see the documentation for those functions for an example of using
 * sfcd_set_current_name() as well.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_set_current_name() in the GTK+ API. Do remember to check
 * if @error is set after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_set_current_name (SandboxFileChooserDialog  *self,
                       const gchar               *name,
                       GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetCurrentName: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 name);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetCurrentName: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (self->priv->dialog), name);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetCurrentName: dialog '%s' ('%s')'s typed name is now '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            name);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

/**
 * sfcd_set_filename:
 * @dialog: a #SandboxFileChooserDialog
 * @filename: (type filename): the filename to set as current
 * @error: a placeholder for a #GError
 * 
 * Sets @filename as the current filename for the dialog, by changing to
 * the file's parent folder and actually selecting the file in list; all other
 * files will be unselected.  If the @dialog is in
 * %GTK_FILE_CHOOSER_ACTION_SAVE mode, the file's base name will also appear in
 * the dialog's file name entry.
 *
 * Note that the file must exist, or nothing will be done except for the
 * directory change.
 *
 * You should use this function only when implementing a <guimenuitem>File/Save
 * As...</guimenuitem> dialog for which you already have a file name to which
 * the user may save.  For example, when the user opens an existing file and
 * then does <guimenuitem>Save As...</guimenuitem> on it to save a copy or
 * a modified version.  If you don't have a file name already &mdash; for
 * example, if the user just created a new file and is saving it for the first
 * time, do not call this function. Instead, use something similar to this:
 * |[
 * if (document_is_new)
 *   {
 *     /&ast; the user just created a new document &ast;/
 *     sfcd_set_current_name (chooser, "Untitled document");
 *   }
 * else
 *   {
 *     /&ast; the user edited an existing document &ast;/ 
 *     sfcd_set_filename (chooser, existing_filename);
 *   }
 * ]|
 *
 * In the first case, the dialog will present the user with useful suggestions
 * as to where to save his new file. In the second case, the file's existing
 * location is already known, so the dialog will use it.
 * 
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_set_filename() in the GTK+ API. Do remember to check
 * if @error is set after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_set_filename (SandboxFileChooserDialog  *self,
                   const gchar               *filename,
                   GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetFilename: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 filename);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetFilename: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (self->priv->dialog), filename);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetFilename: dialog '%s' ('%s')'s current file name now is '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            filename);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

/**
 * sfcd_set_current_folder:
 * @dialog: a #SandboxFileChooserDialog
 * @filename: (type filename): the full path of the new current folder
 * @error: a placeholder for a #GError
 * 
 * Sets the current folder for @dialog from a local filename. The user will be
 * shown the full contents of the current folder, plus user interface elements
 * for navigating to other folders.
 *
 * In general, you should not use this function. You should only cause the dialog
 * to show a specific folder when it is appropriate to use sfcd_set_filename(),
 * i.e. when you are doing a <guimenuitem>Save As...</guimenuitem> command
 * and you already have a file saved somewhere.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_set_current_folder() in the GTK+ API. Do remember to check
 * if @error is set after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_set_current_folder (SandboxFileChooserDialog  *self,
                         const gchar               *filename,
                         GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetCurrentFolder: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 filename);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetCurrentFolder: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (self->priv->dialog), filename);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetCurrentFolder: dialog '%s' ('%s')'s current folder now is '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            filename);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

/**
 * sfcd_set_uri:
 * @dialog: a #SandboxFileChooserDialog
 * @uri: the URI to set as current
 * @error: a placeholder for a #GError
 * 
 * Sets the file referred to by @uri as the current file for the @dialog,
 * by changing to the URI's parent folder and actually selecting the URI in the
 * list. If the @dialog is %GTK_FILE_CHOOSER_ACTION_SAVE mode, the URI's base
 * name will also appear in the dialog's file name entry.
 *
 * Note that the URI must exist, or nothing will be done except for the 
 * directory change.
 *
 * You should use this function only when implementing a <guimenuitem>File/Save
 * As...</guimenuitem> dialog for which you already have a file name to which
 * the user may save.  For example, when the user opens an existing file and
 * then does <guimenuitem>Save As...</guimenuitem> on it to save a copy or a
 * modified version.  If you don't have a file name already &mdash; for example,
 * if the user just created a new file and is saving it for the first time, do
 * not call this function. Instead, use something similar to this:
 * |[
 * if (document_is_new)
 *   {
 *     /&ast; the user just created a new document &ast;/
 *     sfcd_set_current_name (chooser, "Untitled document");
 *   }
 * else
 *   {
 *     /&ast; the user edited an existing document &ast;/ 
 *     sfcd_set_filename (chooser, existing_filename);
 *   }
 * ]|
 *
 * In the first case, the dialog will present the user with useful suggestions
 * as to where to save his new file. In the second case, the file's existing
 * location is already known, so the dialog will use it.
 * 
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_set_uri() in the GTK+ API. Do remember to check
 * if @error is set after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_set_uri (SandboxFileChooserDialog  *self,
              const gchar               *uri,
              GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetUri: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 uri);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetUri: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (self->priv->dialog), uri);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetUri: dialog '%s' ('%s')'s current file uri now is '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            uri);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

/**
 * sfcd_set_current_folder_uri:
 * @dialog: a #SandboxFileChooserDialog
 * @uri: the URI for the new current folder
 * @error: a placeholder for a #GError
 * 
 * Sets the current folder for @dialog from an URI. The user will be
 * shown the full contents of the current folder, plus user interface elements
 * for navigating to other folders.
 *
 * In general, you should not use this function. You should only cause the dialog
 * to show a specific folder when it is appropriate to use sfcd_set_uri(),
 * i.e. when you are doing a <guimenuitem>Save As...</guimenuitem> command
 * and you already have a file saved somewhere.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_set_current_folder_uri() in the GTK+ API. Do remember to
 * check if @error is set after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_set_current_folder_uri (SandboxFileChooserDialog  *self,
                             const gchar               *uri,
                             GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.SetCurrentFolderUri: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 uri);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.SetCurrentFolderUri: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (self->priv->dialog), uri);

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.SetCurrentFolderUri: dialog '%s' ('%s')'s current folder uri now is '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            uri);
  }

  g_mutex_unlock (&self->priv->stateMutex);
}

/**
 * sfcd_add_shortcut_folder:
 * @dialog: a #SandboxFileChooserDialog
 * @folder: (type filename): filename of the folder to add
 * @error: a placeholder for a #GError
 * 
 * Adds a folder to be displayed with the shortcut folders in a dialog.
 * Note that shortcut folders do not get saved, as they are provided by the
 * application.  For example, you can use this to add a
 * "/usr/share/mydrawprogram/Clipart" folder to the volume list.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_add_shortcut_folder() in the GTK+ API. Do remember to
 * check if @error is set after running this method.
 * 
 * Return value: %TRUE if the folder could be added successfully, %FALSE
 * otherwise.  In the latter case, the @error will be set as appropriate.
 *
 * Since: 0.3
 **/
gboolean
sfcd_add_shortcut_folder (SandboxFileChooserDialog  *self,
                          const gchar               *folder,
                          GError                   **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.AddShortcutFolder: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 folder);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.AddShortcutFolder: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    succeeded = gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (self->priv->dialog), folder, error);

    if (succeeded)
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.AddShortcutFolder: dialog '%s' ('%s') has been added a shortcut folder named '%s'.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
              folder);
    else
    {
      g_prefix_error (error,
                      "SandboxFileChooserDialog.AddShortcutFolder: dialog '%s' ('%s') did not allow adding a shortcut folder named '%s').\n",
                      self->id,
                      gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                      folder);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
    }
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

/**
 * sfcd_remove_shortcut_folder:
 * @dialog: a #SandboxFileChooserDialog
 * @folder: (type filename): filename of the folder to remove
 * @error: a placeholder for a #GError
 * 
 * Removes a folder from a dialog's list of shortcut folders.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_remove_shortcut_folder() in the GTK+ API. Do remember to
 * check if @error is set after running this method.
 * 
 * Return value: %TRUE if the operation succeeds, %FALSE otherwise.  
 * In the latter case, the @error will be set as appropriate.
 *
 * See also: sfcd_add_shortcut_folder()
 *
 * Since: 0.3
 **/
gboolean
sfcd_remove_shortcut_folder (SandboxFileChooserDialog  *self,
                             const gchar               *folder,
                             GError                   **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.RemoveShortcutFolder: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 folder);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.RemoveShortcutFolder: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    succeeded = gtk_file_chooser_remove_shortcut_folder (GTK_FILE_CHOOSER (self->priv->dialog), folder, error);

    if (succeeded)
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.RemoveShortcutFolder: dialog '%s' ('%s')'s shortcut folder named '%s' has been removed.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
              folder);
    else
    {
      g_prefix_error (error,
                      "SandboxFileChooserDialog.RemoveShortcutFolder: dialog '%s' ('%s') did not allow removing a shortcut folder named '%s').\n",
                      self->id,
                      gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                      folder);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
    }
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

/**
 * sfcd_list_shortcut_folders:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Queries the list of shortcut folders in the dialog, as set by
 * sfcd_add_shortcut_folder().
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * sfcd_list_shortcut_folders() in the GTK+ API. Do remember to check if @error
 * is set after running this method.
 *
 * Return value: (element-type filename) (transfer full): A list of
 * folder filenames, or %NULL if there are no shortcut folders.  Free
 * the returned list with g_slist_free(), and the filenames with
 * g_free().
 *
 * See also: sfcd_add_shortcut_folder()
 *
 * Since: 0.3
 **/
GSList *
sfcd_list_shortcut_folders (SandboxFileChooserDialog   *self,
                            GError                    **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);
  GSList *list = NULL;

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.ListShortcutFolders: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    list = gtk_file_chooser_list_shortcut_folders (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.ListShortcutFolders: dialog '%s' ('%s')'s list of shortcuts contains %u elements.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            (list == NULL? 0:g_slist_length (list)));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return list;
}

/**
 * sfcd_add_shortcut_folder_uri:
 * @dialog: a #SandboxFileChooserDialog
 * @uri: URI of the folder to add
 * @error: a placeholder for a #GError
 * 
 * Adds a folder URI to be displayed with the shortcut folders in a dialog.
 * Note that shortcut folders do not get saved, as they are provided by the
 * application.  For example, you can use this to add a
 * "file:///usr/share/mydrawprogram/Clipart" folder to the volume list.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_add_shortcut_folder_uri() in the GTK+ API. Do remember to
 * check if @error is set after running this method.
 * 
 * Return value: %TRUE if the folder could be added successfully, %FALSE
 * otherwise.  In the latter case, the @error will be set as appropriate.
 *
 * Since: 0.3
 **/
gboolean
sfcd_add_shortcut_folder_uri (SandboxFileChooserDialog  *self,
                              const gchar               *uri,
                              GError                   **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.AddShortcutFolderUri: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 uri);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.AddShortcutFolderUri: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    succeeded = gtk_file_chooser_add_shortcut_folder_uri (GTK_FILE_CHOOSER (self->priv->dialog), uri, error);

    if (succeeded)
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.AddShortcutFolderUri: dialog '%s' ('%s') has been added a shortcut folder named '%s'.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
              uri);
    else
    {
      g_prefix_error (error,
                      "SandboxFileChooserDialog.AddShortcutFolderUri: dialog '%s' ('%s') did not allow adding a shortcut folder named '%s').\n",
                      self->id,
                      gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                      uri);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
    }
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

/**
 * sfcd_remove_shortcut_folder_uri:
 * @dialog: a #SandboxFileChooserDialog
 * @uri: URI of the folder to remove
 * @error: a placeholder for a #GError
 * 
 * Removes a folder URI from a dialog's list of shortcut folders.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_remove_shortcut_folder_uri() in the GTK+ API. Do remember to
 * check if @error is set after running this method.
 * 
 * Return value: %TRUE if the operation succeeds, %FALSE otherwise.  
 * In the latter case, the @error will be set as appropriate.
 *
 * See also: sfcd_add_shortcut_folder_uri()
 * Since: 0.3
 **/
gboolean
sfcd_remove_shortcut_folder_uri (SandboxFileChooserDialog  *self,
                                 const gchar               *uri,
                                 GError                   **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.RemoveShortcutFolderUri: dialog '%s' ('%s') is already running and cannot be modified (parameter was '%s').\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                 uri);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    if (self->priv->state == SFCD_DATA_RETRIEVAL)
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.RemoveShortcutFolderUri: dialog '%s' ('%s') being put back into 'configuration' state.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));
    }

    self->priv->state = SFCD_CONFIGURATION;
    succeeded = gtk_file_chooser_remove_shortcut_folder_uri (GTK_FILE_CHOOSER (self->priv->dialog), uri, error);

    if (succeeded)
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog.RemoveShortcutFolderUri: dialog '%s' ('%s')'s shortcut folder named '%s' has been removed.\n",
              self->id,
              gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
              uri);
    else
    {
      g_prefix_error (error,
                      "SandboxFileChooserDialog.RemoveShortcutFolderUri: dialog '%s' ('%s') did not allow removing a shortcut folder named '%s').\n",
                      self->id,
                      gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
                      uri);

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
    }
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

/**
 * sfcd_list_shortcut_folder_uris:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Queries the list of shortcut folders in the dialog, as set by
 * sfcd_add_shortcut_folder_uri().
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * sfcd_list_shortcut_folder_uris() in the GTK+ API. Do remember to check if
 * @error is set after running this method.
 *
 * Return value: (element-type utf8) (transfer full): A list of folder
 * URIs, or %NULL if there are no shortcut folders.  Free the returned
 * list with g_slist_free(), and the URIs with g_free().
 *
 * See also: sfcd_add_shortcut_folder_uri()
 *
 * Since: 0.3
 **/
GSList *
sfcd_list_shortcut_folder_uris (SandboxFileChooserDialog   *self,
                                GError                    **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);
  GSList *list = NULL;

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.ListShortcutFoldersUri: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    list = gtk_file_chooser_list_shortcut_folder_uris (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.ListShortcutFoldersUri: dialog '%s' ('%s')'s list of shortcuts contains %u elements.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            (list == NULL? 0:g_slist_length (list)));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return list;
}

/**
 * sfcd_get_current_name:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Gets the current name in the @dialog, as entered by the user in the
 * text entry for "Name".
 *
 * This is meant to be used in save dialogs, to get the currently typed filename
 * when the file itself does not exist yet.  For example, an application that
 * adds a custom extra widget to the dialog for "file format" may want to
 * change the extension of the typed filename based on the chosen format, say,
 * from ".jpg" to ".png".
 *
 * FIXME: this cannot be provided and advertised in the same way in SandboxUtils.
 * An API will be provided to complete/modify filenames for file extension to 
 * format matching, but apps can no longer choose arbitrary names.
 *
 * This method belongs to the %SFCD_DATA_RETRIEVAL state. It is equivalent to
 * gtk_file_chooser_get_current_name() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is
 * undefined.
 *
 * Returns: The raw text from the dialog's "Name" entry. Free this with
 * g_free(). Note that this string is not a full pathname or URI; it is
 * whatever the contents of the entry are.  Note also that this string is in
 * UTF-8 encoding, which is not necessarily the system's encoding for filenames.
 *
 * Since: 0.4
 **/
gchar *
sfcd_get_current_name (SandboxFileChooserDialog   *self,
                       GError                    **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), NULL);

  gchar *name = NULL;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_get_state (self) != SFCD_DATA_RETRIEVAL)
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetCurrentName: dialog '%s' ('%s') is being configured or running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    name = gtk_file_chooser_get_current_name (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetCurrentName: dialog '%s' ('%s')'s typed name currently is '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            name);
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return name;
}

/**
 * sfcd_get_filename:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Gets the filename for the currently selected file in
 * the dialog. The filename is returned as an absolute path. If
 * multiple files are selected, one of the filenames will be returned at
 * random.
 *
 * If the dialog is in folder mode, this function returns the selected
 * folder.
 *
 * This method belongs to the %SFCD_DATA_RETRIEVAL state. It is equivalent to
 * gtk_file_chooser_get_filename() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is
 * undefined.
 * 
 * Return value: (type filename): The currently selected filename, or %NULL
 * if no file is selected, or the selected file can't  be represented with a
 * local filename. Free with g_free().
 *
 * Since: 0.4
 **/
gchar *
sfcd_get_filename (SandboxFileChooserDialog   *self,
                   GError                    **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), NULL);

  gchar *name = NULL;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_get_state (self) != SFCD_DATA_RETRIEVAL)
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetFilename: dialog '%s' ('%s') is being configured or running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetFilename: dialog '%s' ('%s')'s current file name is '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            name);
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return name;
}

/**
 * sfcd_get_filenames:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Lists all the selected files and subfolders in the current folder of
 * @dialog. The returned names are full absolute paths. If files in the current
 * folder cannot be represented as local filenames they will be ignored. (See
 * sfcd_get_uris())
 *
 * This method belongs to the %SFCD_DATA_RETRIEVAL state. It is equivalent to
 * gtk_file_chooser_get_filenames() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is
 * undefined.
 *
 * Return value: (element-type filename) (transfer full): a #GSList
 *    containing the filenames of all selected files and subfolders in
 *    the current folder. Free the returned list with g_slist_free(),
 *    and the filenames with g_free().
 *
 * Since: 0.4
 **/
GSList *
sfcd_get_filenames (SandboxFileChooserDialog   *self,
                    GError                    **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);

  GSList *list = NULL;

  if (sfcd_get_state (self) != SFCD_DATA_RETRIEVAL)
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetFilenames: dialog '%s' ('%s') is being configured or running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    list = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetFilenames: dialog '%s' ('%s')'s list of current file names contains %u elements.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            (list == NULL? 0:g_slist_length (list)));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return list;
}

/**
 * sfcd_get_current_folder:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Gets the current folder of @dialog as a local filename.
 * See sfcd_set_current_folder().
 *
 * Note that this is the folder that the dialog is currently displaying
 * (e.g. "/home/username/Documents"), which is <emphasis>not the same</emphasis>
 * as the currently-selected folder if the dialog is in
 * %GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER mode
 * (e.g. "/home/username/Documents/selected-folder/".  To get the
 * currently-selected folder in that mode, use sfcd_get_uri() as the
 * usual way to get the selection.
 *
 * This method belongs to the %SFCD_DATA_RETRIEVAL state. It is equivalent to
 * gtk_file_chooser_get_current_folder() in the GTK+ API. Do remember to check
 * if @error is set after running this method. If set, the return value is
 * undefined.
 * 
 * Return value: (type filename): the full path of the current folder,
 * or %NULL if the current path cannot be represented as a local
 * filename.  Free with g_free().  This function will also return
 * %NULL if the dialog was unable to load the last folder that
 * was requested from it; for example, as would be for calling
 * sfcd_set_current_folder() on a nonexistent folder.
 *
 * Since: 0.4
 **/
gchar *
sfcd_get_current_folder (SandboxFileChooserDialog   *self,
                         GError                    **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);
  
  gchar *folder = NULL;

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetCurrentFolder: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    folder = gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetCurrentFolder: dialog '%s' ('%s')'s current folder is '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            folder);
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return folder;
}

/**
 * sfcd_get_uri:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 *
 * Gets the URI for the currently selected file in the dialog. If multiple files
 * are selected, one of the filenames will be returned at random.
 * 
 * If the dialog is in folder mode, this function returns the selected folder.
 *
 * This method belongs to the %SFCD_DATA_RETRIEVAL state. It is equivalent to
 * gtk_file_chooser_get_uri() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is
 * undefined.
 * 
 * Return value: The currently selected URI, or %NULL
 *  if no file is selected. If sfcd_set_local_only() is set to %TRUE
 * (the default) a local URI will be returned for any FUSE locations.
 * Free with g_free().
 *
 * Since: 0.4
 **/
gchar *
sfcd_get_uri (SandboxFileChooserDialog   *self,
              GError                    **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);
  gchar *uri = NULL;

  if (sfcd_get_state (self) != SFCD_DATA_RETRIEVAL)
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetUri: dialog '%s' ('%s') is being configured or running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetUri: dialog '%s' ('%s')'s current file uri is '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            uri);
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return uri;
}

/**
 * sfcd_get_uri:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Lists all the selected files and subfolders in the current folder of
 * @dialog. The returned names are full absolute URIs.
 *
 * This method belongs to the %SFCD_DATA_RETRIEVAL state. It is equivalent to
 * gtk_file_chooser_get_uris() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is
 * undefined.
 *
 * Return value: (element-type utf8) (transfer full): a #GSList containing the URIs of all selected
 *   files and subfolders in the current folder. Free the returned list
 *   with g_slist_free(), and the filenames with g_free().
 *
 * Since: 0.3
 **/
GSList *
sfcd_get_uris (SandboxFileChooserDialog   *self,
               GError                    **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);
  
  GSList *list = NULL;

  if (sfcd_get_state (self) != SFCD_DATA_RETRIEVAL)
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetUris: dialog '%s' ('%s') is being configured or running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    list = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetUris: dialog '%s' ('%s')'s list of current file uris contains %u elements.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            (list == NULL? 0:g_slist_length (list)));
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return list;
}

/**
 * sfcd_get_current_folder_uri:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 *
 * Gets the current folder of @dialog as an URI.
 * See sfcd_set_current_folder_uri().
 *
 * Note that this is the folder that the dialog is currently displaying
 * (e.g. "file:///home/username/Documents"), which is <emphasis>not the same</emphasis>
 * as the currently-selected folder if the dialog is in
 * %GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER mode
 * (e.g. "file:///home/username/Documents/selected-folder/".  To get the
 * currently-selected folder in that mode, use sfcd_get_uri() as the
 * usual way to get the selection.
 *
 * This method belongs to the %SFCD_DATA_RETRIEVAL state. It is equivalent to
 * gtk_file_chooser_get_current_folder_uri() in the GTK+ API. Do remember to
 * check if @error is set after running this method. If set, the return value is
 * undefined.
 *
 * Return value: the URI for the current folder. Free with g_free().  This
 * function will also return %NULL if the dialog was unable to load the
 * last folder that was requested from it; for example, as would be for calling
 * sfcd_set_current_folder_uri() on a nonexistent folder.
 * 
 *
 * Since: 0.3
 **/
gchar *
sfcd_get_current_folder_uri (SandboxFileChooserDialog   *self,
                             GError                    **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), NULL);

  g_mutex_lock (&self->priv->stateMutex);
  
  gchar *uri = NULL;

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_CHANGE,
                 "SandboxFileChooserDialog.GetCurrentFolderUri: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", g_error_get_message (*error));
  }
  else
  {
    uri = gtk_file_chooser_get_current_folder_uri (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetCurrentFolderUri: dialog '%s' ('%s')'s current folder uri is '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            uri);
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return uri;
}
