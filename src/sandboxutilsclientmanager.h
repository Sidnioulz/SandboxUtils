/* SandboxUtils -- Sandbox Utilities Client Manager
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
#ifndef _SANDBOX_UTILS_CLIENT_H
#define _SANDBOX_UTILS_CLIENT_H

// TODO manage multiple clients via sub-processes (so a client cannot crash sfad)
// TODO error domain https://developer.gnome.org/gio/2.28/GDBusError.html
// TODO check what apps in the wild typically call after run()

#include <gio/gio.h>
#include "sandboxfilechooserdialog.h"


/* Hello there, client */
typedef struct _SandboxUtilsClient
{
  GHashTable            *dialogs;
  const guint32          ownLimits;
  const guint32          runLimits;   
  GMutex                 dialogsMutex;
} SandboxUtilsClient;


// Temporary client -- later we'll use a hash table based on client IDs
SandboxUtilsClient *
_get_client ();

void
_reset_client ();


#endif /* #ifndef _SANDBOX_UTILS_CLIENT_H */
