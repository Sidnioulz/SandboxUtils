/* SandboxUtils -- SandboxFileChooserDialog DBus Wrapper
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
#ifndef _SFCD_DBUS_WRAPPER_H
#define _SFCD_DBUS_WRAPPER_H

#include <gio/gio.h>
#include "sandboxutilscommon.h"
#include "sandboxutilsclientmanager.h"
#include "sandboxfilechooserdialog.h"


typedef struct {
  guint           owner_id;
  guint           registration_id;
  GDBusNodeInfo  *introspection_data;
} SfcdDbusWrapperInfo;


/*** SandboxFileChooserDialog part of wrapper ***/

gboolean
sfcd_dbus_wrapper_sfcd_new (SandboxUtilsClient *cli,
                            GVariant           *parameters,
                            gchar             **dialog_id,
                            GError            **error);

gboolean
sfcd_dbus_wrapper_sfcd_destroy (SandboxUtilsClient *cli,
                                GVariant           *parameters,
                                GError            **error);

gboolean
sfcd_dbus_wrapper_sfcd_get_state (SandboxUtilsClient *cli,
                                  GVariant           *parameters,
                                  gint32             *state,
                                  GError            **error);

gboolean
sfcd_dbus_wrapper_sfcd_run (SandboxUtilsClient *cli,
                            GVariant           *parameters,
                            GError            **error);

gboolean
sfcd_dbus_wrapper_sfcd_present (SandboxUtilsClient *cli,
                                GVariant           *parameters,
                                GError            **error);

gboolean
sfcd_dbus_wrapper_sfcd_cancel_run (SandboxUtilsClient *cli,
                                   GVariant           *parameters,
                                   GError            **error);

gboolean
sfcd_dbus_wrapper_sfcd_set_action (SandboxUtilsClient *cli,
                                   GVariant           *parameters,
                                   GError            **error);

gboolean
sfcd_dbus_wrapper_sfcd_get_action (SandboxUtilsClient *cli,
                                   GVariant           *parameters,
                                   gint32             *action,
                                   GError            **error);

gboolean
sfcd_dbus_wrapper_sfcd_set_local_only (SandboxUtilsClient *cli,
                                       GVariant           *parameters,
                                       GError            **error);

gboolean
sfcd_dbus_wrapper_sfcd_get_local_only (SandboxUtilsClient *cli,
                                       GVariant           *parameters,
                                       gboolean           *local_only,
                                       GError            **error);
//TODO rest of API










/*** DBus part of wrapper ***/

SfcdDbusWrapperInfo *
sfcd_dbus_wrapper_dbus_init ();

void
sfcd_dbus_wrapper_dbus_shutdown (gpointer data);

#endif /* #ifndef _SFCD_DBUS_WRAPPER_H */
