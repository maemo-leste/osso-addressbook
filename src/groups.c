/*
 * groups.c
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

#include <hildon/hildon.h>
#include <libosso-abook/osso-abook-service-group.h>
#include <libosso-abook/osso-abook-util.h>

#include <libintl.h>

#include "actions.h"
#include "app.h"
#include "groups.h"
#include "menu.h"
#include "sim.h"

struct view_specific_group_cb_data
{
  gpointer data;
  OssoABookGroup *group;
};

static void
view_specific_group_cb(GtkWidget *groups_window_child,
                       struct view_specific_group_cb_data *cb_data)
{
  g_return_if_fail(cb_data);
  g_return_if_fail(!groups_window_child || GTK_IS_WIDGET(groups_window_child));

  view_group_subview(cb_data->data, cb_data->group);

  if (groups_window_child)
    gtk_widget_destroy(gtk_widget_get_toplevel(groups_window_child));
}

static void
attach_to_table(GtkTable *table, osso_abook_data *data, OssoABookGroup *group,
                GtkWidget *child, int attach)
{
  int top_attach = attach / 2;
  struct view_specific_group_cb_data *cb_data;

  if (data->sim_group_ready)
    top_attach++;

  cb_data = g_new0(struct view_specific_group_cb_data, 1);
  cb_data->data = data;
  cb_data->group = group;

  g_signal_connect(child, "clicked",
                   G_CALLBACK(view_specific_group_cb), cb_data);
  gtk_table_attach(table, child, attach % 2, attach % 2 + 1, top_attach,
                   top_attach + 1, GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
}

static void
view_sim_cb(GtkWidget *button, osso_abook_data *data)
{
  g_return_if_fail(data);

  g_assert(data->sim_group);
  open_sim_view_window(data, data->sim_group);
  gtk_widget_destroy(gtk_widget_get_toplevel(button));
}

static void
add_sim_group(GtkTable *table, osso_abook_data *data)
{
  gchar *msgid = dgettext(NULL, osso_abook_group_get_name(data->sim_group));
  GtkWidget *button;
  GtkWidget *image;

  button = hildon_button_new_with_text(HILDON_SIZE_FINGER_HEIGHT,
                                       HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
                                       msgid, NULL);
  image = gtk_image_new_from_icon_name(
      osso_abook_group_get_icon_name(data->sim_group),
      HILDON_ICON_SIZE_FINGER);
  hildon_button_set_image(HILDON_BUTTON(button), image);
  gtk_button_set_alignment(GTK_BUTTON(button), 0.0, 0.5);
  g_signal_connect(button, "clicked", G_CALLBACK(view_sim_cb), data);
  gtk_table_attach(table, button, 0, 2, 0, 1, GTK_FILL | GTK_EXPAND, GTK_FILL,
                   0, 0);
}

static GHashTable *
add_service_groups(GtkTable *table, osso_abook_data *data, int *attach)
{
  GHashTable *hash_table;
  GSList *groups;

  hash_table = g_hash_table_new((GHashFunc)&g_direct_hash,
                                (GEqualFunc)&g_direct_equal);

  for (groups = data->service_groups; groups && groups->data;
       groups = groups->next)
  {
    OssoABookGroup *group = OSSO_ABOOK_GROUP(groups->data);

    if (group && OSSO_ABOOK_IS_SERVICE_GROUP(group))
    {
      TpAccount *account = osso_abook_service_group_get_account(
          OSSO_ABOOK_SERVICE_GROUP(group));

      if (account)
      {
        TpProtocol *protocol = osso_abook_account_manager_get_protocol_object(
            NULL, tp_account_get_protocol_name(account));
        gint count = GPOINTER_TO_INT(g_hash_table_lookup(hash_table,
                                                         protocol));
        g_hash_table_replace(hash_table, protocol,
                             GINT_TO_POINTER(count + 1));
      }
    }
  }

  for (groups = data->service_groups; groups && groups->data;
       groups = groups->next)
  {
    OssoABookGroup *group = OSSO_ABOOK_GROUP(groups->data);

    if (group && OSSO_ABOOK_IS_SERVICE_GROUP(group))
    {
      TpAccount *account = osso_abook_service_group_get_account(
          OSSO_ABOOK_SERVICE_GROUP(group));

      if (account)
      {
        TpProtocol *protocol =
          osso_abook_account_manager_get_protocol_object(
            NULL, tp_account_get_protocol_name(account));
        const char *id = dgettext(NULL, "addr_va_groups_imgrp");
        gchar *title = g_strdup_printf(id,
                                       tp_account_get_display_name(account));
        GtkWidget *button;
        GtkWidget *image;

        if (IS_EMPTY(title))
        {
          id = dgettext(NULL, "addr_va_groups_imgrp");
          g_free(title);
          title = g_strdup_printf(id, tp_protocol_get_english_name(protocol));
        }

        if (GPOINTER_TO_INT(g_hash_table_lookup(hash_table, protocol)) < 2)
        {
          button = hildon_button_new_with_text(
              HILDON_SIZE_FINGER_HEIGHT,
              HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
              title, NULL);
        }
        else
        {
          button = hildon_button_new_with_text(
              HILDON_SIZE_FINGER_HEIGHT,
              HILDON_BUTTON_ARRANGEMENT_VERTICAL,
              title, osso_abook_tp_account_get_bound_name(account));
        }

        image = gtk_image_new_from_icon_name(
            osso_abook_group_get_icon_name(group), HILDON_ICON_SIZE_FINGER);
        hildon_button_set_image(HILDON_BUTTON(button), image);
        hildon_button_set_image_position(HILDON_BUTTON(button), GTK_POS_LEFT);
        gtk_button_set_alignment(GTK_BUTTON(button), 0.0, 0.5);
        g_free(title);
        attach_to_table(table, data, group, button, *attach);
        (*attach)++;
      }
    }
  }

  return hash_table;
}

static void
add_protocol_groups(GtkTable *table, osso_abook_data *data,
                    GSList *groups, int *attach)
{
  for (; groups; groups = groups->next)
  {
    const gchar *msgid = osso_abook_group_get_name(groups->data);
    const gchar *icon = osso_abook_group_get_icon_name(groups->data);
    GtkWidget *button = hildon_button_new_with_text(
        HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
        dgettext(NULL, msgid), NULL);
    GtkWidget *image = gtk_image_new_from_icon_name(icon,
                                                    HILDON_ICON_SIZE_FINGER);

    hildon_button_set_image(HILDON_BUTTON(button), image);
    gtk_button_set_alignment(GTK_BUTTON(button), 0.0, 0.5);
    attach_to_table(table, data, groups->data, button, *attach);
    (*attach)++;
  }
}

void
update_view_groups_accounts(osso_abook_data *data)
{
  GtkWidget *child;
  int attach = 0;
  GtkTable *table;
  GSList *protocol_groups;
  GHashTable *hash_table;
  int rows;

  update_menu(data);

  if (!data->groups_area)
    return;

  child = gtk_bin_get_child(GTK_BIN(data->groups_area));

  if (child)
    gtk_widget_destroy(child);

  protocol_groups = get_protocol_groups();

  rows = g_slist_length(protocol_groups) + g_slist_length(data->service_groups);

  if (rows % 2 > 0)
    rows = rows / 2 + 1;
  else
    rows = rows / 2;

  if (data->sim_group_ready)
    rows++;

  table = GTK_TABLE(gtk_table_new(rows, 2, 1));
  gtk_table_set_col_spacings(table, 8);
  gtk_table_set_row_spacings(table, 8);

  if (data->sim_group_ready)
    add_sim_group(table, data);

  hash_table = add_service_groups(table, data, &attach);
  add_protocol_groups(table, data, protocol_groups, &attach);

  hildon_pannable_area_add_with_viewport(
    HILDON_PANNABLE_AREA(data->groups_area), GTK_WIDGET(table));
  gtk_widget_show_all(GTK_WIDGET(table));
  g_slist_free(protocol_groups);
  g_hash_table_destroy(hash_table);
}
