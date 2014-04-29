/* SandboxUtils -- Sandbox File Chooser Dialog definition
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
#include <glib.h>
#include <gtk/gtk.h>
#include <syslog.h>

#include "sandboxfilechooserdialog.h"
#include "sandboxutilsmarshals.h"

//TODO having the dialog modal locally should cause the compositor to handle this dialog
//  as a child of the client

/* TODO doc
 *
 *
 */
struct _SandboxFileChooserDialogPrivate
{
  GtkWidget             *dialog;
  gint                   runSourceId;
  SfcdRunState           state;
  GMutex                 runMutex;
  GMutex                 stateMutex;
};


G_DEFINE_TYPE_WITH_PRIVATE (SandboxFileChooserDialog, sfcd, G_TYPE_OBJECT)

static void
sfcd_init (SandboxFileChooserDialog *self)
{
  self->priv = sfcd_get_instance_private (self);

  self->priv->dialog      = NULL;
  self->priv->runSourceId = 0;
  self->priv->state       = SFCD_CONFIGURATION;

  g_mutex_init (&self->priv->runMutex);
  g_mutex_init (&self->priv->stateMutex);

  self->id = g_strdup_printf ("%p", self);
}

static void
sfcd_dispose (GObject* object)
{
  SandboxFileChooserDialog *self = SANDBOX_FILE_CHOOSER_DIALOG (object);

  g_mutex_clear (&self->priv->runMutex);
  g_mutex_clear (&self->priv->stateMutex);

  //TODO remove reference to dialog, should only be one left (for gtk_widget_destroy)
  //g_object_unref (self->priv->dialog);

  if (self->priv->dialog)
  {
    g_object_unref (self->priv->dialog);
    gtk_widget_destroy (self->priv->dialog);
  }

  if (self->priv->runSourceId)
    g_source_remove (self->priv->runSourceId); //betting on a crash here

  g_free (self->id);

  //FIXME try to g_free self->priv and see what happens

  G_OBJECT_CLASS (sfcd_parent_class)->dispose (object);

  g_free (self);  // Another double-free crash ahead
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


  /* Add private structure -- normally already done by macro? */
  // g_type_class_add_private (klass, sizeof (SandboxFileChooserDialogPrivate));

  /* Hook finalization functions */
  g_object_class->dispose = sfcd_dispose; /* instance destructor, reverse of init */
  g_object_class->finalize = sfcd_finalize; /* class finalization, reverse of class init */
}

SandboxFileChooserDialog *
sfcd_new (GtkWidget *dialog)
{
  g_return_val_if_fail (GTK_IS_FILE_CHOOSER_DIALOG (dialog), NULL);

  SandboxFileChooserDialog *sfcd = g_object_new (SANDBOX_TYPE_FILE_CHOOSER_DIALOG, NULL);
  g_assert (sfcd != NULL);

  sfcd->priv->dialog = dialog;
  g_object_ref_sink (dialog);
  //TODO take ownership of dialog and reference it

  return sfcd;
}

void
sfcd_destroy (SandboxFileChooserDialog *self)
{
  g_return_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self));

  g_object_unref (self);
}

SfcdRunState
sfcd_get_state (SandboxFileChooserDialog *self)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), 0);

  return self->priv->state;
}

const gchar *
sfcd_get_state_printable  (SandboxFileChooserDialog *self)
{
  g_return_val_if_fail (SANDBOX_IS_FILE_CHOOSER_DIALOG (self), 0);

  return SfcdRunStatePrintable [self->priv->state];
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
    g_object_unref (d->sfcd);
//    _sandbox_file_chooser_dialog_destroy (d->sfcd, d->sfcd->id); //FIXME
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
              SfcdRunStatePrintable [SFCD_CONFIGURATION]);

      d->sfcd->priv->state = SFCD_CONFIGURATION;
    }
    else
    {
      syslog (LOG_DEBUG,
              "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') will now switch to '%s' state.\n",
              d->sfcd->id,
              gtk_window_get_title (GTK_WINDOW (d->sfcd->priv->dialog)),
              SfcdRunStatePrintable [SFCD_DATA_RETRIEVAL]);

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
  g_object_unref (d->sfcd->priv->dialog);

  g_free (d); //FIXME move to priv -- code here will look better

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
            SfcdRunStatePrintable [SFCD_RUNNING]);

    // Now running, prevent destruction
    self->priv->state = SFCD_RUNNING;
    g_object_ref (self->priv->dialog);
            
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

    self->priv->runSourceId = gdk_threads_add_idle_full (G_PRIORITY_DEFAULT, _sfcd_run_func, d, NULL);
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
            "SandboxFileChooserDialog.CancelRun: dialog '%s' ('%s') being cancelled.\n",
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
                 gboolean                   action,
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
