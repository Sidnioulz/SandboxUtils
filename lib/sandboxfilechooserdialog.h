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
 * @SFCD_LAST_STATE: Should never occur. Equivalent to %SFCD_WRONG_STATE.
 *
 * Describes the current state of a #SandboxFileChooserDialog.
 */
typedef enum {
  SFCD_WRONG_STATE     = 0, /* Unused, only to have a fallback state */
  SFCD_CONFIGURATION   = 1,
  SFCD_RUNNING         = 2,
  SFCD_DATA_RETRIEVAL  = 3,
  SFCD_LAST_STATE      = 4,
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

struct _SandboxFileChooserDialog
{
  GObject                             parent_instance;
};

struct _SandboxFileChooserDialogClass
{
  GObjectClass parent_class;

  /* Pointers to virtual methods implemented by local and remote dialogs */
  //TODO get/set_dialog_title
  void                 (*destroy)                       (SandboxFileChooserDialog *);
  SfcdState            (*get_state)                     (SandboxFileChooserDialog *);
  const gchar *        (*get_state_printable)           (SandboxFileChooserDialog *);
  const gchar *        (*get_dialog_title)              (SandboxFileChooserDialog *);
  gboolean             (*is_running)                    (SandboxFileChooserDialog *);
  const gchar *        (*get_id)                        (SandboxFileChooserDialog *);
  void                 (*run)                           (SandboxFileChooserDialog *, GError **);
  void                 (*present)                       (SandboxFileChooserDialog *, GError **);
  void                 (*cancel_run)                    (SandboxFileChooserDialog *, GError **);
  void                 (*set_action)                    (SandboxFileChooserDialog *, GtkFileChooserAction, GError **);
  GtkFileChooserAction (*get_action)                    (SandboxFileChooserDialog *, GError **);
  void                 (*set_local_only)                (SandboxFileChooserDialog *, gboolean, GError **);
  gboolean             (*get_local_only)                (SandboxFileChooserDialog *, GError **);
  void                 (*set_select_multiple)           (SandboxFileChooserDialog *, gboolean, GError **);
  gboolean             (*get_select_multiple)           (SandboxFileChooserDialog *, GError **);
  void                 (*set_show_hidden)               (SandboxFileChooserDialog *, gboolean, GError **);
  gboolean             (*get_show_hidden)               (SandboxFileChooserDialog *, GError **);
  void                 (*set_do_overwrite_confirmation) (SandboxFileChooserDialog *, gboolean, GError **);
  gboolean             (*get_do_overwrite_confirmation) (SandboxFileChooserDialog *, GError **);
  void                 (*set_create_folders)            (SandboxFileChooserDialog *, gboolean, GError **);
  gboolean             (*get_create_folders)            (SandboxFileChooserDialog *, GError **);
  void                 (*set_current_name)              (SandboxFileChooserDialog *, const gchar *, GError **);
  void                 (*set_filename)                  (SandboxFileChooserDialog *, const gchar *, GError **);
  void                 (*set_current_folder)            (SandboxFileChooserDialog *, const gchar *, GError **);
  void                 (*set_uri)                       (SandboxFileChooserDialog *, const gchar *, GError **);
  void                 (*set_current_folder_uri)        (SandboxFileChooserDialog *, const gchar *, GError **);
  gboolean             (*add_shortcut_folder)           (SandboxFileChooserDialog *, const gchar *, GError **);
  gboolean             (*remove_shortcut_folder)        (SandboxFileChooserDialog *, const gchar *, GError **);
  GSList *             (*list_shortcut_folders)         (SandboxFileChooserDialog *, GError **);
  gboolean             (*add_shortcut_folder_uri)       (SandboxFileChooserDialog *, const gchar *, GError **);
  gboolean             (*remove_shortcut_folder_uri)    (SandboxFileChooserDialog *, const gchar *, GError **);
  GSList *             (*list_shortcut_folder_uris)     (SandboxFileChooserDialog *, GError **);
  gchar *              (*get_current_name)              (SandboxFileChooserDialog *, GError **);
  gchar *              (*get_filename)                  (SandboxFileChooserDialog *, GError **);
  GSList *             (*get_filenames)                 (SandboxFileChooserDialog *, GError **);
  gchar *              (*get_current_folder)            (SandboxFileChooserDialog *, GError **);
  gchar *              (*get_uri)                       (SandboxFileChooserDialog *, GError **);
  GSList *             (*get_uris)                      (SandboxFileChooserDialog *, GError **);
  gchar *              (*get_current_folder_uri)        (SandboxFileChooserDialog *, GError **);


  /* Class signals */
  //TODO whole signal thing
  guint destroy_signal;
  guint hide_signal;
  guint response_signal;
  guint show_signal;
};

GType sfcd_get_type (void);

gboolean sfcd_is_accept_label (const gchar *label);

/* GENERIC METHODS */
SandboxFileChooserDialog *
sfcd_new (const gchar *title,
          GtkWindow *parent,
          GtkFileChooserAction action,
          const gchar *first_button_text,
          ...);

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

const gchar *
sfcd_get_id               (SandboxFileChooserDialog *dialog);


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
 * API CHANGE: Response ID to influence state
 *
 * Define response IDs that allow a data retrieval state. Enforce stock
 * labels (that are HIG-compliant) for these response IDs to avoid the user
 * cancelling and the app still getting access.
 *
 *   gtk_file_chooser_run ()
 * _____________________________________________________________________________
 * API CHANGE: Select at least X items
 *
 * Add flag to make sure the user selects multiple items, if needed for a diff
 * like app.
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
//GtkFileChooserConfirmation	confirm-overwrite	Run Last    // TODO implement differently
//void	update-preview	Run Last                            // TODO new architecture for widget preview

/* 

The “focus-out-event” signal
gboolean
user_function (GtkWidget *widget,
               GdkEvent  *event,
               gpointer   user_data)
The ::focus-out-event signal will be emitted when the keyboard focus leaves the widget 's window.

To receive this signal, the GdkWindow associated to the widget needs to enable the GDK_FOCUS_CHANGE_MASK mask.

The “window-state-event” signal
gboolean
user_function (GtkWidget *widget,
               GdkEvent  *event,
               gpointer   user_data)
The ::window-state-event will be emitted when the state of the toplevel window associated to the widget changes.

To receive this signal the GdkWindow associated to the widget needs to enable the GDK_STRUCTURE_MASK mask. GDK will enable this mask automatically for all new windows.

*/


//FIXME remove this hack ASAP
void __temp_sandboxutils_init (gboolean);

G_END_DECLS

#endif /* __SANDBOX_FILE_CHOOSER_DIALOG_H__ */
