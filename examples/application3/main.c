#include <gtk/gtk.h>
#include <exampleapp.h>
#include <sandboxutils.h>
#include <syslog.h>

int
main (int argc, char *argv[])
{
        openlog ("SandboxUtils Example App", LOG_PID | LOG_CONS | LOG_PERROR, LOG_USER);
        sandboxutils_init (&argc, &argv);
        return g_application_run (G_APPLICATION (example_app_new ()), argc, argv);
}
