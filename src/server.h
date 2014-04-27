/* SandboxUtils -- Daemon header
 * Copyright (c) Steve Dodier-Lazaro <sidnioulz@gmail.com>, 2014
 * 
 * Under GPLv3
 * 
 *** 
 * 
 * Class desc here
 * XXX this class manages the 
 * 
 */

#ifndef _SUD_SERVER_H
#define _SUD_SERVER_H

/* sud internals */
#define NAME "sud"
#define VERSION "0.2"

#define SANDBOXUTILS_IFACE "org.mupuf.SandboxUtils"
#define SANDBOXUTILS_PATH "/org/mupuf/SandboxUtils"

//FIXME use sfcd's own files later on
#define SFCD_IFACE "org.mupuf.SandboxFileChooserDialog"
#define SFCD_ERROR_DOMAIN SFCD_IFACE".Error"

/* Future Error Codes - TODO
 * These error codes will be registered with GDBus and propagated back to the
 * calling clients.
 */
static const enum {
  SANDBOX_FILECHOOSERDIALOG_CREATION_ERROR,
  SANDBOX_FILECHOOSERDIALOG_LOOKUP_ERROR,
  SANDBOX_FILECHOOSERDIALOG_FORBIDDEN_CHANGE,
  SANDBOX_FILECHOOSERDIALOG_FORBIDDEN_QUERY
} SandboxFileChooserDialogErrorCodes;


/* Dialog running state - this is used to prevent forms of abuse when the dialog
 * is running. A dialog is created at CONFIGURATION state, configured by the
 * client, run (during which it is in RUNNING state) and then switched to the
 * DATA_RETRIEVAL state, during which the client reads the file(s) selected by 
 * the user and has access to them. If the dialog is modified or reused, it
 * switches back to a previous state and query functions cannot be called until
 * it successfully runs again.
 * 
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
 * foo destroyed XXX -- object no longer usable
 * bar ran XXX --  switches state from RUNNING to DATA_RETRIEVAL
 * 
 ***
 * CONFIGURATION Methods:
 * SetAction -- fails if RUNNING, switches state from DATA_RETRIEVAL to CONFIGURATION
 * GetAction -- fails if RUNNING
 * ...TODO 
 *
 */
typedef enum {
  SANDBOX_FILECHOOSERDIALOG_CONFIGURATION,
  SANDBOX_FILECHOOSERDIALOG_RUNNING,
  SANDBOX_FILECHOOSERDIALOG_DATA_RETRIEVAL
} RunState;






//TODO encode states in already-written functions
typedef struct _SandboxFileChooserDialog
{
  GtkWidget             *dialog;
  RunState               state;
  GMutex                 runMutex;
  GMutex                 stateMutex;
  GThread               *runThread;
  gint                   runSourceId;
    
} SandboxFileChooserDialog;

/* Hello there, client */
typedef struct _SandboxFileChooserDialogClient
{
  GHashTable       *dialogs;
  const guint32     ownLimits;
  const guint32     runLimits;   
  GMutex            dialogsMutex;     
} SandboxFileChooserDialogClient;




/* Struct to transfer data to the running function */
typedef struct {
  gchar                      *dialog_id;
  SandboxFileChooserDialog   *sfcd;
  GDBusConnection            *connection;
  gchar                      *sender;
  gchar                      *object_path;
  gchar                      *interface_name;
  gint                        response_id;
  gint                        return_state;
  GMainLoop                  *loop;
  gboolean                    destroyed;
  gboolean                    was_modal;
  gulong                      response_handler;
  gulong                      unmap_handler;
  gulong                      destroy_handler;
  gulong                      delete_handler;
} RunFuncData;



/* CREATION / DELETION METHODS */
gboolean
server_sandbox_file_chooser_dialog_new (GVariant *parameters,
                                        gchar **dialog_id,
                                        GError **error);
gboolean
server_sandbox_file_chooser_dialog_destroy (GVariant *parameters,
                                            GError **error);


                             
/* RUNNING METHODS */
gboolean
server_sandbox_file_chooser_dialog_run (GDBusConnection       *connection,
                                        const gchar           *sender,
                                        const gchar           *object_path,
                                        const gchar           *interface_name,
                                        GVariant              *parameters,
                                        GError               **error);
                                        
gboolean
server_sandbox_file_chooser_dialog_present (GVariant *parameters,
                                            GError **error);
                          
//TODO Signals                  
//TODO CancelRun




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


gboolean server_gtk_widget_show_all (int server, gchar *dialog_id,
                                             GError **error);


*/



















#endif /* #ifndef _SUD_SERVER_H */
