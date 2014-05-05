/* SandboxUtils -- Sandbox Utils common functions
 * Copyright (c) Steve Dodier-Lazaro <sidnioulz@gmail.com>, 2014
 * 
 * Under GPLv3
 * 
 *** 
 * 
 * TODO
 */

#include "sandboxutilscommon.h"

const gchar *
g_error_get_message (GError *err)
{
  if (err)
    return err->message;
  else
    return "(no error)";
}

const int
min (int a, int b)
{
  return a<b? a:b;
}

const int
max (int a, int b)
{
  return a>b? a:b;
}
