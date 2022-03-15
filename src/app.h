/*
 * app.h
 *
 * Copyright (C) 2020 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef APP_H
#define APP_H

#include <hildon/hildon.h>
#include <libosso.h>
#include <libosso-abook/osso-abook-contact-model.h>
#include <libosso-abook/osso-abook-debug.h>
#include <libosso-abook/osso-abook-roster-manager.h>
#include <libosso-abook/osso-abook-account-manager.h>
#include <libosso-abook/osso-abook-tree-view.h>
#include <libosso-abook/osso-abook-errors.h>
#include <libosso-abook/osso-abook-all-group.h>
#include <libosso-abook/osso-abook-aggregator.h>

#include "osso-abook-recent-view.h"
#include "osso-abook-sim-group.h"

typedef struct
{
  OssoABookRoster *aggregator;
  gboolean unk1;
  HildonStackableWindow *window;
  HildonAppMenu *main_menu;
  GSList *service_groups;
  OssoABookContactModel *contact_model;
  OssoABookFilterModel *filter_model;
  GtkWidget *contact_view;
  GtkTreeModel *model;
  gchar *selected_row_uid;
  GtkTreeRowReference *row;
  guint rows_reorder_id;
  int contacts_mode;
  OssoABookRecentView *recent_view;
  GtkWidget *live_search;
  GtkWidget *delete_live_search;
  int field_40;
  GtkWidget *field_44;
  gboolean dialog_open;
  osso_context_t *osso;
  gchar *arg1;
  GList *import_operations;
  gboolean import_started;
  gulong sequence_complete_id;
  int gap60;
  GtkWidget *groups_area;
  GtkWidget *starter_window;
  GtkWidget *stacked_group;
  int field_70;
  int field_74;
  int field_78;
  OssoABookPluginManager *plugin_manager;
  gboolean show_ui;
  int startup_complete;
  gboolean sim_group_ready;
  gboolean voicemail_contact_available;
  gboolean sim_capabilities_available;
  OssoABookGroup *sim_group;
  int gap98;
  int field_9C;
  GtkWidget *delete_contacts_window;
  GtkWidget *delete_contact_view;
  GtkWidget *align;
  guint one_day_timer_id;
  gboolean quit_on_close;
  gboolean field_B4;
  gboolean field_B8;
} osso_abook_data;

typedef struct
{
  GtkWidget *live_search;
  OssoABookTreeView *tree_view;
  GtkTreeRowReference *row_ref;
  float field_C;
  int field_10;
} live_search_data;

#define STARTUP_PROGRESS_SEPARATOR "====================================================================="

gboolean app_create(osso_context_t *osso, const gchar *arg1,
                    osso_abook_data *data);

void
app_destroy(osso_abook_data *data);
void
app_exit_main_loop(osso_abook_data *data);
void
app_show(osso_abook_data *data);

void
select_contact_row(osso_abook_data *data, GtkTreePath *path);

void
create_menu(osso_abook_data *data, OssoABookMenuEntry *entries,
            int entries_count, OssoABookContact *contact);
const char *
_get_vcard_field_from_uri(const gchar *uri);

GtkWidget *
_setup_live_search(HildonWindow *parent, OssoABookTreeView *tree_view);

#define IS_EMPTY(s) (!((s) && (((const char *)(s))[0])))

#endif /* APP_H */
