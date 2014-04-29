/* SandboxUtils -- Sandbox Utils Daemon
 * Copyright (c) Steve Dodier-Lazaro <sidnioulz@gmail.com>, 2014
 * 
 * Under GPLv3
 * 
 *** 
 * 
 * main.c: ....
 */
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib/gi18n.h>

#include <systemd/sd-daemon.h>

#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#ifndef NDEBUG
#include <mcheck.h>
#endif

#ifndef G_OS_UNIX
#error UNIX only (and most likely Linux only)
#endif


#include "sandboxutilscommon.h"
#include "sandboxfilechooserdialog.h"
#include "sandboxfilechooserdialogdbuswrapper.h"


static gboolean
watchdog_func (gpointer data)
{
  sd_notify(0, "WATCHDOG=1");

  return TRUE;
}

static void
signal_manager (int signal)
{
  syslog (LOG_DEBUG, NAME" received SIGINT, now shutting down...\n");

  //TODO clean up memory, etc

  exit(EXIT_SUCCESS);
}

int
main (int argc, char *argv[])
{
  // Internal to the server
  GMainLoop           *loop;
	SandboxUtilsClient  *cli;
	SfcdDbusWrapperInfo *sfcd_wrapper;
	struct sigaction     action;
	
#ifndef NDEBUG
  mtrace ();
#endif

  // Initialise GTK for later
  gtk_init (&argc, &argv);

	// Intercept signals
	memset (&action, 0, sizeof (struct sigaction));
  action.sa_handler = signal_manager;
  sigaction (SIGINT, &action, NULL);

  // Open log
  openlog (NAME, LOG_PID | LOG_CONS | LOG_PERROR, LOG_USER);
  syslog (LOG_INFO, "Starting "NAME" version "VERSION"\n");

  // Initialise the interface providing SandboxFileChooserDialog
  sfcd_wrapper = sfcd_dbus_wrapper_dbus_init ();

	// Temporary client
	cli = _get_client ();
	
  // Notify systemd of readiness and start the loop
  loop = g_main_loop_new (NULL, FALSE);
  g_timeout_add_seconds (10, watchdog_func, NULL);
  sd_notify(0, "READY=1");
  g_main_loop_run (loop);

  // Clean up the client
  _reset_client ();

  // Close the SandboxFileChooserDialog interface
  // sfcd_dbus_wrapper_dbus_shutdown (sfcd_wrapper);
  // XXX this might be done automatically, need to check before calling

  // Shutdown the syslog log
  closelog ();

  return EXIT_SUCCESS;
}
