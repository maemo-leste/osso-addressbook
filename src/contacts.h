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

#include <libosso-abook/osso-abook-menu-extension.h>
#include <rtcom-eventlogger/eventlogger-query.h>

void
set_contacts_mode(osso_abook_data *data, int mode);

void
scroll_to_top_if_needed(osso_abook_data *data);

void
merge(osso_abook_data *data, const char *uid);

OssoABookContact *
get_starter_contact(osso_abook_data *data);

void
open_delete_contacts_view_window(osso_abook_data *data);

#endif // CONTACTS_H
