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

#ifndef __SANDBOX_FILE_CHOOSER_DIALOG_H__
#define __SANDBOX_FILE_CHOOSER_DIALOG_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "sandboxutilscommon.h"

G_BEGIN_DECLS

#define SFCD_IFACE SANDBOXUTILS_IFACE".SandboxFileChooserDialog"
#define SFCD_ERROR_DOMAIN SFCD_IFACE".Error"

/**
 * SfcdState:
 * @SFCD_WRONG_STATE: Indicates the dialog was destroyed or a bug was detected.
 * @SFCD_CONFIGURATION: Indicates the dialog can be modified and configured
 *  prior to being displayed.
 * @SFCD_RUNNING: Indicates the dialog is being displayed and cannot be modified
 *  or queried.
 * @SFCD_DATA_RETRIEVAL: Indicates the dialog successfully ran and the selection
 *  of the user can now be retrieved.
 *
 * Describes the current state of a #SandboxFileChooserDialog.
 */
typedef enum {
  SFCD_WRONG_STATE     = 0, /* Unused, only to have a fallback state */
  SFCD_CONFIGURATION   = 1,
  SFCD_RUNNING         = 2,
  SFCD_DATA_RETRIEVAL  = 3,
} SfcdState;

/**
 * SfcdStatePrintable:
 * An array of string descriptions for the states of a #SandboxFileChooserDialog.
 * You can index this array with an instance of #SfcdState to obtain a printable
 * description of this instance.
 */
static const
gchar *SfcdStatePrintable[5] = {"Wrong State (an error occurred)",
                                "Configuration",
                                "Running",
                                "Data Retrieval",
                                NULL};

/**
 * SfcdErrorCode:
 * @SFCD_ERROR_CREATION: Occurs when a dialog could not be created.
 * @SFCD_ERROR_LOOKUP: Occurs when a dialog could not be found in external
 *  storage. Not used within #SandboxFileChooserDialog.
 * @SFCD_ERROR_FORBIDDEN_CHANGE: Occurs when one attempts to modify a dialog
 *  when not allowed by its current state. See #SfcdState.
 * @SFCD_ERROR_FORBIDDEN_QUERY: Occurs when one attempts to obtain information
 *  from a dialog when not allowed by its current state. See #SfcdState.
 * @SFCD_ERROR_TOOLKIT_CALL_FAILED: Occurs if the #SandboxFileChooserDialog code
 *  functionned properly but the toolkit itself returned an error when calling a
 *  method of the underlying #GtkFileChooserDialog.
 *  from a dialog when not allowed by its current state. See #SfcdState.
 * @SFCD_ERROR_UNKNOWN: Occurs if the cause of an error cannot be determined.
    Usually you get this error when a sanity check failed, probably indicating a
    bug in #SandboxUtils.
 *
 * Describes an error related to the manipulation of a
 * #SandboxFileChooserDialog instance.
 */
typedef enum {
  SFCD_ERROR_CREATION,
  SFCD_ERROR_LOOKUP,
  SFCD_ERROR_FORBIDDEN_CHANGE,
  SFCD_ERROR_FORBIDDEN_QUERY,
  SFCD_ERROR_TOOLKIT_CALL_FAILED,
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

GType sfcd_get_type (void);


/* GENERIC METHODS */
SandboxFileChooserDialog *
sfcd_new                  (GtkWidget *gtkdialog); //FIXME

void
sfcd_destroy              (SandboxFileChooserDialog *dialog);

SfcdState
sfcd_get_state            (SandboxFileChooserDialog *dialog);

const gchar *
sfcd_get_state_printable  (SandboxFileChooserDialog *dialog);

const gchar *
sfcd_get_dialog_title     (SandboxFileChooserDialog *dialog);

gboolean
sfcd_is_running           (SandboxFileChooserDialog *dialog);


/* RUNNING METHODS */
void
sfcd_run                           (SandboxFileChooserDialog  *dialog,
                                    GError                   **error);

void
sfcd_present                       (SandboxFileChooserDialog  *dialog,
                                    GError                   **error);

void
sfcd_cancel_run                    (SandboxFileChooserDialog  *dialog,
                                    GError                   **error);


/* CONFIGURATION / MANAGEMENT METHODS  -- USABLE BEFORE RUN */
void
sfcd_set_action                    (SandboxFileChooserDialog  *dialog,
                                    GtkFileChooserAction       action,
                                    GError                   **error);

GtkFileChooserAction
sfcd_get_action                    (SandboxFileChooserDialog  *dialog,
                                    GError                   **error);

void
sfcd_set_local_only                (SandboxFileChooserDialog  *dialog,
                                    gboolean                   local_only,
                                    GError                   **error);

gboolean
sfcd_get_local_only                (SandboxFileChooserDialog  *dialog,
                                    GError                   **error);

void
sfcd_set_select_multiple           (SandboxFileChooserDialog  *dialog,
                                    gboolean                   select_multiple,
                                    GError                   **error);

gboolean
sfcd_get_select_multiple           (SandboxFileChooserDialog  *dialog,
                                    GError                   **error);

void
sfcd_set_show_hidden               (SandboxFileChooserDialog  *dialog,
                                    gboolean                   show_hidden,
                                    GError                   **error);

gboolean
sfcd_get_show_hidden               (SandboxFileChooserDialog  *dialog,
                                    GError                   **error);

void
sfcd_set_do_overwrite_confirmation (SandboxFileChooserDialog  *dialog,
                                    gboolean                   do_overwrite_confirmation,
                                    GError                   **error);

gboolean
sfcd_get_do_overwrite_confirmation (SandboxFileChooserDialog  *dialog,
                                    GError                   **error);

void
sfcd_set_create_folders            (SandboxFileChooserDialog  *dialog,
                                    gboolean                   create_folders,
                                    GError                   **error);

gboolean
sfcd_get_create_folders            (SandboxFileChooserDialog  *dialog,
                                    GError                   **error);

void
sfcd_set_current_name              (SandboxFileChooserDialog  *dialog,
                                    const gchar               *name,
                                    GError                   **error);

void
sfcd_set_filename                  (SandboxFileChooserDialog  *dialog,
                                    const gchar               *filename,
                                    GError                   **error);

void
sfcd_set_current_folder            (SandboxFileChooserDialog  *dialog,
                                    const gchar               *filename,
                                    GError                   **error);

void
sfcd_set_uri                       (SandboxFileChooserDialog  *dialog,
                                    const gchar               *uri,
                                    GError                   **error);

void
sfcd_set_current_folder_uri        (SandboxFileChooserDialog  *dialog,
                                    const gchar               *uri,
                                    GError                   **error);

gboolean
sfcd_add_shortcut_folder           (SandboxFileChooserDialog  *dialog,
                                    const gchar               *folder,
                                    GError                   **error);

gboolean
sfcd_remove_shortcut_folder        (SandboxFileChooserDialog  *dialog,
                                    const gchar               *folder,
                                    GError                   **error);

GSList *
sfcd_list_shortcut_folders         (SandboxFileChooserDialog  *dialog,
                                    GError                   **error);

gboolean
sfcd_add_shortcut_folder_uri       (SandboxFileChooserDialog  *dialog,
                                    const gchar               *uri,
                                    GError                   **error);

gboolean
sfcd_remove_shortcut_folder_uri    (SandboxFileChooserDialog  *dialog,
                                    const gchar               *uri,
                                    GError                   **error);

GSList *
sfcd_list_shortcut_folder_uris     (SandboxFileChooserDialog  *dialog,
                                    GError                   **error);


/* DATA RERIEVAL METHODS */
gchar *
sfcd_get_current_name       (SandboxFileChooserDialog   *dialog,
                             GError                    **error);

gchar *
sfcd_get_current_name       (SandboxFileChooserDialog   *dialog,
                             GError                    **error);

gchar *
sfcd_get_filename           (SandboxFileChooserDialog   *dialog,
                             GError                    **error);

GSList *
sfcd_get_filenames          (SandboxFileChooserDialog   *dialog,
                             GError                    **error);

gchar *
sfcd_get_current_folder     (SandboxFileChooserDialog   *dialog,
                             GError                    **error);

gchar *
sfcd_get_uri                (SandboxFileChooserDialog   *dialog,
                             GError                    **error);

GSList *
sfcd_get_uris               (SandboxFileChooserDialog   *dialog,
                             GError                    **error);

gchar *
sfcd_get_current_folder_uri (SandboxFileChooserDialog   *dialog,
                             GError                    **error);



/**
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
 * API CHANGE: make GtkFileFilter DBus-transportable, somehow
 *
 *   void	gtk_file_chooser_add_filter ()
 *   void	gtk_file_chooser_remove_filter ()
 *   GSList *	gtk_file_chooser_list_filters ()
 *   void	gtk_file_chooser_set_filter ()
 *   GtkFileFilter *	gtk_file_chooser_get_filter ()
 * _____________________________________________________________________________
 * API CHANGE: make GFile DBus-transportable, somehow -- or dump this
 *
 *   gboolean	gtk_file_chooser_set_current_folder_file ()
 *   gboolean	gtk_file_chooser_set_file ()
 *   GFile *	gtk_file_chooser_get_current_folder_file ()
 *   GFile *	gtk_file_chooser_get_file ()
 *   GSList *	gtk_file_chooser_get_files ()
 * _____________________________________________________________________________
 * API CHANGE: improving the override confirmation API
 *
 * Improve the override confirmation API by adding a method to generate a new
 * filename from the client's callback, with a scheme under the control of the
 * server (avoids arbitrary names).
 * _____________________________________________________________________________
**/


// TODO: propose a stricter API where state switches need to be made via a Reset
//       method
//
// TODO: having the dialog modal locally should cause the compositor
//       to handle it as a modal child of the client
//
// TODO check which signals are allowed
//GtkFileChooserConfirmation	confirm-overwrite	Run Last    // TODO implement
//void	current-folder-changed	Run Last                    // Not exported, forbidden info
//void	file-activated	Run Last                            // Not exported, "run-finished" instead
//void	selection-changed	Run Last                          // Not exported, forbidden info
//void	update-preview	Run Last                            // TODO new architecture for widget preview
//    GtkDialog: RESPONSE
//    GtkWidget: ??? (TODO make list)


G_END_DECLS

#endif /* __SANDBOX_FILE_CHOOSER_DIALOG_H__ */
