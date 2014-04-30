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
#ifndef __SANDBOX_FILE_CHOOSER_DIALOG_H__
#define __SANDBOX_FILE_CHOOSER_DIALOG_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include "sandboxutilscommon.h"

#define SFCD_IFACE SANDBOXUTILS_IFACE".SandboxFileChooserDialog"
#define SFCD_ERROR_DOMAIN SFCD_IFACE".Error"


/* Dialog running state - this is used to prevent forms of abuse when the dialog
 * is running. A dialog is created at CONFIGURATION state, configured by the
 * client, run (during which it is in RUNNING state) and then switched to the
 * DATA_RETRIEVAL state, during which the client reads the file(s) selected by 
 * the user and has access to them. If the dialog is modified or reused, it
 * switches back to a previous state and query functions cannot be called until
 * it successfully runs again.
 * 
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


typedef enum {
  SFCD_WRONG_STATE     = 0, // Unused, only to have a return value to get_state
  SFCD_CONFIGURATION   = 1,
  SFCD_RUNNING         = 2,
  SFCD_DATA_RETRIEVAL  = 3,
} SfcdRunState;

static const
gchar *SfcdRunStatePrintable[5] = {"Wrong State (an error occurred)",
                                   "Configuration",
                                   "Running",
                                   "Data Retrieval",
                                   NULL};
                                   

/* Error codes - used to distinguish between families of errors when misusing
 * the sfcd methods.
 */
typedef enum {
  SFCD_ERROR_CREATION,
  SFCD_ERROR_LOOKUP,
  SFCD_ERROR_FORBIDDEN_CHANGE,
  SFCD_ERROR_FORBIDDEN_QUERY,
  SFCD_ERROR_UNKNOWN
} SfcdErrorCode;


#define SANDBOX_TYPE_FILE_CHOOSER_DIALOG            (sfcd_get_type ())
#define SANDBOX_FILE_CHOOSER_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SANDBOX_TYPE_FILE_CHOOSER_DIALOG, SandboxFileChooserDialog))
#define SANDBOX_IS_FILE_CHOOSER_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SANDBOX_TYPE_FILE_CHOOSER_DIALOG))
#define SANDBOX_FILE_CHOOSER_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SANDBOX_TYPE_FILE_CHOOSER_DIALOG, SandboxFileChooserDialogClass))
#define SANDBOX_IS_FILE_CHOOSER_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SANDBOX_TYPE_FILE_CHOOSER_DIALOG))
#define SANDBOX_FILE_CHOOSER_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SANDBOX_TYPE_FILE_CHOOSER_DIALOG, SandboxFileChooserDialogClass))

typedef struct _SandboxFileChooserDialog        SandboxFileChooserDialog;
typedef struct _SandboxFileChooserDialogClass   SandboxFileChooserDialogClass;
typedef struct _SandboxFileChooserDialogPrivate SandboxFileChooserDialogPrivate;

struct _SandboxFileChooserDialog
{
  GObject                             parent_instance;
  SandboxFileChooserDialogPrivate    *priv;
  gchar                              *id;
};

struct _SandboxFileChooserDialogClass
{
  GObjectClass parent_class;
  
  guint run_finished_signal;
};

/* used by SANDBOX_TYPE_FILE_CHOOSER_DIALOG */
GType sfcd_get_type (void);



/* GENERIC METHODS */
SandboxFileChooserDialog *
sfcd_new (GtkWidget *dialog);

void
sfcd_destroy (SandboxFileChooserDialog *self);

SfcdRunState
sfcd_get_state (SandboxFileChooserDialog *self);

const gchar *
sfcd_get_state_printable (SandboxFileChooserDialog *self);

const gchar *
sfcd_get_dialog_title (SandboxFileChooserDialog *self);

gboolean
sfcd_is_running (SandboxFileChooserDialog *self);


/* RUNNING METHODS */
gboolean
sfcd_run (SandboxFileChooserDialog  *self,
          GError                   **error);

gboolean
sfcd_present (SandboxFileChooserDialog  *self,
              GError                   **error);

gboolean
sfcd_cancel_run (SandboxFileChooserDialog  *self,
                 GError                   **error);


/* CONFIGURATION / MANAGEMENT METHODS  -- USABLE BEFORE RUN */
// FIXME find out how to pass the actual enum over GDBus
gboolean
sfcd_set_action (SandboxFileChooserDialog  *self,
                 gint32                     action,
                 GError                   **error);

// FIXME find out how to pass the actual enum over GDBus
gboolean
sfcd_get_action (SandboxFileChooserDialog  *self,
                 gint32                    *action,
                 GError                   **error);

gboolean
sfcd_set_local_only (SandboxFileChooserDialog  *self,
                     gboolean                   local_only,
                     GError                   **error);

gboolean
sfcd_get_local_only (SandboxFileChooserDialog  *self,
                     gboolean                  *local_only,
                     GError                   **error);

gboolean
sfcd_set_select_multiple (SandboxFileChooserDialog  *self,
                          gboolean                   select_multiple,
                          GError                   **error);

gboolean
sfcd_get_select_multiple (SandboxFileChooserDialog  *self,
                          gboolean                  *select_multiple,
                          GError                   **error);

gboolean
sfcd_set_show_hidden (SandboxFileChooserDialog  *self,
                      gboolean                   show_hidden,
                      GError                   **error);

gboolean
sfcd_get_show_hidden (SandboxFileChooserDialog  *self,
                      gboolean                  *show_hidden,
                      GError                   **error);

gboolean
sfcd_set_do_overwrite_confirmation (SandboxFileChooserDialog  *self,
                      gboolean                   do_overwrite_confirmation,
                      GError                   **error);

gboolean
sfcd_get_do_overwrite_confirmation (SandboxFileChooserDialog  *self,
                      gboolean                  *do_overwrite_confirmation,
                      GError                   **error);

gboolean
sfcd_set_create_folders (SandboxFileChooserDialog  *self,
                         gboolean                   create_folders,
                         GError                   **error);

gboolean
sfcd_get_create_folders (SandboxFileChooserDialog  *self,
                         gboolean                  *create_folders,
                         GError                   **error);

gboolean
sfcd_get_create_folders (SandboxFileChooserDialog  *self,
                         gboolean                  *create_folders,
                         GError                   **error);

gboolean
sfcd_set_current_name (SandboxFileChooserDialog  *self,
                       const gchar               *name,
                       GError                   **error);

/* 
 * Proposed API changes
 * _____________________________________________________________________________
 * API CHANGE: On-the-fly filename transformation widget
 *
 * The method to get the currently typed name needs to be changed to a method of
 * transforming the filename automatically (using regexp matching maybe?). The
 * key point is that the transformation must be visible to and agreed upon by
 * the user (to account for overrides and custom-override behaviour that would
 * force them).
 *
 *   gtk_file_chooser_get_current_name ()
 * _____________________________________________________________________________
 * API CHANGE: replace the preview widget by a standalone sandboxable previewer
 * 
 * gtk_file_chooser_set_preview_widget () cannot be used because it's impossible
 * to let the client app access files to preview them -- any valuable info would
 * leak. Instead, the server must call a standalone and sandboxable bunch of
 * instructions that ultimately generate a preview. This preview app could be
 * queried with the currently selected filename(s), latest activated one(s), and
 * some custom-format information from the app -- the only thing that matters is
 * that it cannot talk back except in the form of some on-screen rendering.
 * 
 *   void	gtk_file_chooser_set_preview_widget ()
 *   GtkWidget *	gtk_file_chooser_get_preview_widget ()
 *   void	gtk_file_chooser_set_preview_widget_active ()
 *   gboolean	gtk_file_chooser_get_preview_widget_active ()
 *   void	gtk_file_chooser_set_use_preview_label ()
 *   gboolean	gtk_file_chooser_get_use_preview_label ()
 *   char *	gtk_file_chooser_get_preview_filename ()
 *   char *	gtk_file_chooser_get_preview_uri ()
 *   GFile *	gtk_file_chooser_get_preview_file ()
 * _____________________________________________________________________________
 * API CHANGE: replacing the extra widget by ???
 * 
 * I confess I don't know what to do with the extra widget. A client could
 * generate their own widget locally that does not have any access to filenames
 * or the dialog and that widget could be automagically composed without the
 * window of the actual dialog, but that only addresses widgets that interact 
 * exclusively with the app and not with the contents of the dialog.
 * 
 *   void	gtk_file_chooser_set_extra_widget ()
 *   GtkWidget *	gtk_file_chooser_get_extra_widget ()
 * _____________________________________________________________________________
 * API CHANGE: modify the select family a bit. Limit it to the config state, and
 * add a select_if_exists family of methods for apps to preselect files without
 * seeing whether they actually exist, while in config state. Clients should be
 * made aware that they can't really know what's selected, so they can't expect
 * functions like unselect_filename () to be often useful.
 *
 *   gboolean	gtk_file_chooser_select_filename ()
 *   void	gtk_file_chooser_unselect_filename ()
 *   void	gtk_file_chooser_select_all ()
 *   void	gtk_file_chooser_unselect_all ()
 *   gboolean	gtk_file_chooser_select_uri ()
 *   void	gtk_file_chooser_unselect_uri ()
 *   gboolean	gtk_file_chooser_select_file ()
 *   void	gtk_file_chooser_unselect_file ()
 * _____________________________________________________________________________
 * 
 */

/*

// Configuration
void	gtk_file_chooser_set_current_name ()
gboolean	gtk_file_chooser_set_filename ()
gboolean	gtk_file_chooser_set_current_folder ()
gboolean	gtk_file_chooser_set_uri ()
gboolean	gtk_file_chooser_set_current_folder_uri ()
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
gboolean	gtk_file_chooser_set_current_folder_file ()
gboolean	gtk_file_chooser_set_file ()

// Retrieval
gchar *	gtk_file_chooser_get_current_name ()
gchar *	gtk_file_chooser_get_filename ()
GSList *	gtk_file_chooser_get_filenames ()
gchar *	gtk_file_chooser_get_current_folder ()
gchar *	gtk_file_chooser_get_uri ()
GSList *	gtk_file_chooser_get_uris ()
gchar *	gtk_file_chooser_get_current_folder_uri ()
GFile *	gtk_file_chooser_get_current_folder_file ()
GFile *	gtk_file_chooser_get_file ()
GSList *	gtk_file_chooser_get_files ()

*/







//TODO check which signals are allowed
//GtkFileChooserConfirmation	confirm-overwrite	Run Last    // TODO implement
//void	current-folder-changed	Run Last                    // Not exported, forbidden info
//void	file-activated	Run Last                            // Not exported, "run-finished" instead
//void	selection-changed	Run Last                          // Not exported, forbidden info
//void	update-preview	Run Last                            // TODO new architecture for widget preview

//TODO rest of the GtkFileChooserDialog API -- should be quick!

#endif /* __SANDBOX_FILE_CHOOSER_DIALOG_H__ */
