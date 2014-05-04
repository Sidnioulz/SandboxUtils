#include <gtk/gtk.h>
#include <sandboxutils/sandboxfilechooserdialog.h>
#include <syslog.h>

SandboxFileChooserDialog *dialog;
GtkWidget *realgtk;






static void
on_open_dialog_response (SandboxFileChooserDialog *dialog,
                         gint                      response_id,
                         gpointer                  user_data)
{
  if (response_id == GTK_RESPONSE_OK)
  {
    GError *error = NULL;
    gchar *filename = sfcd_get_filename (dialog, error);

    // Check for errors - the server might've crashed, or a security policy
    // could be getting in the way.
    if (!error)
    {
      my_app_open_file (filename);
      g_free (filename);
    }
    else
    {
      my_app_report_error (error);
      g_error_free (error);
    }
  }

  sfcd_destroy (dialog);
}

[...]

void
my_open_function ()
{
  SandboxFileChooserDialog *dialog;

  dialog = sfcd_new (TRUE, // use the remote dialog that passes through the sandbox
                     "Open File",
                     parent_window,
                     GTK_FILE_CHOOSER_ACTION_OPEN,
                     _("_Cancel"), GTK_RESPONSE_CANCEL,
                     _("_Open"), GTK_RESPONSE_ACCEPT,
                     NULL);

  // Ask the dialog to tell us when the user picked a file
  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (on_open_dialog_response),
                    user_data);

  GError *error = NULL;
  sfcd_run (dialog, error);

  // Make sure to verify that the dialog is running
  if (error)
  {
    my_app_report_error (error);
    g_error_free (filename);
  }
}








static gboolean
watchdog_func (gpointer data)
{
  sfcd_run (dialog, NULL);

  return FALSE;
}

int main (int argc, char **argv)
{
  gtk_init (&argc, &argv);
  //TODO sandboxutils_init (&argc, &argv);
  openlog (NAME, LOG_PID | LOG_CONS | LOG_PERROR, LOG_USER);
  

  dialog = sfcd_new (TRUE, "Hello World", NULL,
                     GTK_FILE_CHOOSER_ACTION_OPEN,
                     ("_Cancel"),
                     GTK_RESPONSE_CANCEL,
                     ("_Open"),
                     GTK_RESPONSE_ACCEPT,
                     NULL);

  /*
  realgtk = gtk_file_chooser_dialog_new ("Hello World", NULL,
                     GTK_FILE_CHOOSER_ACTION_OPEN,
                     "Open me!", GTK_RESPONSE_REJECT,
                     "Please!", GTK_RESPONSE_OK,
                     "I beg you!", GTK_RESPONSE_ACCEPT,
                     NULL);

  */
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  g_timeout_add_seconds (1, watchdog_func, NULL);
  g_main_loop_run (loop);

 //TODO sandboxutils_clear (&argc, &argv);
  closelog ();
  return 0;
}
