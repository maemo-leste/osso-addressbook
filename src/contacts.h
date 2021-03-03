/*
 * contacts.h
 *
 * Copyright (C) 2021 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
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

#ifndef CONTACTS_H
#define CONTACTS_H

void set_contacts_mode(osso_abook_data *data, int mode);
void scroll_to_top_if_needed(osso_abook_data *data);

typedef struct _OssoABookGetYourContactsDialog OssoABookGetYourContactsDialog;
typedef struct _OssoABookGetYourContactsDialogClass OssoABookGetYourContactsDialogClass;

GtkWidget *osso_abook_get_your_contacts_dialog_new(osso_abook_data *data);
gboolean osso_abook_get_your_contacts_dialog_already_shown();

#endif // CONTACTS_H
