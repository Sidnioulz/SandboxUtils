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
#include "sandboxfilechooserdialogdbusobject.h"


typedef struct {
  guint                owner_id;
  SfcdDbusWrapper     *interface;
  SandboxUtilsClient  *client;
} SfcdDbusWrapperInfo;


//TODO move to sandboxutilsdbus.h

SfcdDbusWrapperInfo *
sfcd_dbus_wrapper_dbus_init ();

void
sfcd_dbus_wrapper_dbus_shutdown (gpointer data);

#endif /* #ifndef _SFCD_DBUS_WRAPPER_H */
