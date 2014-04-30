/*
 * sandboxfilechooserdialog.c: file chooser dialog for sandboxed apps
 *
 * Copyright (C) 2014 Steve Dodier-Lazaro <sidnioulz@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
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
 * It differs with other dialogs in that it has a #SfcdState among:
 * 
 * @SFCD_CONFIGURATION: initial state, in which the dialog's behavior, current
 *   file and directory, etc. can be modified to match your application's needs
 * @SFCD_RUNNING: switched to by calling #sfcd_run, in which the dialog cannot
 *   be modified or queried and can only be cancelled (see #sfcd_cancel_run),
 *   presented (#sfcd_present) or destroyed (#sfcd_destroy)
 * @SFCD_DATA_RETRIEVAL: retrieval: switched to automatically after a successful
 *   run, in which the dialog cannot be modified but the file(s) currently
 *   selected can be queried as in the original #GtkFileChooserDialog
 *
 * States allow preventing attacks where an app would modify the dialog after
 * the user made a selection and before they clicked on the dialog's appropriate
 * button. If the dialog is modified or reused, it switches back to a previous 
 * state in a rather conservative way, both to provide consistency on the
 * restrictions it imposes and to allow some level of object reuse.
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
 * Since: 0.3
 */

#include <glib.h>
#include <gtk/gtk.h>
#include <syslog.h>

#include "sandboxfilechooserdialog.h"
#include "sandboxutilsmarshals.h"

struct _SandboxFileChooserDialogPrivate
{
  GtkWidget             *dialog;      /* pointer to the #GtkFileChooserDialog */
  SfcdState              state;       /* state of the instance */
  GMutex                 stateMutex;  /* a mutex to provide thread-safety */
};


G_DEFINE_TYPE_WITH_PRIVATE (SandboxFileChooserDialog, sfcd, G_TYPE_OBJECT)

/*
 * TODO refactor this list e.g. setters/getters for config, so that it's clear
 * and easy to explain it
 * TODO document functions a la GNOME, and add custom doc for state display
 ***
 * Generic Methods:
 * Create -- initialises state to CONFIGURATION
 * Destroy -- object no longer usable
 * 
 ***
 * RUNNING Methods:
 * Run -- switches state from * to RUNNING
 * Present -- fails if not RUNNING
 * CancelRun -- switches state from RUNNING to CONFIGURATION
 * 
 * RUNNING Signals:
 * RunDone -- only occurs when running finishes, indicates state switches
 * 
 ***
 * CONFIGURATION Methods:
 * SetAction -- fails if RUNNING, switches state from DATA_RETRIEVAL to CONFIGURATION
 * GetAction -- fails if RUNNING
 * ...TODO 
 *
 */
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
 * with it. Fails if @dialog is %NULL. Free with #sfcd_destroy.
 *
 * Since: 0.3
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
 * @self: a #SandboxFileChooserDialog
 * 
 * Destroys the given instance of #SandboxFileChooserDialog. Internally, this
 * method only removes one reference to the dialog, so it may continue existing
 * if you still have other references to it.
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

SfcdState
sfcd_get_state (SandboxFileChooserDialog *self)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), SFCD_WRONG_STATE);

  return self->priv->state;
}

const gchar *
sfcd_get_state_printable  (SandboxFileChooserDialog *self)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), SfcdStatePrintable[SFCD_WRONG_STATE]);

  return SfcdStatePrintable [self->priv->state];
}

const gchar *
sfcd_get_dialog_title (SandboxFileChooserDialog *self)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), NULL);
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_DIALOG (self->priv->dialog), NULL);

  return gtk_window_get_title (GTK_WINDOW (self->priv->dialog));
}

gboolean
sfcd_is_running (SandboxFileChooserDialog *self)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);

  return self->priv->state == SFCD_RUNNING;
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


gboolean
sfcd_run (SandboxFileChooserDialog *self,
          GError                  **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

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

    syslog (LOG_WARNING, "%s", (*error)->message);
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
    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_present (SandboxFileChooserDialog  *self,
              GError                   **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (!sfcd_is_running (self))
  {
    g_set_error (error,
                g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                SFCD_ERROR_FORBIDDEN_CHANGE,
                "SandboxFileChooserDialog.Present: dialog '%s' ('%s') is not running and cannot be presented.\n",
                self->id,
                gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

    syslog (LOG_WARNING, "%s", (*error)->message);
  }
  else
  {
    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.Present: dialog '%s' ('%s') being presented.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

    gtk_window_present (GTK_WINDOW (self->priv->dialog));
    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_cancel_run (SandboxFileChooserDialog  *self,
                 GError                   **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (!sfcd_is_running (self))
  {
    g_set_error (error,
                g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                SFCD_ERROR_FORBIDDEN_CHANGE,
                "SandboxFileChooserDialog.CancelRun: dialog '%s' ('%s') is not running and cannot be cancelled.\n",
                self->id,
                gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

    syslog (LOG_WARNING, "%s", (*error)->message);
  }
  else
  {
    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.CancelRun: dialog '%s' ('%s') is about to be cancelled.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

    // This is enough to cause the run method to issue a GTK_RESPONSE_NONE
    gtk_widget_hide (self->priv->dialog);  
    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_set_action (SandboxFileChooserDialog  *self,
                 gint32                     action,
                 GError                   **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

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

      syslog (LOG_WARNING, "%s", (*error)->message);
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

    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_get_action (SandboxFileChooserDialog *self,
                 gint32                   *result,
                 GError                  **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (result != NULL, FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetAction: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", (*error)->message);
  }
  else
  {
    *result = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetAction: dialog '%s' ('%s') has action '%d'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            *result);

    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_set_local_only (SandboxFileChooserDialog  *self,
                     gboolean                   local_only,
                     GError                   **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

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

      syslog (LOG_WARNING, "%s", (*error)->message);
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

    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_get_local_only (SandboxFileChooserDialog *self,
                    gboolean                 *result,
                    GError                  **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (result != NULL, FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetLocalOnly: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", (*error)->message);
  }
  else
  {
    *result = gtk_file_chooser_get_local_only (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetLocalOnly: dialog '%s' ('%s') has local-only '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            _B (*result));

    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_set_select_multiple (SandboxFileChooserDialog  *self,
                     gboolean                   select_multiple,
                     GError                   **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

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

      syslog (LOG_WARNING, "%s", (*error)->message);
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

    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_get_select_multiple (SandboxFileChooserDialog *self,
                    gboolean                 *result,
                    GError                  **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (result != NULL, FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetSelectMultiple: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", (*error)->message);
  }
  else
  {
    *result = gtk_file_chooser_get_select_multiple (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetSelectMultiple: dialog '%s' ('%s') has select-multiple '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            _B (*result));

    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_set_show_hidden (SandboxFileChooserDialog  *self,
                      gboolean                   show_hidden,
                      GError                   **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

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

      syslog (LOG_WARNING, "%s", (*error)->message);
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

    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_get_show_hidden (SandboxFileChooserDialog *self,
                     gboolean                 *result,
                     GError                  **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (result != NULL, FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

  g_mutex_lock (&self->priv->stateMutex);

  if (sfcd_is_running (self))
  {
    g_set_error (error,
                 g_quark_from_static_string (SFCD_ERROR_DOMAIN),
                 SFCD_ERROR_FORBIDDEN_QUERY,
                 "SandboxFileChooserDialog.GetShowHidden: dialog '%s' ('%s') is already running and cannot be queried.\n",
                 self->id,
                 gtk_window_get_title (GTK_WINDOW (self->priv->dialog)));

      syslog (LOG_WARNING, "%s", (*error)->message);
  }
  else
  {
    *result = gtk_file_chooser_get_show_hidden (GTK_FILE_CHOOSER (self->priv->dialog));

    syslog (LOG_DEBUG,
            "SandboxFileChooserDialog.GetShowHidden: dialog '%s' ('%s') has show-hidden '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            _B (*result));

    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}
gboolean
sfcd_set_current_name (SandboxFileChooserDialog  *self,
                       const gchar               *name,
                       GError                   **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

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

      syslog (LOG_WARNING, "%s", (*error)->message);
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
            "SandboxFileChooserDialog.SetCurrentName: dialog '%s' ('%s')'s currently typed name is now '%s'.\n",
            self->id,
            gtk_window_get_title (GTK_WINDOW (self->priv->dialog)),
            name);

    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_set_filename (SandboxFileChooserDialog  *self,
                   const gchar               *filename,
                   GError                   **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

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

      syslog (LOG_WARNING, "%s", (*error)->message);
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

    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_set_current_folder (SandboxFileChooserDialog  *self,
                         const gchar               *filename,
                         GError                   **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

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

      syslog (LOG_WARNING, "%s", (*error)->message);
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

    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_set_uri (SandboxFileChooserDialog  *self,
              const gchar               *uri,
              GError                   **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

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

      syslog (LOG_WARNING, "%s", (*error)->message);
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

    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}

gboolean
sfcd_set_current_folder_uri (SandboxFileChooserDialog  *self,
                             const gchar               *uri,
                             GError                   **error)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), FALSE);
  g_return_val_if_fail (error != NULL, FALSE);

  gboolean succeeded = FALSE;

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

      syslog (LOG_WARNING, "%s", (*error)->message);
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

    succeeded = TRUE;
  }

  g_mutex_unlock (&self->priv->stateMutex);

  return succeeded;
}
