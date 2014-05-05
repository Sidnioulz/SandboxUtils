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
#include <string.h>

#include "sandboxutilsclientmanager.h"

//TODO connect/disconnect methods where a client introduces themselves by giving
// access to their STDOUT and STDERR fds. Later we'll use that to help them log
// what happens to their calls to sandboxutilsd.

static SandboxUtilsClient *
sandbox_utils_client_new ()
{
  SandboxUtilsClient *cli = g_malloc (sizeof (SandboxUtilsClient));
  memset (cli, 0, sizeof (SandboxUtilsClient));

  cli->dialogs = g_hash_table_new (g_str_hash, g_str_equal);
  g_mutex_init (&cli->dialogsMutex);

  return cli;
}

static void
sandbox_utils_client_destroy (SandboxUtilsClient *cli)
{
  //TODO verify the client was disconnected properly

  g_mutex_clear (&cli->dialogsMutex);

  if (cli->dialogs)
    g_hash_table_unref (cli->dialogs);

  g_free (cli);
}


static SandboxUtilsClient **
__get_client ()
{
  static SandboxUtilsClient *cli = NULL;

  return &cli;
}

SandboxUtilsClient *
_get_client ()
{
  SandboxUtilsClient **cli = __get_client ();

  if (!*cli)
    *cli = sandbox_utils_client_new ();

  return *cli;
}

void
_reset_client ()
{
  SandboxUtilsClient **cli = __get_client ();

  if (*cli)
  {
    sandbox_utils_client_destroy (*cli);
    *cli = NULL;
  }
}
