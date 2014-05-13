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
 * @stability: Unstable
 * @include: sandboxutils.h
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
 * As of now, it has not yet been decided whether your application will receive
 * file paths or file descriptors or both in the %SFCD_DATA_RETRIEVAL state.
 *
 * Since: 0.3
 **/

//TODO copy extra sections from gtkfilechooser such as gtkfilechooserdialog-setting-up
//FIXME where are the properties? the ctor must handle and dispatch them

#include <glib.h>
#include <gtk/gtk.h>

#include "sandboxfilechooserdialog.h"
#include "sandboxutilsmarshals.h"
#include "localfilechooserdialog.h"
#include "remotefilechooserdialog.h"

G_DEFINE_TYPE (SandboxFileChooserDialog, sfcd, G_TYPE_OBJECT)

/**
 * SandboxFileChooserDialog:SfcdAcceptLabels:
 *
 * An array of button labels that are considered to correspond to an action
 * indicating user acceptance. If you attempt to add a button to a
 * #SandboxFileChooserDialog with a response id among %GTK_RESPONSE_ACCEPT, 
 * %GTK_RESPONSE_OK, %GTK_RESPONSE_YES or %GTK_RESPONSE_APPLY and with a button
 * not contained in %SfcdAcceptLabels, it will be rejected and will not appear.
 * This is to prevent malicious applications from tricking a user into accepting
 * a file selection when they cancel an undesired dialog.
 *
 * See also: sfcd_is_accept_label()
 *
 * Since: 0.4
 */
const gchar *SfcdAcceptLabels[] =
{
  "Accept",
  "Next",
  "Ok",
  "Choose",
  "Confirm",
  "Pick",
  "Yes",
  "Apply",
  "Select",
  "Add",
  "Append",
  "Save",
  "Save as...",
  "Save Copy",
  "Export",
  "Create",
  "Send",
  "Upload",
  "Rename",
  "Write",
  "Merge",
  "Extract",
  "Open",
  "Open as...",
  "Open Copy",
  "Open Read-Only",
  "Import",
  "Print",
  "Read",
  "View",
  "Preview",
  "Load",
  "Download",
  "Play",
  "Enqueue",
  "Attach",
  "Extract",
  "Compare",
  NULL
};

/**
 * sfcd_is_accept_label:
 * @label: a prospective #SandboxFileChooserDialog button label
 * 
 * Verifies whether a button label would be valid for use in a button with a
 * response id suggesting the user accepts a file being selected. This function
 * automatically removes underscores from labels before performing a comparison
 * so you don't need to do it yourself.
 *
 * The %SfcdAcceptLabels array defines labels that are considered acceptable.
 * indicating user acceptance. If you attempt to add a button to a
 * #SandboxFileChooserDialog with a response id among %GTK_RESPONSE_ACCEPT, 
 * %GTK_RESPONSE_OK, %GTK_RESPONSE_YES or %GTK_RESPONSE_APPLY and with a button
 * not contained in %SfcdAcceptLabels, it will be rejected and will not appear.
 * This is to prevent malicious applications from tricking a user into accepting
 * a file selection when they cancel an undesired dialog.
 *
 * The current list of %SfcdAcceptLabels goes as follows:
 * <informalexample><programlisting>
 * const gchar *SfcdAcceptLabels[] =
 * {
 *   "Accept",
 *   "Next",
 *   "Ok",
 *   "Choose",
 *   "Confirm",
 *   "Pick",
 *   "Yes",
 *   "Apply",
 *   "Select",
 *   "Add",
 *   "Append",
 *   "Save",
 *   "Save as...",
 *   "Save Copy",
 *   "Export",
 *   "Create",
 *   "Send",
 *   "Upload",
 *   "Rename",
 *   "Write",
 *   "Merge",
 *   "Extract",
 *   "Open",
 *   "Open as...",
 *   "Open Copy",
 *   "Open Read-Only",
 *   "Import",
 *   "Print",
 *   "Read",
 *   "View",
 *   "Preview",
 *   "Load",
 *   "Download",
 *   "Play",
 *   "Enqueue",
 *   "Attach",
 *   "Extract",
 *   "Compare",
 *   NULL
 * };
 * </programlisting></informalexample>
 *
 * #SfcdAcceptLabels
 *
 * Returns: True if the sanity check succeeds, False if the @dialog or @error
 * were not as expected
 *
 * Since: 0.4
 */
gboolean sfcd_is_accept_label (const gchar *label)
{
  gchar **labelsplit = g_strsplit (label, "_", -1);
  gchar *cleanlabel = g_strjoinv (NULL, labelsplit);
  g_strfreev (labelsplit);

  guint iter = 0;
  while (SfcdAcceptLabels[iter])
  {
    // Should be ok since we receive UTF-8 strings via GDBus
    if (!g_ascii_strcasecmp (SfcdAcceptLabels[iter], cleanlabel))
    {
      g_free (cleanlabel);
      return TRUE;
    }

    ++iter;
  }

  g_free (cleanlabel);
  return FALSE;
}

static void
sfcd_init (SandboxFileChooserDialog *self)
{
}

static void
sfcd_dispose (GObject* object)
{
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
   * SandboxFileChooserDialog::destroy:
   * @dialog: the dialog on which the signal is emitted
   *
   * Signals that all holders of a reference to the @dialog should release
   * the reference that they hold. May result in finalization of the widget
   * if all references are released.
   */
  klass->destroy_signal =
    g_signal_new ("destroy",
                  G_TYPE_FROM_CLASS (g_object_class),
                  G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
	                0,
                  NULL, NULL,
                  sandboxutils_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * SandboxFileChooserDialog::hide:
   * @dialog: the dialog on which the signal is emitted
   *
   * The ::hide signal is emitted when the @dialog is hidden, for example when
   * the user (or the window manager) minimizes your application's window.
   */
  klass->hide_signal  =
    g_signal_new ("hide",
		              G_OBJECT_CLASS_TYPE (g_object_class),
		              G_SIGNAL_RUN_FIRST,
		              0,
		              NULL, NULL,
                  sandboxutils_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

  /**
   * SandboxFileChooserDialog::response:
   * @dialog: the dialog on which the signal is emitted
   * @response_id: the response id obtained when the dialog finished running
   * @return_state: the state of the dialog after running
   *
   * Emitted when an action widget is clicked, the dialog receives a
   * delete event, or the application programmer calls gtk_dialog_response().
   * On a delete event, the response ID is %GTK_RESPONSE_DELETE_EVENT.
   * Otherwise, it depends on which action widget was clicked.
   *
   * Note that the state of the @dialog will depend on whether the response id
   * translates as the user accepting an operation to take place or not. The
   * GTK+ API defines %GTK_RESPONSE_ACCEPT, %GTK_RESPONSE_OK, %GTK_RESPONSE_YES
   * and %GTK_RESPONSE_ACCEPT as meaning user acceptance. In Sandbox Utils, these
   * response ids can only be used for buttons whose label is included into the
   * acceptance-meaning labels.
   */
  klass->response_signal  =
    g_signal_new ("response",
		  G_OBJECT_CLASS_TYPE (g_object_class),
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
      sandboxutils_marshal_VOID__INT_INT,
		  G_TYPE_NONE, 2,
		  G_TYPE_INT,
		  G_TYPE_INT);

  /**
   * SandboxFileChooserDialog::show:
   * @dialog: the dialog on which the signal is emitted
   *
   * The ::show signal is emitted when the @dialog is shown, for example when
   * the user opens your application's window after minimizing it.
   */
  klass->show_signal  =
    g_signal_new ("show",
		  G_OBJECT_CLASS_TYPE (g_object_class),
		  G_SIGNAL_RUN_FIRST,
      0,
		  NULL, NULL,
      sandboxutils_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  /* Hook finalization functions */
  g_object_class->dispose = sfcd_dispose; /* instance destructor, reverse of init */
  g_object_class->finalize = sfcd_finalize; /* class finalization, reverse of class init */
}

/**
 * sfcd_new:
 * @title: (allow-none): Title of the dialog, or %NULL
 * @parent: (allow-none): Transient parent of the dialog, or %NULL
 * @action: Open or save mode for the dialog (see #GtkFileChooserAction)
 * @first_button_text: (allow-none): stock ID or text to go in the first button, or %NULL
 * @...: response ID for the first button, then additional (button, id) pairs, ending with %NULL
 *
 * Creates a new #SandboxFileChooserDialog. Depending on the settings of Sandbox
 * Utils (see sandboxutils_init()), this may either spawn a locally-owned
 * GTK+ dialog or a handle to a remotely-owned dialog controlled over D-Bus.
 * 
 * This is equivalent to gtk_file_chooser_dialog_new() in the GTK+ API.
 *
 * Return value: a new #SandboxFileChooserDialog
 *
 * Since: 0.5
 **/
SandboxFileChooserDialog *
sfcd_new (const gchar *title,
          GtkWindow *parent,
          GtkFileChooserAction action,
          const gchar *first_button_text,
          ...)
{
  SandboxFileChooserDialog *sfcd = NULL;

  va_list varargs;
  va_start(varargs, first_button_text);

  if (sandboxutils_get_sandboxed ())
    sfcd = rfcd_new_valist (title, parent, action, first_button_text, varargs);
  else
    sfcd = lfcd_new_valist (title, NULL, parent, action, first_button_text, varargs);

  va_end(varargs);

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

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->destroy (self);
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
  
  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_state (self);
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_state_printable (self);
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_dialog_title (self);
}

/**
 * sfcd_is_running:
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->is_running (self);
}

/**
 * sfcd_get_id:
 * @dialog: a #SandboxFileChooserDialog
 * 
 * Gets the identifier of the @dialog. If @dialog is a #LocalFileChooserDialog,
 * the identifier typically a unique string produced by the local process. If
 * @dialog is a #RemoteFileChooserDialog, the identifier is produced by the
 * remote process actually providing the dialog.
 *
 * This method can be called from any #SfcdState. It has no GTK+ equivalent.
 * 
 * Returns: the identifier of @dialog. Do not free.
 *
 * Since: 0.5
 **/
const gchar *
sfcd_get_id (SandboxFileChooserDialog *self)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), NULL);

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_id (self);
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

/**
 * sfcd_set_destroy_with_parent:
 * @dialog: a #SandboxFileChooserDialog
 * @setting: whether to destroy @window with its transient parent
 * 
 * If @setting is %TRUE, then destroying the transient parent of @window
 * will also destroy @window itself. This is useful for dialogs that
 * shouldn't persist beyond the lifetime of the main window they're
 * associated with, for example.
 *
 * This method can be called from any #SfcdState. It is equivalent to
 * gtk_window_set_destroy_with_parent() in the GTK+ API.
 *
 * Since: 0.5
 **/
void
sfcd_set_destroy_with_parent (SandboxFileChooserDialog  *self,
                              gboolean                   setting)
{
  g_return_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self));

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->set_destroy_with_parent (self, setting);
}

/**
 * sfcd_get_destroy_with_parent:
 * @dialog: a #SandboxFileChooserDialog
 *
 * Returns whether the window will be destroyed with its transient parent; See
 * sfcd_set_destroy_with_parent() for more information.
 *
 * This method can be called from any #SfcdState. It is equivalent to
 * gtk_window_get_destroy_with_parent() in the GTK+ API.
 *
 * Return value: %TRUE if the window will be destroyed with its transient parent.
 *
 * Since: 0.5
 **/
gboolean
sfcd_get_destroy_with_parent (SandboxFileChooserDialog *self)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_destroy_with_parent (self);
}

/* RUNNING METHODS */
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
 * With Sandbox Utils, you would instead do the following:
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

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->run (self, error);
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

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->present (self, error);
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
 * @response_id will be set to %GTK_RESPONSE_NONE, which you should ignore in
 * the callbacks you connected to this signal. the @dialog will be back into a
 * %SFCD_CONFIGURATION #SfcdState.
 *
 * After being cancelled, the @dialog still exists. If you no longer need it,
 * destroy it with sfcd_destroy(). Note that you can also directly destroy the
 * dialog while running.
 *
 * This method belongs to the %SFCD_RUNNING state. It is equivalent to
 * gtk_widget_hide() in the GTK+ API. Do remember to check if @error is set 
 * after running this method.
 *
 * Since: 0.3
 **/
void
sfcd_cancel_run (SandboxFileChooserDialog  *self,
                 GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->cancel_run (self, error);
}


/* CONFIGURATION METHODS */
/**
 * sfcd_set_extra_widget:
 * @dialog: a #SandboxFileChooserDialog
 * @widget: the extra #GtkWidget controlled by your application
 * @error: a placeholder for a #GError
 * 
 * Sets an application-supplied widget to provide extra options to the user.
 * Note that this widget does not have access to the internal structure of the
 * #SandboxFileChooserDialog or the underlying file system when sandboxed; it
 * runs with the privileges of your own application.
 *
 * This method can be called from any #SfcdState. It is equivalent to
 * gtk_file_chooser_set_extra_widget() in the GTK+ API. Do remember to check if
 * @error is set after running this method.
 *
 * Since: 0.5
 **/
void
sfcd_set_extra_widget (SandboxFileChooserDialog  *self,
                       GtkWidget                 *widget,
                       GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->set_extra_widget (self, widget, error);
}

/**
 * sfcd_get_extra_widget:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Gets the #GtkWidget that was added to @dialog with sfcd_set_extra_widget().
 *
 * This method can be called from any #SfcdState. It is equivalent to
 * gtk_file_chooser_get_extra_widget() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is undefined.
 *
 * Return value: the extra application-provided widget attached to @dialog
 *
 * Since: 0.5
 **/
GtkWidget *
sfcd_get_extra_widget (SandboxFileChooserDialog *self,
                       GError                  **error)
{
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), NULL);
  
  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_extra_widget (self, error);
}

/**
 * sfcd_select_filename:
 * @dialog: a #SandboxFileChooserDialog
 * @filename: (type filename): the filename to select
 * @error: a placeholder for a #GError
 * 
 * Selects a filename. If the file name isn't in the current
 * folder of @dialog, then the current folder of @dialog will
 * be changed to the folder containing @filename.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_select_filename() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is
 * undefined.
 *
 * See also: sfcd_set_filename()
 *
 * Since: 0.6
 **/
void
sfcd_select_filename (SandboxFileChooserDialog  *self,
                      const gchar               *filename,
                      GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->select_filename (self, filename, error);
}

/**
 * sfcd_unselect_filename:
 * @dialog: a #SandboxFileChooserDialog
 * @filename: (type filename): the filename to unselect
 * @error: a placeholder for a #GError
 * 
 * Unselects a currently selected filename. If the filename
 * is not in the current directory, does not exist, or
 * is otherwise not currently selected, does nothing.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_unselect_filename() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is
 * undefined.
 *
 * Since: 0.6
 **/
void
sfcd_unselect_filename (SandboxFileChooserDialog  *self,
                        const gchar               *filename,
                        GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->unselect_filename (self, filename, error);
}

/**
 * sfcd_select_all:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Selects all the files in the current folder of a dialog.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_select_all() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is
 * undefined.
 *
 * Since: 0.6
 **/
void
sfcd_select_all (SandboxFileChooserDialog  *self,
                 GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->select_all (self, error);
}

/**
 * sfcd_unselect_all:
 * @dialog: a #SandboxFileChooserDialog
 * @error: a placeholder for a #GError
 * 
 * Unselects all the files in the current folder of a dialog.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_unselect_all() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is
 * undefined.
 *
 * Since: 0.6
 **/
void
sfcd_unselect_all (SandboxFileChooserDialog  *self,
                   GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->unselect_all (self, error);
}

/**
 * sfcd_select_uri:
 * @dialog: a #SandboxFileChooserDialog
 * @uri: the URI to select
 * @error: a placeholder for a #GError
 * 
 * Selects the file to by @uri. If the URI doesn't refer to a
 * file in the current folder of @dialog, then the current folder of
 * @dialog will be changed to the folder containing @filename.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_select_uri() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is
 * undefined.
 *
 * Since: 0.6
 **/
void
sfcd_select_uri (SandboxFileChooserDialog  *self,
                 const gchar               *uri,
                 GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->select_uri (self, uri, error);
}

/**
 * sfcd_unselect_uri:
 * @dialog: a #SandboxFileChooserDialog
 * @uri: the URI to unselect
 * @error: a placeholder for a #GError
 * 
 * Unselects the file referred to by @uri. If the file
 * is not in the current directory, does not exist, or
 * is otherwise not currently selected, does nothing.
 *
 * This method belongs to the %SFCD_CONFIGURATION state. It is equivalent to
 * gtk_file_chooser_unselect_uri() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is
 * undefined.
 *
 * Since: 0.6
 **/
void
sfcd_unselect_uri (SandboxFileChooserDialog  *self,
                   const gchar               *uri,
                   GError                   **error)
{
  g_return_if_fail (_sfcd_entry_sanity_check (self, error));

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->unselect_uri (self, uri, error);
}

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

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->set_action (self, action, error);
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
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), GTK_FILE_CHOOSER_ACTION_OPEN);
  
  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_action (self, error);
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
  
  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->set_local_only (self, local_only, error);
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
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), FALSE);
    
  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_local_only (self, error);
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
  
  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->set_select_multiple (self, select_multiple, error);
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
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), FALSE);

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_select_multiple (self, error);
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

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->set_show_hidden (self, show_hidden, error);
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
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), FALSE);
  
  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_show_hidden (self, error);
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
 * If set to %TRUE, the @dialog will show a stock confirmation dialog. There is
 * currently no way to override this dialog, though customization will be
 * allowed in the future, including proposing an alternative name based on the
 * autocompletion plugin.
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
  
  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->set_do_overwrite_confirmation (self, do_overwrite_confirmation, error);
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
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), FALSE);
  
  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_do_overwrite_confirmation (self, error);
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

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->set_create_folders (self, create_folders, error);
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
  g_return_val_if_fail (_sfcd_entry_sanity_check (self, error), FALSE);
  
  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_create_folders (self, error);
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

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->set_current_name (self, name, error);
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

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->set_filename (self, filename, error);
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

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->set_current_folder (self, filename, error);
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

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->set_uri (self, uri, error);
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

  SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->set_current_folder_uri (self, uri, error);
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->add_shortcut_folder (self, folder, error);
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->remove_shortcut_folder (self, folder, error);
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->list_shortcut_folders (self, error);
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->add_shortcut_folder_uri (self, uri, error);
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->remove_shortcut_folder_uri (self, uri, error);
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->list_shortcut_folder_uris (self, error);
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
 * This method belongs to the %SFCD_DATA_RETRIEVAL state. It is equivalent to
 * gtk_file_chooser_get_current_name() in the GTK+ API. Do remember to check if
 * @error is set after running this method. If set, the return value is
 * undefined.
 *
 * <note><para>This method should not be used as it was in GTK+ to pick the typed name
 * and complete or modify their extension. App can no longer choose arbitrary
 * names in the context of sandboxing. A specific API for automatic filename
 * completion will be provided in the future</para></note>
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_current_name (self, error);
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_filename (self, error);
}

/**
 * sfcd_get_uris:
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_filenames (self, error);
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_current_folder (self, error);
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_uri (self, error);
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_uris (self, error);
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

  return SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS (self)->get_current_folder_uri (self, error);
}
