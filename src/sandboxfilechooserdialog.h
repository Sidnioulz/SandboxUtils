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
gboolean
sfcd_set_action (SandboxFileChooserDialog  *self,
                 gint32                     action,
                 GError                   **error);

gboolean
sfcd_get_action (SandboxFileChooserDialog  *self,
                 gint32                    *result,
                 GError                   **error);

//TODO rest of the GtkFileChooserDialog API -- should be quick!

#endif /* __SANDBOX_FILE_CHOOSER_DIALOG_H__ */
