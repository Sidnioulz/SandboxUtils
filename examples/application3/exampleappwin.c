#include "exampleapp.h"
#include "exampleappwin.h"
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <sandboxutils.h>

struct ExampleAppWindow {
        GtkApplicationWindow parent;
};

struct ExampleAppWindowClass {
        GtkApplicationWindowClass parent_class;
};

typedef struct ExampleAppWindowPrivate ExampleAppWindowPrivate;

struct ExampleAppWindowPrivate {
        GtkWidget *header;
        GtkWidget *stack;
        GtkWidget *open_button;
        GtkWidget *save_button;
        GtkWidget *cancel_button;
        GtkWidget *destroy_button;
};

G_DEFINE_TYPE_WITH_PRIVATE(ExampleAppWindow, example_app_window, GTK_TYPE_APPLICATION_WINDOW);

static SandboxFileChooserDialog *dialog;
static SandboxFileChooserDialog *savedialog;

static void
on_response (SandboxFileChooserDialog *sfcd,
             gint response_id,
             gint state,
             gpointer user_data)
{
  ExampleAppWindow *win = (ExampleAppWindow *) user_data;

  g_printf ("Response: %d\nState: %d\n", response_id, state);

  if (state == SFCD_DATA_RETRIEVAL && response_id == GTK_RESPONSE_OK)
  {
    GError *error = NULL;
    gchar *path = sfcd_get_filename (dialog, &error);
    if (error)
    {
      g_printf ("Get Filename Error: %s\n", error->message);
      g_error_free (error); //TODO
    }
    else
    {
      GFile *file = g_file_new_for_path (path);
      example_app_window_open (win, file);
      g_free (path);
    }
  }
}

static void
on_destroy (SandboxFileChooserDialog *sfcd,
             gpointer user_data)
{
  g_printf ("Open dialog was destroyed, setting to NULL.\n");
  dialog = NULL;
}

static void
on_open_clicked (GtkButton *button,
                 gpointer   user_data)
{
        ExampleAppWindow *win = (ExampleAppWindow *) user_data;
        if (!dialog)
        {
          dialog = sfcd_new ("Sandbox-Compatible Open",
                             GTK_WINDOW (win),
                             GTK_FILE_CHOOSER_ACTION_OPEN,
                             "Open", GTK_RESPONSE_OK,
                             "Cancel", GTK_RESPONSE_CANCEL,
                             NULL);
        }

        else if (sfcd_is_running (dialog))
        {
          GError *error = NULL;
          sfcd_present (dialog, &error);
          if (error)
          {
            g_printf ("Present Error: %s\n", error->message);
            g_error_free (error); //TODO
          }
          return;
        }

        if (dialog)
        {
          g_signal_connect (dialog, "response", (GCallback) on_response, win);
          g_signal_connect (dialog, "destroy", (GCallback) on_destroy, win);

          GError *error = NULL;
          sfcd_run (dialog, &error);
          if (error)
          {
            g_printf ("Run Error: %s\n", error->message);
            g_error_free (error); //TODO
          }
        }
        else
            g_printf ("%s", "Could not create a dialog.\n");
}

static void
on_save_response (SandboxFileChooserDialog *sfcd,
                  gint response_id,
                  gint state,
                  gpointer user_data)
{
  ExampleAppWindow *win = (ExampleAppWindow *) user_data;

  g_printf ("Response: %d\nState: %d\n", response_id, state);

  if (state == SFCD_DATA_RETRIEVAL && response_id == GTK_RESPONSE_OK)
  {
    GError *error = NULL;
    gchar *path = sfcd_get_filename (savedialog, &error);
    if (error)
    {
      g_printf ("Get Filename Error: %s\n", error->message);
      g_error_free (error); //TODO
    }
    else
    {
      g_printf ("TODO: get the current file and save it at '%s'\n", path);
      g_free (path);
    }
  }
}

static void
on_save_destroy (SandboxFileChooserDialog *sfcd,
                 gpointer user_data)
{
  g_printf ("Save dialog was destroyed, setting to NULL.\n");
  savedialog = NULL;
}

static void
on_save_clicked (GtkButton *button,
                 gpointer   user_data)
{
        if (!savedialog)
        {
          ExampleAppWindow *win = (ExampleAppWindow *) user_data;
          savedialog = sfcd_new ("Sandbox-Compatible Save",
                                 GTK_WINDOW (win),
                                 GTK_FILE_CHOOSER_ACTION_SAVE,
                                 "Save", GTK_RESPONSE_OK,
                                 "Cancel", GTK_RESPONSE_CANCEL,
                                 NULL);

          if (savedialog)
          {
            g_signal_connect (savedialog, "response", (GCallback) on_save_response, win);
            g_signal_connect (savedialog, "destroy", (GCallback) on_save_destroy, win);

            GError *error = NULL;
            sfcd_set_do_overwrite_confirmation (savedialog, TRUE, &error);
            if (error)
            {
              g_printf ("Do Confirm Overwrite Error: %s\n", error->message);
              g_error_free (error); //TODO
            }
          }
        }

        else if (sfcd_is_running (savedialog))
        {
          GError *error = NULL;
          sfcd_present (savedialog, &error);
          if (error)
          {
            g_printf ("Present Error: %s\n", error->message);
            g_error_free (error); //TODO
          }
          return;
        }

        if (savedialog)
        {
          GError *error = NULL;
          sfcd_run (savedialog, &error);
          if (error)
          {
            g_printf ("Run Error: %s\n", error->message);
            g_error_free (error); //TODO
          }
        }
        else
            g_printf ("%s", "Could not create a dialog.\n");
}

static void
on_cancel_clicked (GtkButton *button,
                   gpointer   user_data)
{
        if (dialog)
        {
          GError *error = NULL;
          sfcd_cancel_run (dialog, &error);
          if (error)
          {
            g_printf ("Cancel Error: %s\n", error->message);
            g_error_free (error); //TODO
          }
        }

        if (savedialog)
        {
          GError *error = NULL;
          sfcd_cancel_run (savedialog, &error);
          if (error)
          {
            g_printf ("Cancel Error: %s\n", error->message);
            g_error_free (error); //TODO
          }
        }
}

static void
on_destroy_clicked (GtkButton *button,
                   gpointer   user_data)
{
        if (dialog)
        {
          sfcd_destroy (dialog);
        }

        if (savedialog)
        {
          sfcd_destroy (savedialog);
        }
}

static void
example_app_window_init (ExampleAppWindow *win)
{
        gtk_widget_init_template (GTK_WIDGET (win));

        ExampleAppWindowPrivate *priv = example_app_window_get_instance_private (win);
        gtk_header_bar_set_decoration_layout (GTK_HEADER_BAR (priv->header), ":");


        priv->open_button = gtk_button_new_from_icon_name ("document-open-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_widget_set_valign (priv->open_button, GTK_ALIGN_CENTER);

        gtk_header_bar_pack_start (GTK_HEADER_BAR (priv->header), priv->open_button);
        gtk_widget_show (priv->open_button);
        g_signal_connect (priv->open_button, "clicked", (GCallback) on_open_clicked, win);


        priv->save_button = gtk_button_new_from_icon_name ("document-save-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_widget_set_valign (priv->save_button, GTK_ALIGN_CENTER);

        gtk_header_bar_pack_start (GTK_HEADER_BAR (priv->header), priv->save_button);
        gtk_widget_show (priv->save_button);
        g_signal_connect (priv->save_button, "clicked", (GCallback) on_save_clicked, win);


        priv->cancel_button = gtk_button_new_from_icon_name ("process-error-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_widget_set_valign (priv->cancel_button, GTK_ALIGN_CENTER);

        gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->header), priv->cancel_button);
        gtk_widget_show (priv->cancel_button);
        g_signal_connect (priv->cancel_button, "clicked", (GCallback) on_cancel_clicked, win);


        priv->destroy_button = gtk_button_new_from_icon_name ("edit-delete-symbolic", GTK_ICON_SIZE_BUTTON);
        gtk_widget_set_valign (priv->destroy_button, GTK_ALIGN_CENTER);

        gtk_header_bar_pack_end (GTK_HEADER_BAR (priv->header), priv->destroy_button);
        gtk_widget_show (priv->destroy_button);
        g_signal_connect (priv->destroy_button, "clicked", (GCallback) on_destroy_clicked, win);
}

static void
example_app_window_class_init (ExampleAppWindowClass *class)
{
        gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                                     "/org/gtk/exampleapp/window.ui");
        gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class), ExampleAppWindow, header);
        gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (class), ExampleAppWindow, stack);
}

ExampleAppWindow *
example_app_window_new (ExampleApp *app)
{
        return g_object_new (EXAMPLE_APP_WINDOW_TYPE, "application", app, NULL);
}

void
example_app_window_open (ExampleAppWindow *win,
                         GFile            *file)
{
        ExampleAppWindowPrivate *priv = example_app_window_get_instance_private (win);
        gchar *basename;
        GtkWidget *scrolled, *view;
        gchar *contents;
        gsize length;

        basename = g_file_get_basename (file);

        scrolled = gtk_scrolled_window_new (NULL, NULL);
        gtk_widget_show (scrolled);
        gtk_widget_set_hexpand (scrolled, TRUE);
        gtk_widget_set_vexpand (scrolled, TRUE);
        view = gtk_text_view_new ();
        gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
        gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
        gtk_widget_show (view);
        gtk_container_add (GTK_CONTAINER (scrolled), view);
        gtk_stack_add_titled (GTK_STACK (priv->stack), scrolled, basename, basename);

        if (g_file_load_contents (file, NULL, &contents, &length, NULL, NULL)) {
                GtkTextBuffer *buffer;

                buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
                gtk_text_buffer_set_text (buffer, contents, length);
                g_free (contents);
        }

        g_free (basename);
}
