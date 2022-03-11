/*
 * menu.c
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

#include <glib-2.0/glib.h>
#include <libosso-abook/osso-abook-plugin-manager.h>
#include <libosso-abook/osso-abook-settings.h>

#include <libintl.h>

#include "app.h"
#include "menu.h"
#include "hw.h"
#include "contacts.h"

/*
OssoABookMenuEntry main_menu_actions[MENU_ACTIONS_COUNT] =
{
  {
    "addr_me_new_contact", 78, GDK_CONTROL_MASK,
    contact_new_cb, "new-contact-bt"
  },
  {
    "addr_me_remove_contacts", 68, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    view_contacts_remove_cb, "delete-bt"
  },
  {
    "addr_me_import", 73, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    import_cb, "import-bt"
  },
  {
    "addr_me_export", 88, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    export_cb, "export-bt"
  },
  {
    "addr_me_mecard", 77, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    view_mecard_cb, NULL
  },
  {
    "addr_me_groups", 71, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    view_groups_cb, "groups-bt"
  },
  {
    "addr_me_settings", 83, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    view_settings_cb, NULL
  }
};
*/

static void
view_main_abc_cb(GtkButton *button, gpointer user_data)
{
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    switch_to_abc_view(user_data);
}

static void
view_main_status_cb(GtkButton *button, gpointer user_data)
{
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
  {
    osso_abook_settings_set_contact_order(OSSO_ABOOK_CONTACT_ORDER_PRESENCE);
    set_contacts_mode(user_data, 0);
  }
}

static void
view_main_recent_cb(GtkButton *button, gpointer user_data)
{
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
    set_contacts_mode(user_data, 1);
}

OssoABookMenuEntry main_menu_filters[3] =
{
  { "addr_me_abc", 0, 0, G_CALLBACK(view_main_abc_cb), "sort-abc-bt" },
  { "addr_me_status", 0, 0, G_CALLBACK(view_main_status_cb), "sort-status-bt" },
  { "addr_me_recent", 0, 0, G_CALLBACK(view_main_recent_cb), "sort-recent-bt" }
};

GtkWidget *main_menu_filter_buttons[G_N_ELEMENTS(main_menu_filters)] = {};

void
switch_to_abc_view(osso_abook_data *data)
{
  osso_abook_settings_set_contact_order(OSSO_ABOOK_CONTACT_ORDER_NAME);
  set_contacts_mode(data, 0);
}

static void
action_button_clicked_cb(GObject *button, gpointer user_data)
{
  if (GPOINTER_TO_INT(g_object_get_data(button, "disable-lowmem")))
  {
    if (hw_is_lowmem_mode())
    {
      GSignalInvocationHint *hint = g_signal_get_invocation_hint(button);

      g_signal_stop_emission(button, hint->signal_id, hint->detail);
      hildon_banner_show_information(
            NULL, NULL, g_dgettext("ke-recv", "memr_ib_operation_disabled"));
    }
  }
}

static GtkWidget *
create_action_button(const OssoABookMenuEntry *entry,
                     GtkAccelGroup *accel_group, gboolean is_msgid,
                     gpointer data, GClosureNotify destroy_data)
{
  const char *title = entry->label;
  GtkWidget *button;

  if (is_msgid)
    title = dgettext(NULL, title);

  button = hildon_button_new_with_text(HILDON_SIZE_FINGER_HEIGHT,
                                       HILDON_BUTTON_ARRANGEMENT_VERTICAL,
                                       title, NULL);
  if (accel_group && entry->accel_key)
  {
    gtk_widget_add_accelerator(button, "clicked", accel_group, entry->accel_key,
                               entry->accel_mods, 0);
  }

  g_signal_connect_after(button, "clicked",
                         G_CALLBACK(action_button_clicked_cb), NULL);
  g_signal_connect_data(button, "clicked",
                         entry->callback, data, destroy_data, G_CONNECT_AFTER);

  return button;
}

static GtkWidget *
app_menu_append_action(HildonAppMenu *menu, GtkAccelGroup *accel_group,
                       const OssoABookMenuEntry *entry, gboolean label_is_msgid,
                       gpointer data, GClosureNotify destroy_data)
{
  GtkWidget *button = create_action_button(entry, accel_group, label_is_msgid,
                                           data, destroy_data);

  hildon_app_menu_append(menu, GTK_BUTTON(button));
  gtk_widget_show(button);

  return button;
}

static GtkWidget *
app_menu_append_filter(HildonAppMenu *menu, GtkAccelGroup *accel_group,
                       OssoABookMenuEntry *entry, gpointer user_data,
                       GSList *group)
{
  GtkWidget *button =
      hildon_gtk_radio_button_new(HILDON_SIZE_FINGER_HEIGHT, group);

  gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
  gtk_button_set_label(GTK_BUTTON(button), dgettext(NULL, entry->label));
  g_signal_connect(button, "clicked", entry->callback, user_data);
  hildon_app_menu_add_filter(menu, GTK_BUTTON(button));
  gtk_widget_show(button);

  return button;
}

void
append_menu_extension_entries(HildonAppMenu *menu, const char *menu_name,
                              GtkWindow *parent, OssoABookContact *contact,
                              osso_abook_data *data)
{
  GList *extensions;

  g_return_if_fail(menu_name != NULL);

  if (!data->plugin_manager)
    data->plugin_manager = osso_abook_plugin_manager_new();

  extensions = osso_abook_plugin_manager_create_menu_extensions(
        data->plugin_manager, OSSO_ABOOK_TYPE_MENU_EXTENSION,
        "menu-name", menu_name,
        "parent", parent,
        "contact", contact,
        NULL);

  while(extensions)
  {
    OssoABookMenuExtension *extension = extensions->data;
    const OssoABookMenuEntry *menu_entries =
        osso_abook_menu_extension_get_menu_entries(extension);
    int n_menu_entries =
        osso_abook_menu_extension_get_n_menu_entries(extension);
    int i;

    g_return_if_fail(NULL != menu_entries);

    for (i = 0; i < n_menu_entries; i++)
    {
      app_menu_append_action(menu, 0, menu_entries, 0, extension,
                             (GClosureNotify)g_object_unref);
      menu_entries++;
    }

    extensions = g_list_delete_link(extensions, extensions);
  }
}

static GtkWidget *
app_menu_get_widget(HildonAppMenu *app_menu, const char *name)
{
  g_return_val_if_fail(HILDON_IS_APP_MENU(app_menu), NULL);
  g_return_val_if_fail(name != NULL, NULL);

  return g_object_get_data(G_OBJECT(app_menu), name);
}

void
app_menu_set_disable_on_lowmem(HildonAppMenu *main_menu, const char *menu_name,
                               gboolean disable)
{
  GtkWidget *widget = app_menu_get_widget(main_menu, menu_name);

  if (widget)
  {
    g_object_set_data(G_OBJECT(widget), "disable-lowmem",
                      GINT_TO_POINTER(disable));
  }
}

HildonAppMenu *
app_menu_from_menu_entries(GtkAccelGroup *accel_group,
                           OssoABookMenuEntry *menu_actions,
                           int menu_actions_count,
                           OssoABookMenuEntry *menu_filters,
                           int menu_filters_count, gpointer data,
                           GClosureNotify destroy_data)
{
  HildonAppMenu *menu;
  int i;
  GSList *group = NULL;

  g_return_val_if_fail(menu_actions, NULL);

  menu = HILDON_APP_MENU(hildon_app_menu_new());

  for (i = 0; i < menu_actions_count; i++)
  {
    OssoABookMenuEntry *entry = &menu_actions[i];
    GtkWidget *button = app_menu_append_action(menu, accel_group, entry, TRUE,
                                               data, destroy_data);
    if (entry->name)
      g_object_set_data(G_OBJECT(menu), entry->name, button);
  }

  if (!menu_filters)
    return menu;

  for (i = 0; i < menu_filters_count; i++)
  {
    OssoABookMenuEntry *entry = &menu_filters[i];
    GtkWidget *button = app_menu_append_filter(menu, accel_group, entry, data,
                                               group);

    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(button));

    if (menu_filters == main_menu_filters)
      main_menu_filter_buttons[i] = button;

    if (entry->name)
      g_object_set_data(G_OBJECT(menu), entry->name, button);
  }

  return menu;
}

void
set_active_toggle_button(osso_abook_data *data)
{
  if (data->contacts_mode == 1)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(main_menu_filter_buttons[2]),
                                 TRUE);
  }
  else if (osso_abook_settings_get_contact_order() ==
           OSSO_ABOOK_CONTACT_ORDER_PRESENCE)
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(main_menu_filter_buttons[1]),
                                 TRUE);
  }
  else
  {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(main_menu_filter_buttons[0]),
                                 TRUE);
  }
}
