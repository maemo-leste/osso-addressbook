/*
 * utils.cpp
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

#include "config.h"

#include "app.h"
#include "utils.h"

static gboolean
has_children(GtkWidget *widget)
{
  GList *children_list;

  if (gtk_widget_has_focus(widget))
    return TRUE;

  if (!GTK_IS_CONTAINER(widget))
    return FALSE;

  children_list = gtk_container_get_children(GTK_CONTAINER(widget));
  if (!children_list)
    return FALSE;

  while (!has_children(children_list->data))
  {
    children_list = g_list_delete_link(children_list, children_list);
    if (!children_list)
      return FALSE;
  }

  g_list_free(children_list);

  return TRUE;
}

static void
gkey_set_focus(GKeyFile *gkeyfile, const gchar *key, GtkWidget *widget)
{
  g_key_file_set_boolean(gkeyfile, key, "Focused", has_children(widget));
}

static void
state_save(osso_abook_data *data)
{
  GKeyFile *gkeyfile;
  GtkTreeView *tree_view;
  gint *indices;
  GtkTreeSelection *tree_selection;
  GList *sel_rows;
  gint length;
  gint *list;
  gint path_indices;
  gchar *state_data;
  osso_context_t *osso;
  HildonLiveSearch *live_search;
  osso_state_t state;
  gsize state_size;
  GtkTreePath *path;

  gkeyfile = g_key_file_new();
  gkey_set_focus(gkeyfile, "ContactView", GTK_WIDGET(data->contact_view));
  tree_view = osso_abook_tree_view_get_tree_view(
                OSSO_ABOOK_TREE_VIEW(data->contact_view));
  gtk_tree_view_get_cursor(tree_view, &path, NULL);

  if (path)
  {
    indices = gtk_tree_path_get_indices(path);
    g_key_file_set_integer(gkeyfile, "ContactView", "CursorRow", *indices);
    gtk_tree_path_free(path);
  }

  tree_selection = gtk_tree_view_get_selection(tree_view);
  sel_rows = gtk_tree_selection_get_selected_rows(tree_selection, NULL);
  length = g_list_length(sel_rows);

  if (length > 0)
  {
    list = g_new(gint, length);
    if (sel_rows)
    {
      int i = 0;
      do
      {
        path = sel_rows->data;
        path_indices = *gtk_tree_path_get_indices(path);
        list[i] = path_indices;
        gtk_tree_path_free(path);
        ++i;
        sel_rows = g_list_delete_link(sel_rows, sel_rows);
      }
      while (sel_rows);
    }
    g_key_file_set_integer_list(gkeyfile, "ContactView", "Selection", list,
                                length);
    g_free(list);
  }

  if (gtk_widget_get_mapped(data->live_search))
  {
    gkey_set_focus(gkeyfile, "SearchEntry", GTK_WIDGET(data->live_search));
    live_search = HILDON_LIVE_SEARCH(data->live_search);
    hildon_live_search_save_state(live_search, gkeyfile);
    state_data = g_key_file_to_data(gkeyfile, &state_size, NULL);

    if (state_data)
    {
      osso = data->osso;
      state.state_data = state_data;
      state.state_size = state_size + 1;
      if (osso_state_write(osso, &state))
        g_critical("Failed to write state");
      g_free(state_data);
      g_key_file_free(gkeyfile);
      return;
    }

    g_critical("Failed to write state");
    g_key_file_free(gkeyfile);
    return;
  }

  state_data = g_key_file_to_data(gkeyfile, &state_size, NULL);
  if (!state_data)
    g_critical("Failed to write state");

  g_key_file_free(gkeyfile);
}

void
_window_is_topmost_cb(HildonWindow *window, GParamSpec *pspec,
                     osso_abook_data *data)
{
  HildonProgram *program = hildon_program_get_instance();

  if (hildon_window_get_is_topmost(window))
    hildon_program_set_can_hibernate(program, FALSE);
  else
  {
    state_save(data);
    hildon_program_set_can_hibernate(program, !data->dialog_open);
  }
}
