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

#include <syslog.h>

#include "sandboxfilechooserdialogdbuswrapper.h"



//TODO methods here, call the real sfcd inside






gboolean
sfcd_dbus_wrapper_sfcd_new (SandboxUtilsClient *cli,
                            GVariant           *parameters,
                            gchar             **dialog_id,
                            GError            **error)
{
  /*TODO security_hook to:
     - verify that a client can legitimately create a dialog (e.g. has a GUI)
     - implement limits on the number of concurrent dialogs per client
   */

  gboolean      succeeded = FALSE;
  const gchar  *title;
  const gchar  *parent_id;
  const gint32  action;
  GVariantIter *iter;
  GVariant     *item;

  g_return_val_if_fail (cli!=NULL, FALSE);
  g_return_val_if_fail (dialog_id!=NULL, FALSE);
  g_return_val_if_fail (error!=NULL, FALSE);

  g_variant_get (parameters, "(&ssia{sv})", &title, &parent_id, &action, &iter);

  GtkWidget *dialog = gtk_file_chooser_dialog_new (title, NULL, action, NULL, NULL);

  // Most likely memory limits
	if (dialog==NULL)
	{
		g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SFCD_ERROR_CREATION,
					"SfcdDbusWrapper.Sfcd.New: could not allocate memory to create GtkFileChooserDialog.\n");
		syslog (LOG_CRIT, "%s", (*error)->message);
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

    SandboxFileChooserDialog *sfcd = sfcd_new (dialog);

    if (sfcd==NULL)
    {
		  g_set_error (error, g_quark_from_static_string (SFCD_ERROR_DOMAIN), SFCD_ERROR_CREATION,
					  "SfcdDbusWrapper.Sfcd.New: could not allocate memory to create SandboxFileChooserDialog.\n");
		  syslog (LOG_CRIT, "%s", (*error)->message);
    }
    else
    {
	    *dialog_id = g_strdup_printf ("%p", sfcd);
	    gchar *key = g_strdup (*dialog_id);
	    
      g_mutex_lock (&cli->dialogsMutex);
	    g_hash_table_insert (cli->dialogs, key, sfcd);
	    syslog (LOG_DEBUG, "SfcdDbusWrapper.Sfcd.New: dialog '%s' ('%s') has just been created.\n",
              *dialog_id, title);
      
      succeeded = TRUE;
    }

    g_mutex_unlock (&cli->dialogsMutex);
	}

  return succeeded;
}




static void
sfcd_dbus_wrapper_dbus_call_handler (GDBusConnection       *connection,
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


  gboolean            ret    = FALSE;
  GError             *err    = NULL;
  SandboxUtilsClient *client = _get_client ();

  if (g_strcmp0 (method_name, "New") == 0)
  {
    gchar        *dialog_id = NULL;

    ret = sfcd_dbus_wrapper_sfcd_new (client,
                                      parameters,
                                      &dialog_id,
                                      &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, 
                                             g_variant_new ("(s)", dialog_id));
    if (dialog_id)
      g_free (dialog_id);
  }
  /*
  else if (g_strcmp0 (method_name, "Destroy") == 0)
  {
    ret = sfcd_dbus_wrapper_sfcd_destroy (parameters, &err);

    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "GetState") == 0)
  {
    gint32 state = -1;

    ret = sfcd_dbus_wrapper_sfcd_get_state (parameters, 
                                                        &state,
                                                        &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, 
                                             g_variant_new ("(i)", state));
  }
  else if (g_strcmp0 (method_name, "Run") == 0)
  {
    ret = sfcd_dbus_wrapper_sfcd_run (connection,
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
    ret = sfcd_dbus_wrapper_sfcd_cancel_run (parameters, 
                                                         &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "Present") == 0)
  {
    ret = sfcd_dbus_wrapper_sfcd_present (parameters, 
                                                      &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "SetAction") == 0)
  { 
    ret = sfcd_dbus_wrapper_sfcd_set_action (parameters, &err);

    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "GetAction") == 0)
  {
    gint32 action = -1;

    ret = sfcd_dbus_wrapper_sfcd_get_action (parameters, 
                                                         &action,
                                                         &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, 
                                             g_variant_new ("(i)", action));
  }
  else if (g_strcmp0 (method_name, "SetLocalOnly") == 0)
  { 
    ret = sfcd_dbus_wrapper_sfcd_set_local_only (parameters, &err);

    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "GetLocalOnly") == 0)
  {
    gboolean local_only;

    ret = sfcd_dbus_wrapper_sfcd_get_local_only (parameters, 
                                                             &local_only,
                                                             &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, 
                                             g_variant_new ("(b)", local_only));
  }
  else if (g_strcmp0 (method_name, "SetSelectMultiple") == 0)
  { 
    ret = sfcd_dbus_wrapper_sfcd_set_select_multiple (parameters, &err);

    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "GetSelectMultiple") == 0)
  {
    gboolean select_multiple;

    ret = sfcd_dbus_wrapper_sfcd_get_select_multiple (parameters, 
                                                                  &select_multiple,
                                                                  &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, 
                                             g_variant_new ("(b)", select_multiple));
  }
  else if (g_strcmp0 (method_name, "SetShowHidden") == 0)
  { 
    ret = sfcd_dbus_wrapper_sfcd_set_show_hidden (parameters, &err);

    if (ret)
      g_dbus_method_invocation_return_value (invocation, NULL);
  }
  else if (g_strcmp0 (method_name, "GetShowHidden") == 0)
  {
    gboolean show_hidden;

    ret = sfcd_dbus_wrapper_sfcd_get_show_hidden (parameters, 
                                                                  &show_hidden,
                                                                  &err);
    if (ret)
      g_dbus_method_invocation_return_value (invocation, 
                                             g_variant_new ("(b)", show_hidden));
  }

  
  */

  // Handle errors here -- will need attention later on (factorised for now)
  // TODO: distinguish security policy errors from GTK errors
  // FIXME: never ever put client-controlled strings into error without
  // neutralising the format (doesnt occur on GDBus-obtained strings though)
  if (err)
  {
      g_dbus_method_invocation_return_error (invocation,
                                             G_DBUS_ERROR,
                                             G_DBUS_ERROR_FAILED,
                                             "SfcdDbusWrapper.Dbus.CallHandler: %s",
                                             err->message);
      g_error_free (err);
  }
  else if (!ret)
  {
		syslog (LOG_CRIT,
		        "SfcdDbusWrapper.Dbus.CallHandler: the '%s' handler failed but did not return any error message (this should never happen, please report a bug).\n",
		        method_name);
  }
}




static const gchar sfcd_introspection_xml[] =
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

static const GDBusInterfaceVTable sfcd_dbus_interface_vtable =
  {
    sfcd_dbus_wrapper_dbus_call_handler,
    NULL,
    NULL
  };


static void
sfcd_dbus_on_bus_acquired (GDBusConnection *connection,
                           const gchar     *name,
                           gpointer         user_data)
{
  SfcdDbusWrapperInfo *info = user_data;

  //TODO some syslogging

  GError *error = NULL;
  // TODO in the future we'll provide a pointer to a global client table here
  // free_func will stay empty
  info->registration_id = g_dbus_connection_register_object (connection,
                                                             SANDBOXUTILS_PATH,
                                                             info->introspection_data->interfaces[0],
                                                             &sfcd_dbus_interface_vtable,
                                                             NULL,  /* user_data */
                                                             NULL,  /* user_data_free_func */
                                                             &error); /* GError** */

  if (error)
  {
    syslog (LOG_CRIT, "SfcdDbusWrapper.Dbus.OnBusAcquired: %s\n", error->message);
    g_error_free (error);
  }

  g_assert (info->registration_id > 0);
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
//  syslog (LOG_CRIT, "'%s' was lost, shutting down the SandboxFileChooserDialog interface.\n");

  //TODO finalise my sfcd dbus?
  //FIXME or should I rather reinitialise?

  //  exit (EXIT_FAILURE);
}


static SfcdDbusWrapperInfo *
sfcd_dbus_info_new ()
{
  SfcdDbusWrapperInfo *i = g_malloc (sizeof (SfcdDbusWrapperInfo));

  i->owner_id = 0;
  i->registration_id = 0;
  i->introspection_data = NULL;

  return i;
}

SfcdDbusWrapperInfo *
sfcd_dbus_wrapper_dbus_init ()
{
  SfcdDbusWrapperInfo *info = sfcd_dbus_info_new ();

  // Quoting the doc: this is lazy!
  info->introspection_data = g_dbus_node_info_new_for_xml (sfcd_introspection_xml, NULL);
  g_assert (info->introspection_data != NULL);

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

  return info;
}

void
sfcd_dbus_wrapper_dbus_shutdown (gpointer data)
{
  SfcdDbusWrapperInfo *info = data;

  //TODO error checking?

  // Clean up server
  g_bus_unown_name (info->owner_id);
  g_dbus_node_info_unref (info->introspection_data);

  g_free (info);
}

