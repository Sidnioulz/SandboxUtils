#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <syslog.h>
#include <gio/gio.h>
#include <glib/gprintf.h>

#ifdef G_OS_UNIX
#include <gio/gunixfdlist.h>
/* For STDOUT_FILENO */
#include <unistd.h>
#endif

#include "server.h"

// TODO manage multiple clients via sub-processes (so a client cannot crash sfad)
// TODO watchdog
// TODO error domain https://developer.gnome.org/gio/2.28/GDBusError.html


//FIXME temp client
SandboxFileChooserDialogClient cli;

// GDBus internals
static GDBusNodeInfo *introspection_data = NULL;

/* Introspection data for the service we are exporting */
static const gchar introspection_xml[] =
  "<node name='"SANDBOXUTILSPATH"'>"
	  "<interface name='"SANDBOXFILECHOOSERDIALOGIFACE"'>"
		  "<annotation name='org.freedesktop.DBus.GLib.CSymbol' value='server'/>"
		  "<method name='New'>"
			  "<arg type='s' name='title' direction='in' />"
			  "<arg type='s' name='parent_id' direction='in' />"
			  "<arg type='i' name='action' direction='in' />"
			  "<arg type='a{sv}' name='button_list' direction='in' />"
			  "<arg type='s' name='dialog_id' direction='out' />"
		  "</method>"
		  "<method name='Destroy'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
		  "</method>"
		  "<method name='Run'>"
			  "<arg type='s' name='dialog_id' direction='in' />"
			  "<arg type='i' name='return_code' direction='out' />"
			  "<arg type='b' name='was_destroyed' direction='out' />"
		  "</method>"
		  "<method name='Present'>"
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
     - prevent DoS attacks from a specific client
     - implement limits on the number of concurrent dialogs per client
   */

//TODO


  const gchar *title;
  const gchar *parent_id;
  const gint32 action;
  GVariantIter *iter;
  GVariant *item;

  g_variant_get (parameters, "(&ssia{sv})", &title, &parent_id, &action, &iter);
  
  GtkWidget *dialog = gtk_file_chooser_dialog_new (title, NULL, action, NULL, NULL);

  // Most likely memory limits
	if (dialog==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
					SANDBOX_FILECHOOSERDIALOG_CREATION_ERROR,
					"The dialog is uncooperative. Exterminate!");
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
	  g_hash_table_insert (cli.dialogs, key, sfcd);
    syslog (LOG_DEBUG, "New GtkFileChooserDialog: %s, titled '%s'\n", *dialog_id, title);

	  return TRUE;
	}
}




static void
_server_sandbox_file_chooser_dialog_destroy (SandboxFileChooserDialog *sfcd, const gchar *dialog_id)
{
  if (g_hash_table_remove (cli.dialogs, dialog_id))
    {
      syslog (LOG_DEBUG, "Destroying GtkFileChooserDialog: %s, titled '%s'\n", dialog_id, gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
            
      //TODO unlock/join
      g_mutex_clear (&sfcd->runMutex);
      g_mutex_clear (&sfcd->stateMutex);
      gtk_widget_destroy (sfcd->dialog);
    }
    else
      syslog (LOG_CRIT, "GtkFileChooserDialog race condition on deletion -- will not free or touch object just in case (%s)\n", dialog_id);
}

gboolean
server_sandbox_file_chooser_dialog_destroy (GVariant *parameters,
                                            GError  **error)
{
  /*TODO security_hook to:
     - manage limits on number of dialogs
     - if RUNNING state, send a deletion signal and let others manage
   */
  
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;

  g_variant_get (parameters, "(&s)", &dialog_id);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
					SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"These are not the dialogs you are looking to exterminate.");
		return FALSE;
	}
	else
	{
	  _server_sandbox_file_chooser_dialog_destroy  (sfcd, dialog_id);
    
	  return TRUE;
	}
}

/* RUNNING METHODS */
//FIXME when ANY dialog is running, presenting/running other dialogs should be proscribed? or compo should avoid overlappin'               
//TODO rewrite in GTK to verify a static structure indicating which GtkFileChooserDialog's exist. If a dialog being run is a GFCD, call GFCD_run and exit
gboolean
server_sandbox_file_chooser_dialog_run (GVariant *parameters,
                                        gint32 *return_code,
                                        gboolean *was_destroyed,
                                        GError **error)
{
  /*TODO security_hook to:
     - change state to RUNNING - no more changes of config
     - manage limits on number of concurrent dialogs
   */
   
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;

  g_variant_get (parameters, "(&s)", &dialog_id);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
					SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"These are not the dialogs you are looking to run.");
		return FALSE;
	}
	else
	{
    syslog (LOG_DEBUG,
            "Running GtkFileChooserDialog: %s, titled '%s'...\n",
            dialog_id,
            gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));

    *return_code = gtk_dialog_run (GTK_DIALOG (sfcd->dialog));
    
    
    /* In the original API closing or destroying the window/widget would cause a
       GTK_RESPONSE_DELETE_EVENT. Here when the user doesn't express a desire to
       use the window to open a file, we destroy it and notify the client. */
    if (*return_code == GTK_RESPONSE_NONE || *return_code == GTK_RESPONSE_DELETE_EVENT)
    {
  	  _server_sandbox_file_chooser_dialog_destroy  (sfcd, dialog_id);

      syslog (LOG_DEBUG,
            "Ran GtkFileChooserDialog: %s, and destroyed it (return code %d)\n",
            dialog_id,
            *return_code);
      *was_destroyed = TRUE;
    }
    else
    {
      gtk_widget_hide (sfcd->dialog); // client can destroy after querying

      syslog (LOG_DEBUG,
            "Ran GtkFileChooserDialog: %s, titled '%s' and got return code %d\n",
            dialog_id,
            gtk_window_get_title (GTK_WINDOW (sfcd->dialog)),
            *return_code);
      *was_destroyed = FALSE;
    }
    
	  return TRUE;
  }
}

gboolean
server_sandbox_file_chooser_dialog_present (GVariant *parameters,
                                            GError **error)
{
  /*TODO security_hook to:
     - only allow when the state is RUNNING (otherwise do nothing)
   */
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;

  g_variant_get (parameters, "(&s)", &dialog_id);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
					SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"These are not the dialogs you are looking to present.");
		return FALSE;
	}
	else if (0) {} //TODO state checking
	else
	{
    syslog (LOG_DEBUG,
            "Presenting GtkFileChooserDialog: %s, titled '%s'...\n",
            dialog_id,
            gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));

    gtk_window_present (GTK_WINDOW (sfcd->dialog));
    
	  return TRUE;
  }
}






/* CONFIGURATION / MANAGEMENT METHODS  -- USABLE BEFORE RUN */
gboolean
server_sandbox_file_chooser_dialog_set_action (GVariant *parameters,
                                               GError  **error)
{
  /*TODO security_hook to:
     - ensure the dialog isn't running. A running dialog should not be edited by
       the app to prevent social engineering+timing attacks
   */
   
  const gint32               action;
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;

  g_variant_get (parameters, "(&si)", &dialog_id, &action);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
					SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"These are not the dialogs you are looking to set the action for.");
		return FALSE;
	}
	else
	{
    syslog (LOG_DEBUG,
            "Setting Action %d for GtkFileChooserDialog: %s, titled '%s'\n",
            action,
            dialog_id,
            gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
    gtk_file_chooser_set_action (GTK_FILE_CHOOSER (sfcd->dialog), action);
      
	  return TRUE;
	}
}



gboolean
server_sandbox_file_chooser_dialog_get_action (GVariant *parameters,
                                               gint32   *action,
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
		g_set_error (error, g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
					SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"These are not the dialogs you are looking to interrogate (on their action).");
		return FALSE;
	}
	else
	{
    syslog (LOG_DEBUG,
            "Getting Action %d for GtkFileChooserDialog: %s, titled '%s'\n",
            *action,
            dialog_id,
            gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
    *action = gtk_file_chooser_get_action (GTK_FILE_CHOOSER (sfcd->dialog));
    
	  return TRUE;
	}
}



gboolean
server_sandbox_file_chooser_dialog_set_local_only (GVariant *parameters,
                                                   GError  **error)
{
  /*TODO security_hook to:
     - ensure the dialog isn't running. A running dialog should not be edited by
       the app to prevent social engineering+timing attacks
   */
   
  const gboolean              local_only;
  const gchar                *dialog_id  = NULL;
  SandboxFileChooserDialog   *sfcd       = NULL;

  g_variant_get (parameters, "(&sb)", &dialog_id, &local_only);
  sfcd = g_hash_table_lookup (cli.dialogs, dialog_id);

	if (sfcd==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
					SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"The dialog for which you want to set the local-only flag is nowhere to be found.");
		return FALSE;
	}
	else
	{
    syslog (LOG_DEBUG,
            "Setting Local Only to %s for GtkFileChooserDialog: %s, titled '%s'\n",
            local_only? "True":"False",
            dialog_id,
            gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (sfcd->dialog), local_only);
      
	  return TRUE;
	}
}



gboolean
server_sandbox_file_chooser_dialog_get_local_only (GVariant *parameters,
                                                   gboolean *local_only,
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
		g_set_error (error, g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
					SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
					"These are not the dialogs you are looking to interrogate (on their local-only flag).");
		return FALSE;
	}
	else
	{
    syslog (LOG_DEBUG, "Local Only is %s for GtkFileChooserDialog: %s, titled '%s'\n",
            *local_only? "True":"False",
            dialog_id,
            gtk_window_get_title (GTK_WINDOW (sfcd->dialog)));
    *local_only = gtk_file_chooser_get_local_only (GTK_FILE_CHOOSER (sfcd->dialog));
    
	  return TRUE;
	}
}



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
		g_set_error (error, g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
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
		g_set_error (error, g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
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
		g_set_error (error, g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
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
		g_set_error (error, g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
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
  //TODO new client? should be some sort of greetings during which it sends me
  // its STDOUT and STDERR. This will allow transparent composition of error
  // messages

  gboolean ret = FALSE;
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
  else if (g_strcmp0 (method_name, "Run") == 0)
  {
    gint32 return_code;
    gboolean was_destroyed;
    
    
    ret = server_sandbox_file_chooser_dialog_run (parameters, 
                                                 &return_code,
                                                 &was_destroyed,
                                                 &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, 
                                             g_variant_new ("(ib)", return_code, was_destroyed));
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
  else
  {
    //TODO return "unsupported method" type of error and clean-up  
  }
  
  // Handle errors here -- will need attention later on (factorised for now)
  // TODO: distinguish security policy errors from GTK errors
  // XXX: never ever put client-controlled strings into error without
  // neutralising the format
  if (err)
  {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "/org/mupuf/SandboxUtils - %s",
                                             err->message);
      g_error_free (err);
  }
}

// Only async, no properties anyway
static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  NULL,
  NULL
};

/*
static void
register_dbus_errors ()
{
  g_dbus_error_register_error (g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
					                     SANDBOX_FILECHOOSERDIALOG_CREATION_ERROR,
					                     "SandboxFileChooserDialog Creation Error");
					                    
  g_dbus_error_register_error (g_quark_from_static_string (SANDBOXFILECHOOSERDIALOGERRORDOMAIN),
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
                                                       "/org/mupuf/SandboxUtils",
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       NULL,  /* user_data */
                                                       NULL,  /* user_data_free_func */
                                                       NULL); /* GError** */
  g_assert (registration_id > 0);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  syslog (LOG_CRIT, NAME" is already running elsewhere, shutting down\n");
  
  //TODO Clean up the memory, never know what can happen

  // Systemd will hopefully solve this offense for us, let's just exit sadly
  exit (EXIT_FAILURE);
}

static void
signal_manager (int signal)
{
  syslog (LOG_DEBUG, NAME" received SIGINT, now shutting down...\n"); 

  //TODO clean up client memory, etc

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
  openlog (NAME, LOG_PID | LOG_CONS, LOG_USER);
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
                             SANDBOXFILECHOOSERDIALOGIFACE,
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
  //TODO wait on mutex.
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
