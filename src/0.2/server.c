#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <syslog.h>
#include <gio/gio.h>
#include <glib/gprintf.h>

#ifndef G_OS_UNIX
#error UNIX only (and most likely Linux only)
#endif
#include <gio/gunixfdlist.h>
#include <systemd/sd-daemon.h>
/* For STDOUT_FILENO */
#include <unistd.h>

#include "server.h"

//TODO having the dialog modal locally should cause the compositor to handle this dialog
//  as a child of the client

// TODO manage multiple clients via sub-processes (so a client cannot crash sfad)
// TODO error domain https://developer.gnome.org/gio/2.28/GDBusError.html
// TODO check for apps that use _set/get fn's after run in original GTK API

// FIXME right now stateMutex is useless, in the future unlock dialogsMutex
//       immediately, make sfcd a GObject and use g_object_ref to avoid deletions

//FIXME temp client -- later use a hash table based on client IDs
SandboxFileChooserDialogClient cli;

// GDBus internals
static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node name='"SANDBOXUTILS_PATH"'>"
	  "<interface name='"SFCD_IFACE"'>"
		  "<annotation name='org.freedesktop.DBus.GLib.CSymbol' value='server'/>"
		  "<method name='New'>"
			  "<arg type='s' name='title' direction='in' />"
			  "<arg type='s' name='parent_id' direction='in' />"
			  "<arg type='i' name='action' direction='in' />"
			  "<arg type='a{sv}' name='button_list' direction='in' />"
			  "<arg type='s' name='dialog_id' direction='out' />"
		  "</method>"
		  "<method name='GetState'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
			  "<arg type='i' name='state' direction='out' />"
		  "</method>"
		  "<method name='Destroy'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
		  "</method>"
		  "<method name='Run'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
		  "</method>"
      "<signal name='RunDone'>"
			  "<arg type='s' name='dialog_id' />"
        "<arg type='i' name='return_code' />"
        "<arg type='i' name='state' />"
        "<arg type='b' name='destroyed' />"
      "</signal>"
		  "<method name='Present'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
		  "</method>"
		  "<method name='CancelRun'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
		  "</method>"
		  "<method name='SetAction'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
			  "<arg type='i' name='action' direction='in' />"
		  "</method>"
		  "<method name='GetAction'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
			  "<arg type='i' name='action' direction='out' />"
		  "</method>"
		  "<method name='SetLocalOnly'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
			  "<arg type='b' name='local_only' direction='in' />"
		  "</method>"
		  "<method name='GetLocalOnly'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
			  "<arg type='b' name='local_only' direction='out' />"
		  "</method>"
		  "<method name='SetSelectMultiple'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
			  "<arg type='b' name='select_multiple' direction='in' />"
		  "</method>"
		  "<method name='GetSelectMultiple'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
			  "<arg type='b' name='select_multiple' direction='out' />"
		  "</method>"
		  "<method name='SetShowHidden'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
			  "<arg type='b' name='show_hidden' direction='in' />"
		  "</method>"
		  "<method name='GetShowHidden'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
			  "<arg type='b' name='show_hidden' direction='out' />"
		  "</method>"
	  "</interface>"
  "</node>";
  
  
  
  
  
  

/* CREATION / DELETION METHODS */
gboolean
server_sandbox_file_chooser_dialog_new (GVariant *parameters,
                                        gchar   **dialog_id,
                                        GError  **error)
{
  /*TODO security_hook to:
     - verify that a client can legitimately create a dialog (e.g. has a GUI)
     - implement limits on the number of concurrent dialogs per client
   */

  const gchar  *title;
  const gchar  *parent_id;
  const gint32  action;
  GVariantIter *iter;
  GVariant     *item;

  g_variant_get (parameters, "(&ssia{sv})", &title, &parent_id, &action, &iter);
  
  GtkWidget *dialog = gtk_file_chooser_dialog_new (title, NULL, action, NULL, NULL);

  // Most likely memory limits
	if (dialog==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_CREATION_ERROR,
					"SandboxFileChooserDialog.New: could not allocate memory to create dialog.\n");
		syslog (LOG_CRIT, "%s", (*error)->message);
		
		return FALSE;
	}
	else
	{
    while ((item = g_variant_iter_next_value (iter)))
    {
      const gchar *key;
      GVariant *value;

      g_variant_get (item, "{sv}", &key, &value);
      gtk_dialog_add_button (GTK_DIALOG (dialog), key, g_variant_get_int32 (value));
    }
    
    SandboxFileChooserDialog *sfcd = g_malloc (sizeof(SandboxFileChooserDialog));
    sfcd->dialog = dialog;
    sfcd->state = SANDBOX_FILECHOOSERDIALOG_CONFIGURATION;
    g_mutex_init (&sfcd->runMutex);
    g_mutex_init (&sfcd->stateMutex);
    
	  *dialog_id = g_strdup_printf ("%p", sfcd);
	  gchar *key = g_strdup (*dialog_id);
	  
    g_mutex_lock (&cli.dialogsMutex);
	  g_hash_table_insert (cli.dialogs, key, sfcd);
	  syslog (LOG_DEBUG, "SandboxFileChooserDialog.New: dialog '%s' ('%s') has just been created.\n",
            *dialog_id, title);
    g_mutex_unlock (&cli.dialogsMutex);

	  return TRUE;
	}
}

// Removing a widget from its container or the list of toplevels results in the widget being finalized, unless you’ve added additional references to the widget with g_object_ref().
// FIXME: replace long locking of mutex by a GObject ref on the whole sfcd (or whatever's stored by the hash table in the future)

// FIXME: the code below would then become the _finalize method of my GObject, and its callees would call a _destroy method
static void
_server_sandbox_file_chooser_dialog_destroy (SandboxFileChooserDialog *sfcd, const gchar *dialog_id)
{
  //FIXME style is wrong
  if (g_hash_table_remove (cli.dialogs, dialog_id))
    {
      syslog (LOG_DEBUG, "Destroying GtkFileChooserDialog: %s, titled '%s'\n", dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
            
      //FIXME i really need this semaphore approach to let functions who wait on a lock run or fail before actually destroying the object
      //FIXME otherwise I'm not thread-safe and need to rely on GTK noticing the error or the client being nice
      
      //TODO unlock/join
      g_mutex_clear (&sfcd->runMutex);
      g_mutex_clear (&sfcd->stateMutex);
      gtk_widget_destroy (sfcd->dialog);
      
      //TODO cleanup the sfcd and de-allocate it, too
    }
    else
      syslog (LOG_CRIT, "GtkFileChooserDialog race condition on deletion -- will not free or touch object just in case (%s)\n", dialog_id);
}

//FIXME not entirely thread-safe
gboolean
server_sandbox_file_chooser_dialog_destroy (GVariant *parameters,
                                            GError  **error)
{
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;
  gboolean                    succeeded  = FALSE;

  g_variant_get (parameters, "(&s)", &dialog_id);
  g_mutex_lock (&cli.dialogsMutex);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"SandboxFileChooserDialog.Destroy: dialog '%s' does not exist.\n", dialog_id);
		
		syslog (LOG_WARNING, "%s", (*error)->message);
	}
	else
	{
    //FIXME at this point we might delete the object that has already been looked
    // up elsewhere, but in the future this'll be handled properly: all other calls 
    // will ref the whole sfcd object and we'll only remove the main ref to it and
    // remove it from the table, hence the finalisation will occur only when the 
    // last current call on the sfcd object has finished (as it unrefs the sfcd)

    // Don't destroy directly if running, just tell the run function to do it
    if (sfcd->state == SANDBOX_FILECHOOSERDIALOG_RUNNING)
    {
  		  syslog (LOG_DEBUG, "SandboxFileChooserDialog.Destroy: dialog '%s' ('%s') is still running, will emit a delete event.\n",
                dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
        
        // FIXME there is a very problematic race condition here as the call
        // may be issued between the moment the state changes and the moment
        // gtk_dialog_run actually starts... the only solution is to make it
        // thread-safe. (UPDATE: this might've changed now that I have my own
        // handlers -- still gotta check though).
        
        
        gboolean propagate = FALSE;
        g_signal_emit_by_name (GTK_WIDGET (sfcd->dialog), "delete-event", 0, &propagate);
    }
    else // sfcd->state != SANDBOX_FILECHOOSERDIALOG_RUNNING
    {
  	  _server_sandbox_file_chooser_dialog_destroy  (sfcd, dialog_id);
    }
    
    succeeded = TRUE;
	}

  g_mutex_unlock (&cli.dialogsMutex);
  return succeeded;
}




gboolean
server_sandbox_file_chooser_dialog_get_state (GVariant *parameters,
                                              gint32   *state,
                                              GError  **error)
{
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;
  gboolean                    succeeded  = FALSE;

  g_variant_get (parameters, "(&s)", &dialog_id);
  g_mutex_lock (&cli.dialogsMutex);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
				"SandboxFileChooserDialog.GetState: dialog '%s' does not exist.\n", dialog_id);

	  syslog (LOG_WARNING, "%s", (*error)->message);
	}
	else
	{
    *state = sfcd->state;
    
	  syslog (LOG_DEBUG, "SandboxFileChooserDialog.GetState: dialog '%s' ('%s') has state '%s'.\n",
            dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)), (*state == SANDBOX_FILECHOOSERDIALOG_CONFIGURATION? "'configuration'" : 
                                                                         (*state == SANDBOX_FILECHOOSERDIALOG_RUNNING? "'running'" : 
                                                                                      "'data retrieval'")));
    succeeded = TRUE;
	}
	
  g_mutex_unlock (&cli.dialogsMutex);
  return succeeded;
}





/* RUNNING METHODS */


// Handlers below taken from GtkDialog.c (GTK+ 3.10)
static inline void
shutdown_loop (RunFuncData *d)
{
  syslog (LOG_DEBUG, "SHUTDOWN LOOP\n");
  if (g_main_loop_is_running (d->loop))
    g_main_loop_quit (d->loop);
}

static void
run_unmap_handler (GtkDialog *dialog, gpointer data)
{
  syslog (LOG_DEBUG, "UNMAP HANDLER\n");
  RunFuncData *d = data;

  shutdown_loop (d);
}

static void
run_response_handler (GtkDialog *dialog,
                      gint response_id,
                      gpointer data)
{
  syslog (LOG_DEBUG, "RESPONSE HANDLER\n");
  RunFuncData *d = data;

  d->response_id = response_id;

  shutdown_loop (d);
}

static gint
run_delete_handler (GtkDialog *dialog,
                    GdkEventAny *event,
                    gpointer data)
{
  syslog (LOG_DEBUG, "DELETE HANDLER\n");
  RunFuncData *d = data;
  
  shutdown_loop (d);

  return TRUE; /* Do not destroy */
}

static void
run_destroy_handler (GtkDialog *dialog, gpointer data)
{
  syslog (LOG_DEBUG, "DESTROY HANDLER\n");
  RunFuncData *d = data;
  
  /* shutdown_loop will be called by run_unmap_handler */

  d->destroyed = TRUE;
}





gboolean
_server_sandbox_file_chooser_dialog_run_func (gpointer data)
{
  RunFuncData *d = (RunFuncData *) data;
  
  if(!d) {
		syslog (LOG_CRIT, "%s", "SandboxFileChooserDialog._RunFunc: parameter is NULL, no dialog to run (this should never happen, please report a bug).\n");
    return G_SOURCE_REMOVE;
  }
  
	if (d->sfcd==NULL)
	{
		syslog (LOG_CRIT, "SandboxFileChooserDialog._RunFunc: dialog '%s' does not exist (this should never happen, please report a bug).\n", d->dialog_id);
	}
	else
	{
	  // @@ignore-deprecated@@ TODO: find the correct ignore syntax for these two calls
    gdk_threads_leave ();
    g_main_loop_run (d->loop);
    gdk_threads_enter ();
    
		syslog (LOG_DEBUG, "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') has finished running (return code is %d).\n",
            d->dialog_id, gtk_window_get_title (GTK_WINDOW (d->sfcd->dialog)), d->response_id);
    
	  g_mutex_lock (&cli.dialogsMutex);

    // We want to handle deletion by sending a response signal with a response set
    // to GTK_RESPONSE_DELETE_EVENT, rather than letting Destroy or our own clean-up
    // code touch the dialog. If deletion does occur, then this is a bug.
    if (d->destroyed)
    {
    		syslog (LOG_CRIT, "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') has been destroyed before finished running (this should never happen, please report a bug).\n",
            d->dialog_id, gtk_window_get_title (GTK_WINDOW (d->sfcd->dialog)));
    }
    
    // Closing or destroying the window (either via Destroy or via the WM) results
    // in a GTK_RESPONSE_DELETE_EVENT. In such a case, we destroy the dialog and
    // notify the client process.
    else if (d->response_id == GTK_RESPONSE_DELETE_EVENT)
    {
      g_signal_handler_disconnect (d->sfcd->dialog, d->response_handler);
      g_signal_handler_disconnect (d->sfcd->dialog, d->unmap_handler);
      g_signal_handler_disconnect (d->sfcd->dialog, d->delete_handler);
      g_signal_handler_disconnect (d->sfcd->dialog, d->destroy_handler);
      
	    syslog (LOG_DEBUG, "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') was marked for deletion while running, will be deleted.\n",
              d->dialog_id, gtk_window_get_title (GTK_WINDOW (d->sfcd->dialog)));
              
      g_object_unref (d->sfcd->dialog); //FIXME
      //TODO unref sfcd here
  	  d->destroyed = TRUE;
  	  d->return_state = SANDBOX_FILECHOOSERDIALOG_RUNNING;

  	  _server_sandbox_file_chooser_dialog_destroy (d->sfcd, d->dialog_id);
    }
    else
    {
      g_mutex_lock (&d->sfcd->stateMutex);
      
      // Dialog clean-up
      if (!d->was_modal)
        gtk_window_set_modal (GTK_WINDOW(d->sfcd->dialog), FALSE);

      g_signal_handler_disconnect (d->sfcd->dialog, d->response_handler);
      g_signal_handler_disconnect (d->sfcd->dialog, d->unmap_handler);
      g_signal_handler_disconnect (d->sfcd->dialog, d->delete_handler);
      g_signal_handler_disconnect (d->sfcd->dialog, d->destroy_handler);
      
      // Sanity check on state and then response handling
      if (d->sfcd->state != SANDBOX_FILECHOOSERDIALOG_RUNNING)
      {
    		syslog (LOG_CRIT, "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') is not 'running' after returning from run (this should never happen, please report a bug).\n",
              d->dialog_id, gtk_window_get_title (GTK_WINDOW (d->sfcd->dialog)));
      }
      else
      {
        // According to the doc, could happen if dialog was destroyed. Ours can only be
        // destroyed via a delete-event though, so NONE won't mean dialog destruction
        if (d->response_id == GTK_RESPONSE_NONE)
        {
		      syslog (LOG_DEBUG, "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') ran and returned no response, will return to 'configuration' state.\n",
                  d->dialog_id, gtk_window_get_title (GTK_WINDOW (d->sfcd->dialog)));
          
          d->sfcd->state = SANDBOX_FILECHOOSERDIALOG_CONFIGURATION;
        }
        else
        {
		      syslog (LOG_DEBUG, "SandboxFileChooserDialog._RunFunc: dialog '%s' ('%s') will now switch to 'data retrieval' state.\n",
                  d->dialog_id, gtk_window_get_title (GTK_WINDOW (d->sfcd->dialog)));
          
          d->sfcd->state = SANDBOX_FILECHOOSERDIALOG_DATA_RETRIEVAL;
        }
      }

      // sfcd is not defined if the other branch is explored, yet state is needed
  	  d->return_state = d->sfcd->state;

      // Client can now query and destroy the dialog -- hide it in the meantime
      gtk_widget_hide (d->sfcd->dialog);
      g_mutex_unlock (&d->sfcd->stateMutex);
    
      //TODO unref sfcd here
      g_object_unref (d->sfcd->dialog);
    }
    
    g_mutex_unlock (&cli.dialogsMutex);

    // Free the loop after handlers have been disconnected
    g_main_loop_unref (d->loop);
    d->loop = NULL;
    
    // Telling the client about the outcome, at last
    GError *sigerr = NULL;
    g_dbus_connection_emit_signal (d->connection,
                                   NULL, //sender?
                                   d->object_path,
                                   d->interface_name,
                                   "RunDone",
                                   g_variant_new ("(siib)",
                                                  d->dialog_id,
                                                  d->response_id,
                                                  d->return_state,
                                                  d->destroyed),
                                   &sigerr);

    if (sigerr)
    {
      syslog (LOG_WARNING, "SandboxFileChooserDialog._RunFunc: end-of-run signal emission failed (%s)\n", sigerr->message);
      g_error_free (sigerr);
    }
  }

  g_free (d->sender);
  g_free (d->dialog_id);
  g_free (d->object_path);
  g_free (d->interface_name);
  g_free (d);
  
  return G_SOURCE_REMOVE;
}

gboolean
server_sandbox_file_chooser_dialog_run (GDBusConnection       *connection,
                                        const gchar           *sender,
                                        const gchar           *object_path,
                                        const gchar           *interface_name,
                                        GVariant              *parameters,
                                        GError               **error)
{
  /*TODO security_hook to:
     - manage limits on number of concurrent dialogs
   */
   
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;
  gboolean                    succeeded  = FALSE;

  g_variant_get (parameters, "(&s)", &dialog_id);
  g_mutex_lock (&cli.dialogsMutex);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"SandboxFileChooserDialog.Run: dialog '%s' does not exist.\n", dialog_id);
		
		syslog (LOG_WARNING, "%s", (*error)->message);
	}
	else
	{
    g_mutex_lock (&sfcd->stateMutex);
    
    // It doesn't make sense to call run when the dialog's already running
    if (sfcd->state == SANDBOX_FILECHOOSERDIALOG_RUNNING)
    {
  		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_FORBIDDEN_CHANGE,
					"SandboxFileChooserDialog.Run: dialog '%s' ('%s') is already running.\n",
					dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
		
		  syslog (LOG_WARNING, "%s", (*error)->message);
    }
    else // sfcd->state != SANDBOX_FILECHOOSERDIALOG_RUNNING
    {
      // DATA_RETRIEVAL will be cancelled by this function
      if (sfcd->state == SANDBOX_FILECHOOSERDIALOG_DATA_RETRIEVAL)
      {
  		  syslog (LOG_DEBUG, "SandboxFileChooserDialog.Run: dialog '%s' ('%s') switching from 'data retrieval' to 'running' state.\n",
                dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
      }
      else if (sfcd->state == SANDBOX_FILECHOOSERDIALOG_CONFIGURATION)
      {
  		  syslog (LOG_DEBUG, "SandboxFileChooserDialog.Run: dialog '%s' ('%s') switching from 'configuration' to 'running' state.\n",
                dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
      }

      // Setting state to RUNNING and then running the GTK dialog's code
      sfcd->state = SANDBOX_FILECHOOSERDIALOG_RUNNING;
		  syslog (LOG_DEBUG, "SandboxFileChooserDialog.Run: dialog '%s' ('%s') is about to run.\n",
              dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
              
      // Data shared between Run call and the idle func running the dialog
      RunFuncData *d = g_malloc (sizeof (RunFuncData));
      d->dialog_id = g_strdup (dialog_id);
      d->sfcd = sfcd;
      d->connection = connection;
      d->sender = g_strdup (sender);
      d->object_path = g_strdup (object_path);
      d->interface_name = g_strdup (interface_name);   
      d->loop = g_main_loop_new (NULL, FALSE);
      d->response_id = GTK_RESPONSE_NONE;
      d->destroyed = FALSE;
      d->was_modal = gtk_window_get_modal (GTK_WINDOW (sfcd->dialog));
              
              
      d->response_handler = g_signal_connect (sfcd->dialog,
                                              "response",
                                              G_CALLBACK (run_response_handler),
                                              d);

      d->unmap_handler = g_signal_connect (sfcd->dialog,
                                           "unmap",
                                           G_CALLBACK (run_unmap_handler),
                                           d);

      d->delete_handler = g_signal_connect (sfcd->dialog,
                                            "delete-event",
                                            G_CALLBACK (run_delete_handler),
                                            d);

      d->destroy_handler = g_signal_connect (sfcd->dialog,
                                             "destroy",
                                             G_CALLBACK (run_destroy_handler),
                                             d);
      
      // Setup dialog for running
      g_object_ref (sfcd->dialog); // TODO later will ref the whole sfcd

      if (!d->was_modal)
        gtk_window_set_modal (GTK_WINDOW (sfcd->dialog), TRUE);

      if (!gtk_widget_get_visible (GTK_WIDGET (sfcd->dialog)))
        gtk_widget_show (GTK_WIDGET (sfcd->dialog));

      sfcd->runSourceId = gdk_threads_add_idle_full (G_PRIORITY_DEFAULT, _server_sandbox_file_chooser_dialog_run_func, d, NULL);
      succeeded = TRUE;
    }
      
    g_mutex_unlock (&sfcd->stateMutex);
	}

  g_mutex_unlock (&cli.dialogsMutex);
  return succeeded;
}


/** Final version **/
/* TODO the doc should indicate that race conditions can occur if Present is
 * called too fast after the dialog ran. In such a case, it is not presented.
 */
gboolean
server_sandbox_file_chooser_dialog_present (GVariant *parameters,
                                            GError **error)
{
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;
  gboolean                    succeeded  = FALSE;

  g_variant_get (parameters, "(&s)", &dialog_id);
  g_mutex_lock (&cli.dialogsMutex);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"SandboxFileChooserDialog.Present: dialog '%s' does not exist.\n", dialog_id);
		
		syslog (LOG_WARNING, "%s", (*error)->message);
	}	
	else
	{
	  g_mutex_lock (&sfcd->stateMutex);
    
    // CONFIGURATION and DATA_RETRIEVAL are forbidden states for this function
    if (sfcd->state != SANDBOX_FILECHOOSERDIALOG_RUNNING)
    {
  		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_FORBIDDEN_CHANGE,
					"SandboxFileChooserDialog.Present: dialog '%s' ('%s') is not running and cannot be presented.\n",
					dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
		
		  syslog (LOG_WARNING, "%s", (*error)->message);
    }
    else // sfcd->state == SANDBOX_FILECHOOSERDIALOG_RUNNING
    {
		  syslog (LOG_DEBUG, "SandboxFileChooserDialog.Present: dialog '%s' ('%s') being presented.\n",
              dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
      
      gtk_window_present (GTK_WINDOW (sfcd->dialog));
      succeeded = TRUE;
    }

    g_mutex_unlock (&sfcd->stateMutex);
	}

  g_mutex_unlock (&cli.dialogsMutex);
  return succeeded;
}




/** Final version **/
/* TODO the doc should indicate that race conditions can occur if CancelRun is
 * called too fast after the dialog ran. In such a case, it is not cancelled.
 */
gboolean
server_sandbox_file_chooser_dialog_cancel_run (GVariant *parameters,
                                               GError **error)
{
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;
  gboolean                    succeeded  = FALSE;

  g_variant_get (parameters, "(&s)", &dialog_id);
  g_mutex_lock (&cli.dialogsMutex);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"SandboxFileChooserDialog.CancelRun: dialog '%s' does not exist.\n", dialog_id);
		
		syslog (LOG_WARNING, "%s", (*error)->message);
	}	
	else
	{
	  g_mutex_lock (&sfcd->stateMutex);
    
    // CONFIGURATION and DATA_RETRIEVAL are forbidden states for this function
    if (sfcd->state != SANDBOX_FILECHOOSERDIALOG_RUNNING)
    {
  		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_FORBIDDEN_CHANGE,
					"SandboxFileChooserDialog.CancelRun: dialog '%s' ('%s') is not running.\n",
					dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
		
		  syslog (LOG_WARNING, "%s", (*error)->message);
    }
    else // sfcd->state == SANDBOX_FILECHOOSERDIALOG_RUNNING
    {
		  syslog (LOG_DEBUG, "SandboxFileChooserDialog.CancelRun: dialog '%s' ('%s') being cancelled.\n",
              dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));

      // This is enough to cause the run method to issue a GTK_RESPONSE_NONE
      gtk_widget_hide (sfcd->dialog);  
      succeeded = TRUE;
    }
    
    g_mutex_unlock (&sfcd->stateMutex);
  }
 
  g_mutex_unlock (&cli.dialogsMutex);
  return succeeded;
}




/* CONFIGURATION / MANAGEMENT METHODS  -- USABLE BEFORE RUN */
gboolean
server_sandbox_file_chooser_dialog_set_action (GVariant *parameters,
                                               GError  **error)
{
  const gint32                action;
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;
  gboolean                    succeeded  = FALSE;

  g_variant_get (parameters, "(&si)", &dialog_id, &action);
  g_mutex_lock (&cli.dialogsMutex);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"SandboxFileChooserDialog.SetAction: dialog '%s' does not exist.\n", dialog_id);
		
		syslog (LOG_WARNING, "%s", (*error)->message);
	}
	else
	{
    g_mutex_lock (&sfcd->stateMutex);
    
    // RUNNING is a forbidden state for this function
    if (sfcd->state == SANDBOX_FILECHOOSERDIALOG_RUNNING)
    {
  		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_FORBIDDEN_CHANGE,
					"SandboxFileChooserDialog.SetAction: dialog '%s' ('%s') is already running and cannot be modified (parameter was %d).\n",
					dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)), action);
		
		  syslog (LOG_WARNING, "%s", (*error)->message);
    }
    else // sfcd->state != SANDBOX_FILECHOOSERDIALOG_RUNNING
    {
      // DATA_RETRIEVAL will be cancelled by this function
      if (sfcd->state == SANDBOX_FILECHOOSERDIALOG_DATA_RETRIEVAL)
      {
  		  syslog (LOG_DEBUG, "SandboxFileChooserDialog.SetAction: dialog '%s' ('%s') being put back into 'configuration' state.\n",
                dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
      }

      // Setting state to CONFIGURATION and then running the GTK dialog's code
      sfcd->state = SANDBOX_FILECHOOSERDIALOG_CONFIGURATION;
		  syslog (LOG_DEBUG, "SandboxFileChooserDialog.SetAction: dialog '%s' ('%s') now has action '%d'.\n",
              dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)), action);
      
      gtk_file_chooser_set_action (GTK_FILE_CHOOSER (sfcd->dialog), action);
      succeeded = TRUE;
    }
      
    g_mutex_unlock (&sfcd->stateMutex);
	}

  g_mutex_unlock (&cli.dialogsMutex);
  return succeeded;
}

gboolean
server_sandbox_file_chooser_dialog_get_action (GVariant *parameters,
                                               gint32   *action,
                                               GError  **error)
{
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;
  gboolean                    succeeded  = FALSE;

  g_variant_get (parameters, "(&s)", &dialog_id);
  g_mutex_lock (&cli.dialogsMutex);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
				"SandboxFileChooserDialog.GetAction: dialog '%s' does not exist.\n", dialog_id);

	  syslog (LOG_WARNING, "%s", (*error)->message);
	}
	else
	{
	  g_mutex_lock (&sfcd->stateMutex);
    
    // RUNNING is a forbidden state for this function
    if (sfcd->state == SANDBOX_FILECHOOSERDIALOG_RUNNING)
    {
  		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_FORBIDDEN_QUERY,
					"SandboxFileChooserDialog.GetAction: dialog '%s' ('%s') is already running and cannot be queried.\n",
					dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
		
		  syslog (LOG_WARNING, "%s", (*error)->message);
    }
    else // sfcd->state != SANDBOX_FILECHOOSERDIALOG_RUNNING
    {
      *action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (sfcd->dialog));
		  syslog (LOG_DEBUG, "SandboxFileChooserDialog.GetAction: dialog '%s' ('%s') has action '%d'.\n",
              dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)), *action);
      succeeded = TRUE;
    }
    
    g_mutex_unlock (&sfcd->stateMutex);
	}
	
  g_mutex_unlock (&cli.dialogsMutex);
  return succeeded;
}

gboolean
server_sandbox_file_chooser_dialog_set_local_only (GVariant *parameters,
                                                   GError  **error)
{
  const gboolean              local_only;
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;
  gboolean                    succeeded  = FALSE;

  g_variant_get (parameters, "(&sb)", &dialog_id, &local_only);
  g_mutex_lock (&cli.dialogsMutex);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"SandboxFileChooserDialog.SetLocalOnly: dialog '%s' does not exist.\n", dialog_id);
		
		syslog (LOG_WARNING, "%s", (*error)->message);
	}
	else
	{
    g_mutex_lock (&sfcd->stateMutex);
    
    // RUNNING is a forbidden state for this function
    if (sfcd->state == SANDBOX_FILECHOOSERDIALOG_RUNNING)
    {
  		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_FORBIDDEN_CHANGE,
					"SandboxFileChooserDialog.SetLocalOnly: dialog '%s' ('%s') is already running and cannot be modified (parameter was %s).\n",
					dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)), (local_only?"true":"false"));
		
		  syslog (LOG_WARNING, "%s", (*error)->message);
    }
    else // sfcd->state != SANDBOX_FILECHOOSERDIALOG_RUNNING
    {
      // DATA_RETRIEVAL will be cancelled by this function
      if (sfcd->state == SANDBOX_FILECHOOSERDIALOG_DATA_RETRIEVAL)
      {
  		  syslog (LOG_DEBUG, "SandboxFileChooserDialog.SetLocalOnly: dialog '%s' ('%s') being put back into 'configuration' state.\n",
                dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
      }

      // Setting state to CONFIGURATION and then running the GTK dialog's code
      sfcd->state = SANDBOX_FILECHOOSERDIALOG_CONFIGURATION;
		  syslog (LOG_DEBUG, "SandboxFileChooserDialog.SetLocalOnly: dialog '%s' ('%s') now has local-only set to %s.\n",
              dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)), (local_only?"true":"false"));
      
      gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (sfcd->dialog), local_only);
      succeeded = TRUE;
    }
      
    g_mutex_unlock (&sfcd->stateMutex);
	}

  g_mutex_unlock (&cli.dialogsMutex);
  return succeeded;
}

gboolean
server_sandbox_file_chooser_dialog_get_local_only (GVariant *parameters,
                                                   gboolean *local_only,
                                                   GError  **error)
{
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;
  gboolean                    succeeded  = FALSE;

  g_variant_get (parameters, "(&s)", &dialog_id);
  g_mutex_lock (&cli.dialogsMutex);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
				"SandboxFileChooserDialog.GetLocalOnly: dialog '%s' does not exist.\n", dialog_id);

	  syslog (LOG_WARNING, "%s", (*error)->message);
	}
	else
	{
	  g_mutex_lock (&sfcd->stateMutex);
    
    // RUNNING is a forbidden state for this function
    if (sfcd->state == SANDBOX_FILECHOOSERDIALOG_RUNNING)
    {
  		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SANDBOX_FILECHOOSERDIALOG_FORBIDDEN_QUERY,
					"SandboxFileChooserDialog.GetLocalOnly: dialog '%s' ('%s') is already running and cannot be queried.\n",
					dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
		
		  syslog (LOG_WARNING, "%s", (*error)->message);
    }
    else // sfcd->state != SANDBOX_FILECHOOSERDIALOG_RUNNING
    {
      *local_only = gtk_file_chooser_get_local_only (GTK_FILE_CHOOSER (sfcd->dialog));
		  syslog (LOG_DEBUG, "SandboxFileChooserDialog.GetLocalOnly: dialog '%s' ('%s') has local-only set to %s.\n",
              dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)), (*local_only?"true":"false"));
      succeeded = TRUE;
    }
    
    g_mutex_unlock (&sfcd->stateMutex);
	}

  g_mutex_unlock (&cli.dialogsMutex);
  return succeeded;
}


























//FIXME all the code below is in the old style and not mutex-protected

gboolean
server_sandbox_file_chooser_dialog_set_select_multiple (GVariant *parameters,
                                                        GError  **error)
{
  /*TODO security_hook to:
     - ensure the dialog isn't running.
   */
   
  const gboolean              select_multiple;
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;

  g_variant_get (parameters, "(&sb)", &dialog_id, &select_multiple);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN),
					SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"The dialog for which you want to set the select-multiple flag is nowhere to be found.");
		return FALSE;
	}
	else
	{
    syslog (LOG_DEBUG,
            "Setting Multiple Selection to %s for GtkFileChooserDialog: %s, titled '%s'\n",
            select_multiple? "True":"False",
            dialog_id,
            gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (sfcd->dialog), select_multiple);
      
	  return TRUE;
	}
}



gboolean
server_sandbox_file_chooser_dialog_get_select_multiple (GVariant *parameters,
                                                        gboolean *select_multiple,
                                                        GError  **error)
{
  /*TODO security_hook to:
     - nothing special, just state management
   */
   
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;

  g_variant_get (parameters, "(&s)", &dialog_id);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN),
					SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"These are not the dialogs you are looking to interrogate (on their multiple selection).");
		return FALSE;
	}
	else
	{
    syslog (LOG_DEBUG, "Mutiple Selection is %s for GtkFileChooserDialog: %s, titled '%s'\n",
            *select_multiple? "On":"Off",
            dialog_id,
            gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
    *select_multiple = gtk_file_chooser_get_select_multiple (GTK_FILE_CHOOSER (sfcd->dialog));
    
	  return TRUE;
	}
}



gboolean
server_sandbox_file_chooser_dialog_set_show_hidden (GVariant *parameters,
                                                    GError  **error)
{
  /*TODO security_hook to:
     - ensure the dialog isn't running.
   */
   
  const gboolean              show_hidden;
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;

  g_variant_get (parameters, "(&sb)", &dialog_id, &show_hidden);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN),
					SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"The dialog for which you want to set the show-hidden flag is nowhere to be found.");
		return FALSE;
	}
	else
	{
    syslog (LOG_DEBUG,
            "%s hidden files for GtkFileChooserDialog: %s, titled '%s'\n",
            show_hidden? "Showing":"Hiding",
            dialog_id,
            gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
    gtk_file_chooser_set_show_hidden (GTK_FILE_CHOOSER (sfcd->dialog), show_hidden);
      
	  return TRUE;
	}
}



gboolean
server_sandbox_file_chooser_dialog_get_show_hidden (GVariant *parameters,
                                                    gboolean *show_hidden,
                                                    GError  **error)
{
  /*TODO security_hook to:
     - nothing special, just state management
   */
   
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;

  g_variant_get (parameters, "(&s)", &dialog_id);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN),
					SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"These are not the dialogs you are looking to interrogate (on their multiple selection).");
		return FALSE;
	}
	else
	{
    syslog (LOG_DEBUG, "Show hidden files is %s for GtkFileChooserDialog: %s, titled '%s'\n",
            *show_hidden? "On":"Off",
            dialog_id,
            gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
    *show_hidden = gtk_file_chooser_get_show_hidden (GTK_FILE_CHOOSER (sfcd->dialog));
    
	  return TRUE;
	}
}





















static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  // With the current code architecture there should be a single sender for each
  // running server (less dependable this way). We might change that in the
  // future after the code has been severely hardened.

  //TODO new client? should be some sort of greetings during which it sends me
  // its STDOUT and STDERR. This will allow transparent composition of error
  // messages

                               
  gboolean  ret = FALSE;
  GError   *err = NULL;
  

  if (g_strcmp0 (method_name, "New") == 0)
  {
    gchar        *dialog_id = NULL;
    
    ret = server_sandbox_file_chooser_dialog_new (parameters, 
                                                  &dialog_id,
                                                  &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, 
                                             g_variant_new ("(s)", dialog_id));
    if (dialog_id)
      g_free (dialog_id);
  }
  else if (g_strcmp0 (method_name, "Destroy") == 0)
  {
    ret = server_sandbox_file_chooser_dialog_destroy (parameters, &err);

    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "GetState") == 0)
  {
    gint32 state = -1;
  
    ret = server_sandbox_file_chooser_dialog_get_state (parameters, 
                                                        &state,
                                                        &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, 
                                             g_variant_new ("(i)", state));
  }
  else if (g_strcmp0 (method_name, "Run") == 0)
  {
    ret = server_sandbox_file_chooser_dialog_run (connection,
                                                  sender,
                                                  object_path,
                                                  interface_name,
                                                  parameters,
                                                  &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "CancelRun") == 0)
  {
    ret = server_sandbox_file_chooser_dialog_cancel_run (parameters, 
                                                         &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "Present") == 0)
  {
    ret = server_sandbox_file_chooser_dialog_present (parameters, 
                                                      &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "SetAction") == 0)
  { 
    ret = server_sandbox_file_chooser_dialog_set_action (parameters, &err);

    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "GetAction") == 0)
  {
    gint32 action = -1;
    
    ret = server_sandbox_file_chooser_dialog_get_action (parameters, 
                                                         &action,
                                                         &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, 
                                             g_variant_new ("(i)", action));
  }
  else if (g_strcmp0 (method_name, "SetLocalOnly") == 0)
  { 
    ret = server_sandbox_file_chooser_dialog_set_local_only (parameters, &err);

    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "GetLocalOnly") == 0)
  {
    gboolean local_only;
    
    ret = server_sandbox_file_chooser_dialog_get_local_only (parameters, 
                                                             &local_only,
                                                             &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, 
                                             g_variant_new ("(b)", local_only));
  }
  else if (g_strcmp0 (method_name, "SetSelectMultiple") == 0)
  { 
    ret = server_sandbox_file_chooser_dialog_set_select_multiple (parameters, &err);

    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "GetSelectMultiple") == 0)
  {
    gboolean select_multiple;
    
    ret = server_sandbox_file_chooser_dialog_get_select_multiple (parameters, 
                                                                  &select_multiple,
                                                                  &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, 
                                             g_variant_new ("(b)", select_multiple));
  }
  else if (g_strcmp0 (method_name, "SetShowHidden") == 0)
  { 
    ret = server_sandbox_file_chooser_dialog_set_show_hidden (parameters, &err);

    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "GetShowHidden") == 0)
  {
    gboolean show_hidden;
    
    ret = server_sandbox_file_chooser_dialog_get_show_hidden (parameters, 
                                                                  &show_hidden,
                                                                  &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, 
                                             g_variant_new ("(b)", show_hidden));
  }
  
  // Handle errors here -- will need attention later on (factorised for now)
  // TODO: distinguish security policy errors from GTK errors
  // FIXME: never ever put client-controlled strings into error without
  // neutralising the format (doesnt occur on GDBus-obtained strings though)
  if (err)
  {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "org.mupuf.SandboxUtils.%s",
                                             err->message);
      g_error_free (err);
  }
  else if (!ret)
  {
		syslog (LOG_CRIT,
		        "org.mupuf.SandboxUtils.SandboxFileChooserDialog: the '%s' handler failed but did not return any error message (this should never happen, please report a bug).\n",
		        method_name);
  }
}

// Only async, no properties anyway
static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  NULL,
  NULL
};

//TODO
/*
static void
register_dbus_errors ()
{
  g_dbus_error_register_error (g_quark_from_static_string (SFCD_ERROR_DOMAIN),
					                     SANDBOX_FILECHOOSERDIALOG_CREATION_ERROR,
					                     "SandboxFileChooserDialog Creation Error");
					                    
  g_dbus_error_register_error (g_quark_from_static_string (SFCD_ERROR_DOMAIN),
					                     SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					                     "SandboxFileChooserDialog Not Found");
}
*/

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  guint registration_id;

  registration_id = g_dbus_connection_register_object (connection,
                                                       SANDBOXUTILS_PATH,
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       NULL,  /* user_data */
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError** */
  
  g_assert (registration_id > 0);
}

static gboolean
watchdog_func (gpointer data)
{
  sd_notify(0, "WATCHDOG=1");
  
  return TRUE;
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  sd_notify(0, "READY=1");
  
  g_timeout_add_seconds (10, watchdog_func, NULL);  
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  syslog (LOG_CRIT, NAME" is already running elsewhere, shutting down\n");
  
  //TODO Clean up the memory

  // Systemd will hopefully solve this offense for us, let's just exit sadly
  exit (EXIT_FAILURE);
}

static void
signal_manager (int signal)
{
  syslog (LOG_DEBUG, NAME" received SIGINT, now shutting down...\n"); 

  //TODO clean up memory, etc

  exit(EXIT_SUCCESS);  
}

int
main (int argc, char *argv[])
{
  // Internal to the server
  guint               owner_id;
  GMainLoop           *loop;
	struct sigaction    action;
	
#ifndef NDEBUG
#ifdef _GNU_SOURCE
  mtrace ();
#endif
#endif

	// Temporary client
  memset (&cli, 0, sizeof (SandboxFileChooserDialogClient));
  cli.dialogs = g_hash_table_new (g_str_hash, g_str_equal);
  g_mutex_init (&cli.dialogsMutex);

  // Initialise GTK for later
  gtk_init (&argc, &argv);

	// Intercept signals
	memset (&action, 0, sizeof (struct sigaction));
  action.sa_handler = signal_manager;
  sigaction (SIGINT, &action, NULL);
  
  // Open log
  openlog (NAME, LOG_PID | LOG_CONS | LOG_PERROR, LOG_USER);
  syslog (LOG_INFO, "Starting "NAME" version "VERSION"\n");


  // Quoting the doc: this is lazy!
  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_assert (introspection_data != NULL);
  
/*
  // Register errors to shut DBus's mouth
  register_dbus_errors ();
*/

  // Get this bus going
  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             SFCD_IFACE,
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             NULL,
                             NULL);

  // Run
  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  // Clean up server
  g_bus_unown_name (owner_id);
  g_dbus_node_info_unref (introspection_data);
  
  // Clean up client
  g_mutex_clear (&cli.dialogsMutex);
  
  if (cli.dialogs)
  {
    //TODO empty dialogs table
    g_object_unref (cli.dialogs);
  }
  
  // Shutdown the syslog log
  closelog ();

  return EXIT_SUCCESS;
}
