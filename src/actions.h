/*
 * actions.h
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

#ifndef ACTIONS_H
#define ACTIONS_H

#include <libosso-abook/osso-abook-menu-extension.h>

RTComElQuery *create_communication_history_query(OssoABookContact *, RTComEl *);

/* contact_menu_actions declarations */
#define CONTACT_MENU_COUNT 8
extern OssoABookMenuEntry contact_menu_actions[CONTACT_MENU_COUNT];
static void contact_starter_edit_cb(int unused, osso_abook_data *data);
static void contact_send_card_cb(int unused, osso_abook_data *data);
static void contact_send_detail_cb(int unused, osso_abook_data *data);
static void contact_merge_cb(int unused, osso_abook_data *data);
static void contact_starter_delete_cb(int unused, osso_abook_data *data);
static void contact_create_shortcut_cb(int unused, osso_abook_data *data);
static void contact_request_authorization_cb(int unused, osso_abook_data *data);
static void contact_communication_history_cb(int unused, osso_abook_data *data);

#endif // ACTIONS_H
