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

#define NAME "Sandbox Utils"
#define VERSION "0.5.0"

#define SANDBOXUTILS_IFACE "org.mupuf.SandboxUtils"
#define SANDBOXUTILS_PATH "/org/mupuf/SandboxUtils"

#define _B(foo) (foo? "true":"false")

#include <glib.h>
const gchar *
g_error_get_message (GError *err);

const int
min (int a, int b);

const int
max (int a, int b);

#endif /* __SANDBOX_UTILS_COMMON_H__ */
