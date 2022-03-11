/*
 * osso-abook-get-your-contacts-dialog.h
 *
 * Copyright (C) 2022 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef OSSOABOOKGETYOURCONTACTSDIALOG_H
#define OSSOABOOKGETYOURCONTACTSDIALOG_H

#include <gtk/gtk.h>

#include "app.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_GET_YOUR_CONTACTS_DIALOG \
                (osso_abook_get_your_contacts_dialog_get_type ())
#define OSSO_ABOOK_GET_YOUR_CONTACTS_DIALOG(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_GET_YOUR_CONTACTS_DIALOG, \
                 OssoABookGetYourContactsDialog))
#define OSSO_ABOOK_GET_YOUR_CONTACTS_DIALOG_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_GET_YOUR_CONTACTS_DIALOG, \
                 OssoABookGetYourContactsDialogClass))
#define OSSO_ABOOK_IS_GET_YOUR_CONTACTS_DIALOG(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_GET_YOUR_CONTACTS_DIALOG))
#define OSSO_ABOOK_IS_GET_YOUR_CONTACTS_DIALOG_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_GET_YOUR_CONTACTS_DIALOG))
#define OSSO_ABOOK_GET_YOUR_CONTACTS_DIALOG_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_GET_YOUR_CONTACTS_DIALOG, \
                 OssoABookGetYourContactsDialogClass))

typedef struct _OssoABookGetYourContactsDialogClass OssoABookGetYourContactsDialogClass;
typedef struct _OssoABookGetYourContactsDialog OssoABookGetYourContactsDialog;

struct _OssoABookGetYourContactsDialogClass
{
  GtkDialogClass parent_class;
};

struct _OssoABookGetYourContactsDialog
{
  GtkDialog parent;
};

GType
osso_abook_get_your_contacts_dialog_get_type(void) G_GNUC_CONST;


gboolean osso_abook_get_your_contacts_dialog_already_shown();
GtkWidget *osso_abook_get_your_contacts_dialog_new(osso_abook_data *data);

G_END_DECLS

#endif // OSSOABOOKGETYOURCONTACTSDIALOG_H
