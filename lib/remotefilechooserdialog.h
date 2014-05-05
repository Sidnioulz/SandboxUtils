/*
 * remotefilechooserdialog.h: SandboxFileChooserDialog over GDBus
 *
 * Copyright (C) 2014 Steve Dodier-Lazaro <sidnioulz@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Steve Dodier-Lazaro <sidnioulz@gmail.com>
 */

#ifndef __REMOTE_FILE_CHOOSER_DIALOG_H__
#define __REMOTE_FILE_CHOOSER_DIALOG_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "sandboxutilscommon.h"
#include "sandboxfilechooserdialog.h"

G_BEGIN_DECLS



#define REMOTE_TYPE_FILE_CHOOSER_DIALOG            (rfcd_get_type ())
#define REMOTE_FILE_CHOOSER_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), REMOTE_TYPE_FILE_CHOOSER_DIALOG, RemoteFileChooserDialog))
#define REMOTE_IS_FILE_CHOOSER_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), REMOTE_TYPE_FILE_CHOOSER_DIALOG))
#define REMOTE_FILE_CHOOSER_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), REMOTE_TYPE_FILE_CHOOSER_DIALOG, RemoteFileChooserDialogClass))
#define REMOTE_IS_FILE_CHOOSER_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), REMOTE_TYPE_FILE_CHOOSER_DIALOG))
#define REMOTE_FILE_CHOOSER_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), REMOTE_TYPE_FILE_CHOOSER_DIALOG, RemoteFileChooserDialogClass))

typedef struct _RemoteFileChooserDialog        RemoteFileChooserDialog;
typedef struct _RemoteFileChooserDialogClass   RemoteFileChooserDialogClass;
typedef struct _RemoteFileChooserDialogPrivate RemoteFileChooserDialogPrivate;

struct _RemoteFileChooserDialog
{
  SandboxFileChooserDialog          parent_instance;
  RemoteFileChooserDialogPrivate   *priv;
};

struct _RemoteFileChooserDialogClass
{
  SandboxFileChooserDialogClass parent_class;
  
  GDBusProxy *proxy;
};

GType rfcd_get_type (void);

SandboxFileChooserDialog *
rfcd_new_valist (const gchar          *title,
                 GtkWindow            *parent,
                 GtkFileChooserAction  action,
                 const gchar          *first_button_text,
                 va_list               varargs);

SandboxFileChooserDialog *
rfcd_new (const gchar          *title,
          GtkWindow            *parent,
          GtkFileChooserAction  action,
          const gchar          *first_button_text,
          ...);

G_END_DECLS

#endif /* __REMOTE_FILE_CHOOSER_DIALOG_H__ */
