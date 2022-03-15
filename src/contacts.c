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

#include "config.h"

#include <libintl.h>

#include <libosso-abook/osso-abook-contact-view.h>
#include <libosso-abook/osso-abook-recent-group.h>
#include <libosso-abook/osso-abook-dialogs.h>

#include "actions.h"
#include "menu.h"
#include "osso-abook-get-your-contacts-dialog.h"
#include "utils.h"

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

static void
delete_edit_toolbar_button_clicked_cb(GtkWidget *toolbar, osso_abook_data *data)
{
  GList *selection;

  gtk_widget_hide(data->delete_live_search);
  selection = osso_abook_contact_view_get_selection(
        OSSO_ABOOK_CONTACT_VIEW(data->delete_contact_view));

  if (!selection)
    return;

  osso_abook_confirm_delete_contacts_dialog_run(
        GTK_WINDOW(data->delete_contacts_window), NULL, selection);
  g_list_free(selection);
  gtk_widget_destroy(data->delete_contacts_window);
  gtk_window_present(GTK_WINDOW(data->window));
}

void
open_delete_contacts_view_window(osso_abook_data *data)
{
  GtkWidget *toolbar;
  OssoABookContactModel *model;
  GtkWidget *align;
  GConfValue *val;
  gint list_mode;
  OssoABookFilterModel *filter_model;

  data->delete_contacts_window = hildon_stackable_window_new();
  gtk_window_set_title(GTK_WINDOW(data->delete_contacts_window),
                       dgettext(NULL, "addr_ti_view_select_contacts"));
  toolbar = hildon_edit_toolbar_new_with_text(
      dgettext(NULL, "addr_ti_view_select_contacts"),
      dgettext("hildon-libs", "wdgt_bd_delete"));

  hildon_window_set_edit_toolbar(HILDON_WINDOW(data->delete_contacts_window),
                                 HILDON_EDIT_TOOLBAR(toolbar));

  g_signal_connect(toolbar, "button-clicked",
                   G_CALLBACK(delete_edit_toolbar_button_clicked_cb), data);
  g_signal_connect_swapped(toolbar, "arrow-clicked",
                           G_CALLBACK(gtk_widget_destroy),
                           data->delete_contacts_window);

  hildon_program_add_window(hildon_program_get_instance(),
                            HILDON_WINDOW(data->delete_contacts_window));
  gtk_widget_set_size_request(data->delete_contacts_window, 720, -1);

  g_signal_connect(data->delete_contacts_window, "notify::is-topmost",
                   G_CALLBACK(_window_is_topmost_cb), data);

  model = osso_abook_contact_model_get_default();
  filter_model = osso_abook_filter_model_new(OSSO_ABOOK_LIST_STORE(model));

  align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(align), 4, 0, 16, 16);

  data->delete_contact_view =
    osso_abook_contact_view_new(HILDON_UI_MODE_EDIT, model, filter_model);
  osso_abook_contact_view_set_minimum_selection(
    OSSO_ABOOK_CONTACT_VIEW(data->delete_contact_view), 1);
  osso_abook_contact_view_set_maximum_selection(
    OSSO_ABOOK_CONTACT_VIEW(data->delete_contact_view), G_MAXUINT);

  val = gconf_client_get(osso_abook_get_gconf_client(),
                         "/apps/osso-addressbook/list-mode", NULL);

  if (val)
  {
    list_mode = gconf_value_get_int(val);
    gconf_value_free(val);
  }
  else
    list_mode = 1;

  osso_abook_tree_view_set_avatar_view(
    OSSO_ABOOK_TREE_VIEW(data->delete_contact_view), list_mode);
  gtk_container_add(GTK_CONTAINER(align), data->delete_contact_view);
  data->delete_live_search =
    _setup_live_search(HILDON_WINDOW(data->delete_contacts_window),
                       OSSO_ABOOK_TREE_VIEW(data->delete_contact_view));

  if (gtk_widget_get_visible(data->live_search))
  {
    hildon_live_search_append_text(
      HILDON_LIVE_SEARCH(data->delete_live_search),
      hildon_live_search_get_text(HILDON_LIVE_SEARCH(data->live_search)));
  }
  else
    gtk_widget_hide(data->delete_live_search);

  gtk_widget_hide(data->live_search);
  g_object_unref(filter_model);
  gtk_container_add(GTK_CONTAINER(data->delete_contacts_window), align);
  gtk_widget_show_all(data->delete_contacts_window);
  gtk_window_fullscreen(GTK_WINDOW(data->delete_contacts_window));
}
