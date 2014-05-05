/*
 * localfilechooserdialog.h: locally-running SandboxFileChooserDialog
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

#ifndef __LOCAL_FILE_CHOOSER_DIALOG_H__
#define __LOCAL_FILE_CHOOSER_DIALOG_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "sandboxutilscommon.h"
#include "sandboxfilechooserdialog.h"

G_BEGIN_DECLS



#define LOCAL_TYPE_FILE_CHOOSER_DIALOG            (lfcd_get_type ())
#define LOCAL_FILE_CHOOSER_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), LOCAL_TYPE_FILE_CHOOSER_DIALOG, LocalFileChooserDialog))
#define LOCAL_IS_FILE_CHOOSER_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LOCAL_TYPE_FILE_CHOOSER_DIALOG))
#define LOCAL_FILE_CHOOSER_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), LOCAL_TYPE_FILE_CHOOSER_DIALOG, LocalFileChooserDialogClass))
#define LOCAL_IS_FILE_CHOOSER_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LOCAL_TYPE_FILE_CHOOSER_DIALOG))
#define LOCAL_FILE_CHOOSER_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), LOCAL_TYPE_FILE_CHOOSER_DIALOG, LocalFileChooserDialogClass))

typedef struct _LocalFileChooserDialog        LocalFileChooserDialog;
typedef struct _LocalFileChooserDialogClass   LocalFileChooserDialogClass;
typedef struct _LocalFileChooserDialogPrivate LocalFileChooserDialogPrivate;

struct _LocalFileChooserDialog
{
  SandboxFileChooserDialog          parent_instance;
  LocalFileChooserDialogPrivate    *priv;
};

struct _LocalFileChooserDialogClass
{
  SandboxFileChooserDialogClass parent_class;
};

GType lfcd_get_type (void);

SandboxFileChooserDialog *
lfcd_new_valist (const gchar          *title,
                 const gchar          *parentWinId,
                 GtkWindow            *parent,
                 GtkFileChooserAction  action,
                 const gchar          *first_button_text,
                 va_list               varargs);

SandboxFileChooserDialog *
lfcd_new_variant (const gchar          *title,
                  const gchar          *parentWinId,
                  GtkWindow            *parent,
                  GtkFileChooserAction  action,
                  GVariant             *button_list);

SandboxFileChooserDialog *
lfcd_new (const gchar          *title,
          GtkWindow            *parent,
          GtkFileChooserAction  action,
          const gchar          *first_button_text,
          ...);

SandboxFileChooserDialog *
lfcd_new_with_remote_parent (const gchar          *title,
                             const gchar          *parentWinId,
                             GtkFileChooserAction  action,
                             const gchar          *first_button_text,
                             ...);

G_END_DECLS

#endif /* __LOCAL_FILE_CHOOSER_DIALOG_H__ */
