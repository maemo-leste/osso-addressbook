/*
 * contacts.c
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

#include <libosso-abook/osso-abook-recent-group.h>

#include "actions.h"
#include "menu.h"
#include "osso-abook-get-your-contacts-dialog.h"

#include "contacts.h"

void
merge(osso_abook_data *data, const char *uid)
{
  GtkTreeIter iter;

  if (osso_abook_contact_model_find_contact(data->contact_model, uid, &iter))
  {
    GtkTreeView *tree_view = osso_abook_tree_view_get_tree_view(
          OSSO_ABOOK_TREE_VIEW(data->contact_view));
    GtkTreeModel *tree_model = gtk_tree_view_get_model(tree_view);
    GtkTreePath *model_path;
    GtkTreeIter filter_iter;

    gtk_tree_model_filter_convert_child_iter_to_iter(
          GTK_TREE_MODEL_FILTER(tree_model), &filter_iter, &iter);
    model_path = gtk_tree_model_get_path(tree_model, &filter_iter);
    select_contact_row(data, model_path);
    gtk_tree_path_free(model_path);
  }
  else
  {
    g_free(data->selected_row_uid);
    data->selected_row_uid = g_strdup(uid);
  }
}

static void
recent_view_show_contact_cb(OssoABookRecentView *view,
                            OssoABookContact *contact, osso_abook_data *data)
{
  create_menu(data, contact_menu_actions, 8, contact);
}

void
set_contacts_mode(osso_abook_data *data, int mode)
{
  OssoABookGroup *group;

  if (data->contacts_mode == mode)
    return;

  data->contacts_mode = mode;
  gconf_client_set_int(osso_abook_get_gconf_client(),
                       "/apps/osso-addressbook/contacts-mode",
                       data->contacts_mode, NULL);
  if (mode == 1)
  {
    gtk_widget_hide(data->live_search);
    hildon_live_search_widget_unhook(HILDON_LIVE_SEARCH(data->live_search));

    if (!data->recent_view)
    {
      data->recent_view = osso_abook_recent_view_new(
            OSSO_ABOOK_AGGREGATOR(data->aggregator));
      g_object_ref_sink(data->recent_view);
      g_signal_connect(data->recent_view, "show-contact",
                       G_CALLBACK(recent_view_show_contact_cb), data);
    }

    gtk_container_remove(GTK_CONTAINER(data->align),
                         GTK_WIDGET(data->contact_view));
    gtk_container_add(GTK_CONTAINER(data->align),
                      GTK_WIDGET(data->recent_view));
    gtk_widget_show(GTK_WIDGET(data->recent_view));

    osso_abook_recent_view_install_live_search(data->recent_view,
                                               HILDON_WINDOW(data->window));
    group = osso_abook_recent_group_get();
  }
  else
  {
    GtkTreeView *tree_view;

    group = osso_abook_all_group_get();
    osso_abook_recent_view_remove_live_search(data->recent_view);
    gtk_widget_hide(GTK_WIDGET(data->recent_view));
    gtk_container_remove(GTK_CONTAINER(data->align),
                         GTK_WIDGET(data->recent_view));
    gtk_container_add(GTK_CONTAINER(data->align),
                      GTK_WIDGET(data->contact_view));
    tree_view = osso_abook_tree_view_get_tree_view(
          OSSO_ABOOK_TREE_VIEW(data->contact_view));
    hildon_live_search_widget_hook(HILDON_LIVE_SEARCH(data->live_search),
                                   GTK_WIDGET(data->window),
                                   GTK_WIDGET(tree_view));
  }

  scroll_to_top_if_needed(data);
  gtk_window_set_title(GTK_WINDOW(data->window),
                       osso_abook_group_get_display_title(group));
  set_active_toggle_button(data);
}

void
scroll_to_top_if_needed(osso_abook_data *data)
{
  if (data->contacts_mode == 1)
  {
    if (data->field_B8)
    {
      osso_abook_recent_view_scroll_to_top(data->recent_view);
      data->field_B8 = FALSE;
    }
  }
  else if (data->contacts_mode == 0)
  {
    if (data->field_B4)
    {
      GtkWidget *area = osso_abook_tree_view_get_pannable_area(
            OSSO_ABOOK_TREE_VIEW(data->contact_view));

      if (gtk_widget_get_realized(area))
        hildon_pannable_area_jump_to(HILDON_PANNABLE_AREA(area), -1, 0);

      data->field_B4 = FALSE;
    }
  }
}

void
open_delete_contacts_view_window(osso_abook_data *data)
{
  /* implement me */
  g_assert(0);
}
