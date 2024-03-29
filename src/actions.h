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

#include "app.h"

#include <rtcom-eventlogger-ui/rtcom-log-model.h>
#include <rtcom-eventlogger-ui/rtcom-log-view.h>
#include <libebook-contacts/libebook-contacts.h>

RTComElQuery *
create_communication_history_query(OssoABookContact *, RTComEl *);

gboolean
contact_saved_cb(GtkWidget *editor, const char *uid, osso_abook_data *data);

/* contact_menu_actions declarations */
#define CONTACT_MENU_COUNT 8
extern OssoABookMenuEntry contact_menu_actions[CONTACT_MENU_COUNT];

#define BT_MENU_COUNT 4
extern OssoABookMenuEntry sim_bt_menu_actions[BT_MENU_COUNT];

#define MAIN_MENU_COUNT 7
extern OssoABookMenuEntry main_menu_actions[MAIN_MENU_COUNT];

void
switch_to_group_subview(osso_abook_data *data, OssoABookGroup *group);

void
view_group_subview(osso_abook_data *data, OssoABookGroup *group);

#endif // ACTIONS_H
