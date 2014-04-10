/*
 *   Source Code from the DBus Activation Tutorial
 *   from Raphael Slinckx
 *
 *   This code illustrates how to requrest the dbus daemon
 *   to automatically start a program that provides a given
 *   service. For more detailed information refer also to
 *   http://raphael.slinckx.net/blog/documents/dbus-tutorial
 *   where all source has taken from.
 *
 *   Provision of all glue code to form compilable application
 *   by Otto Linnemann
 *
 *   Declarations for Server
 */

#ifndef SERVER_H
#define SERVER_H

/* sfad internals */
#define NAME "sfad"
#define VERSION "0.2"

#define SANDBOXFILECHOOSERDIALOGIFACE "org.mupuf.SandboxFileChooserDialog"
#define SANDBOXUTILSPATH "/org/mupuf/SandboxUtils"
#define SANDBOXFILECHOOSERDIALOGERRORDOMAIN SANDBOXFILECHOOSERDIALOGIFACE".Error"

static const enum {
  SANDBOX_FILECHOOSERDIALOG_CREATION_ERROR,
  SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR
} SandboxFileChooserDialogErrorCodes;

/* Dialog running state - this is used to prevent forms of abuse when the dialog
 * is running. A dialog is created at CONFIGURATION state, configured by the
 * client, run (during which it is in RUNNING state) and then switched to the
 * DATA_RETRIEVAL state, during which the client reads the file(s) selected by 
 * the user and has access to them. If the dialog is reconfigured or reused, it
 * switches back to a previous state and query functions cannot be called until
 * it successfully runs again.
 */

typedef enum {
  SANDBOX_FILECHOOSERDIALOG_CONFIGURATION,
  SANDBOX_FILECHOOSERDIALOG_RUNNING,
  SANDBOX_FILECHOOSERDIALOG_DATA_RETRIEVAL
} RunState;

//XXX original plan for mutex'ing the state
// A semaphore is incremented before any method looks up the table, and until it's done touching the GTK dialog (except run/destroy)
// The destroy method waits, after removing the dialog entry from the table, and until said semaphore is empty, before it destroys the actual object
// When the dialog is in running state, all conf/query methods must return before they can change state themselves (mutex for state changing)
// When the dialog is running and a destroy call is sent, destroy will lock access to the semaphor and join as usual, but emit a delete-event signal beforehand


//TODO style fooBar -> foo_bar
//TODO on the long term, use g_thread_try_new and use the previous sync function with timeout handling for when threads cant be made?
 
//TODO encode states in already-written functions
//TODO use that class
// state-changing code should be mutex-protected
// In particular, run changes handlers for deletion events and must be protected
// before destroy is ever called.
typedef struct _SandboxFileChooserDialog
{
  GtkWidget             *dialog;
  RunState              state;
  GMutex                runMutex;
  GMutex                stateMutex;
    
} SandboxFileChooserDialog;

/* Hello there, client */
typedef struct _SandboxFileChooserDialogClient
{
  GHashTable       *dialogs;
  const guint32     ownLimits;
  const guint32     runLimits;   
  GMutex            dialogsMutex;     
} SandboxFileChooserDialogClient;


/* CREATION / DELETION METHODS */
gboolean
server_sandbox_file_chooser_dialog_new (GVariant *parameters,
                                        gchar **dialog_id,
                                        GError **error);
gboolean
server_sandbox_file_chooser_dialog_destroy (GVariant *parameters,
                                            GError **error);


                             
/* RUNNING METHODS */
//FIXME when ANY dialog is running, presenting/running other dialogs should be proscribed? or compo should avoid overlappin'               
//TODO rewrite in GTK to verify a static structure indicating which GtkFileChooserDialog's exist. If a dialog being run is a GFCD, call GFCD_run and exit
gboolean
server_sandbox_file_chooser_dialog_run (GVariant *parameters,
                                        gint32 *return_code,
                                        gboolean *was_destroyed,
                                        GError **error);
                                        
gboolean
server_sandbox_file_chooser_dialog_present (GVariant *parameters,
                                            GError **error);




/* CONFIGURATION / MANAGEMENT METHODS  -- USABLE BEFORE RUN */
gboolean
server_sandbox_file_chooser_dialog_set_action (GVariant *parameters,
                                               GError  **error);
gboolean
server_sandbox_file_chooser_dialog_get_action (GVariant *parameters,
                                               gint32   *action,
                                               GError  **error);
gboolean
server_sandbox_file_chooser_set_local_only (GVariant *parameters,
                                            GError **error);
gboolean
server_sandbox_file_chooser_get_local_only (GVariant *parameters,
                                            gboolean *local_only,
                                            GError  **error);
                                        
gboolean
server_sandbox_file_chooser_set_select_multiple (GVariant *parameters,
                                                 GError **error);
gboolean
server_sandbox_file_chooser_get_select_multiple (GVariant *parameters,
                                                 gboolean *select_multiple,
                                                 GError  **error);
                                        
gboolean
server_sandbox_file_chooser_set_show_hidden (GVariant *parameters,
                                             GError **error);
gboolean
server_sandbox_file_chooser_get_show_hidden (GVariant *parameters,
                                             gboolean *show_hidden,
                                             GError  **error);




                                            

gboolean server_gtk_file_chooser_set_show_hidden (int server, gchar *dialog_id, gboolean show_hidden,
                                        GError **error);

gboolean server_gtk_file_chooser_get_show_hidden (int server, gchar *dialog_id,
                                        gboolean *show_hidden,
                                        GError **error);

gboolean server_gtk_file_chooser_set_do_overwrite_confirmation (int server, gchar *dialog_id, gboolean do_overwrite_confirmation,
                                                      GError **error);

gboolean server_gtk_file_chooser_get_do_overwrite_confirmation (int server, gchar *dialog_id,
                                                      gboolean *do_overwrite_confirmation,
                                                      GError **error);

gboolean server_gtk_file_chooser_set_create_folders (int server, gchar *dialog_id, gboolean create_folders,
                                           GError **error);

gboolean server_gtk_file_chooser_get_create_folders (int server, gchar *dialog_id,
                                           gboolean *create_folders,
                                           GError **error);






gboolean server_gtk_file_chooser_set_current_name (int server, gchar *dialog_id, gchar *current_name,
                                         GError **error);

gboolean server_gtk_file_chooser_get_current_name (int server, gchar *dialog_id,
                                         gchar **current_name,
                                         GError **error);

gboolean server_gtk_file_chooser_get_filename (int server, gchar *dialog_id,
                                         gchar **filename,
                                         GError **error);

gboolean server_gtk_file_chooser_set_filename (int server, gchar *dialog_id, gchar *filename,
                                         GError **error);

gboolean server_gtk_file_chooser_select_filename (int server, gchar *dialog_id, gchar *filename,
                                         GError **error);

gboolean server_gtk_file_chooser_unselect_filename (int server, gchar *dialog_id, gchar *filename,
                                         GError **error);

gboolean server_gtk_file_chooser_select_all (int server, gchar *dialog_id,
                                         GError **error);

gboolean server_gtk_file_chooser_unselect_all (int server, gchar *dialog_id,
                                         GError **error);

gboolean server_gtk_file_chooser_get_filenames (int server, gchar *dialog_id,
                                         gchar ***filenames,
                                         GError **error);

gboolean server_gtk_file_chooser_set_current_folder (int server, gchar *dialog_id, gchar *current_folder,
                                         GError **error);

gboolean server_gtk_file_chooser_get_current_folder (int server, gchar *dialog_id,
                                         gchar **current_folder,
                                         GError **error);

gboolean server_gtk_file_chooser_get_uri (int server, gchar *dialog_id,
                                         gchar **uri,
                                         GError **error);

gboolean server_gtk_file_chooser_set_uri (int server, gchar *dialog_id, gchar *uri,
                                         GError **error);

gboolean server_gtk_file_chooser_select_uri (int server, gchar *dialog_id, gchar *uri,
                                         GError **error);

gboolean server_gtk_file_chooser_unselect_uri (int server, gchar *dialog_id, gchar *uri,
                                         GError **error);

gboolean server_gtk_file_chooser_get_uris (int server, gchar *dialog_id,
                                         gchar ***uris,
                                         GError **error);

gboolean server_gtk_file_chooser_set_current_folder_uri (int server, gchar *dialog_id, gchar *current_folder_uri,
                                         GError **error);

gboolean server_gtk_file_chooser_get_current_folder_uri (int server, gchar *dialog_id,
                                         gchar **current_folder_uri,
                                         GError **error);

/*
void	gtk_file_chooser_set_preview_widget ()
GtkWidget *	gtk_file_chooser_get_preview_widget ()

void	gtk_file_chooser_set_preview_widget_active ()
gboolean	gtk_file_chooser_get_preview_widget_active ()

void	gtk_file_chooser_set_use_preview_label ()
gboolean	gtk_file_chooser_get_use_preview_label ()

char *	gtk_file_chooser_get_preview_filename ()
char *	gtk_file_chooser_get_preview_uri ()

void	gtk_file_chooser_set_extra_widget ()
GtkWidget *	gtk_file_chooser_get_extra_widget ()


void	gtk_file_chooser_add_filter ()
void	gtk_file_chooser_remove_filter ()
GSList *	gtk_file_chooser_list_filters ()
void	gtk_file_chooser_set_filter ()
GtkFileFilter *	gtk_file_chooser_get_filter ()

gboolean	gtk_file_chooser_add_shortcut_folder ()
gboolean	gtk_file_chooser_remove_shortcut_folder ()
GSList *	gtk_file_chooser_list_shortcut_folders ()
gboolean	gtk_file_chooser_add_shortcut_folder_uri ()
gboolean	gtk_file_chooser_remove_shortcut_folder_uri ()
GSList *	gtk_file_chooser_list_shortcut_folder_uris ()
GFile *	gtk_file_chooser_get_current_folder_file ()


GFile *	gtk_file_chooser_get_file ()
GSList *	gtk_file_chooser_get_files ()
GFile *	gtk_file_chooser_get_preview_file ()
gboolean	gtk_file_chooser_select_file ()


gboolean	gtk_file_chooser_set_current_folder_file ()


gboolean	gtk_file_chooser_set_file ()
gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);
gtk_file_chooser_set_current_name (chooser, _("Untitled document"));
gtk_file_chooser_set_filename (chooser, existing_filename);
gtk_file_chooser_get_filename (chooser);


// ETC TODO: all GtkWidget, GtkDialog must be explored for useful API
// The API must be revised through a security eye though
                                            
gboolean server_gtk_widget_show_all (int server, gchar *dialog_id,
                                             GError **error);


*/



















#endif /* #ifndef SERVER_H */
