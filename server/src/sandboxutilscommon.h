/* SandboxUtils -- Sandbox Utils common variables
 * Copyright (c) Steve Dodier-Lazaro <sidnioulz@gmail.com>, 2014
 * 
 * Under GPLv3
 * 
 *** 
 * 
 * TODO
 */
#ifndef __SANDBOX_UTILS_COMMON_H__
#define __SANDBOX_UTILS_COMMON_H__

#define NAME "sandboxutilsd"
#define VERSION "0.4"

#define SANDBOXUTILS_IFACE "org.mupuf.SandboxUtils"
#define SANDBOXUTILS_PATH "/org/mupuf/SandboxUtils"

#define _B(foo) (foo? "true":"false")

inline const gchar *
g_error_get_message (GError *err)
{
  if (err)
    return err->message;
  else
    return "(no error)";
}

#endif /* __SANDBOX_UTILS_COMMON_H__ */
