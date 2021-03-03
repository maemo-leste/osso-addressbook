/*
 * menu.h
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

#ifndef MENU_H
#define MENU_H

#include <libosso-abook/osso-abook-menu-extension.h>

#define MENU_ACTIONS_COUNT 7

extern OssoABookMenuEntry main_menu_actions[MENU_ACTIONS_COUNT];
extern OssoABookMenuEntry main_menu_filters[3];

void
set_active_toggle_button(osso_abook_data *data);
void
app_menu_set_disable_on_lowmem(HildonAppMenu *main_menu, const char *menu_name,
                               gboolean disable);
void
append_menu_extension_entries(HildonAppMenu *menu, const char *menu_name,
                              GtkWindow *parent, OssoABookContact *contact,
                              osso_abook_data *data);

void
switch_to_abc_view(osso_abook_data *data);

HildonAppMenu *
app_menu_from_menu_entries(GtkAccelGroup *accel_group,
                           OssoABookMenuEntry *menu_actions,
                           int menu_actions_count,
                           OssoABookMenuEntry *menu_filters,
                           int menu_filters_count, gpointer data,
                           GClosureNotify destroy_data);

void
update_menu(osso_abook_data *data);

#endif // MENU_H
