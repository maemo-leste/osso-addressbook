/*
 * actions.c
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

#include <libintl.h>

#include <libosso-abook/osso-abook-aggregator.h>
#include <libosso-abook/osso-abook-contact-editor.h>
#include <libosso-abook/osso-abook-errors.h>
#include <libosso-abook/osso-abook-touch-contact-starter.h>
#include <libosso-abook/osso-abook-util.h>
#include <libosso-abook/osso-abook-dialogs.h>
#include <libosso-abook/osso-abook-send-contacts.h>
#include <libosso-abook/osso-abook-self-contact.h>
#include <libosso-abook/osso-abook-mecard-view.h>
#include <libosso-abook/osso-abook-settings-dialog.h>

#include <rtcom-eventlogger-ui/rtcom-log-model.h>
#include <rtcom-eventlogger-ui/rtcom-log-view.h>

#include <libebook-contacts/libebook-contacts.h>

#include "osso-abook-get-your-contacts-dialog.h"

#include "actions.h"
#include "contacts.h"
#include "sim.h"
#include "menu.h"
#include "groups.h"

OssoABookMenuEntry sim_bt_menu_actions[BT_MENU_COUNT] =
{
  { "addr_me_copy_to_contacts", 0, 0,
    G_CALLBACK(import_selected_contact), NULL },
  { "addr_me_merge_contact", 0, 0,
    G_CALLBACK(contact_merge_cb), "sim-merge-bt" },
  { "addr_me_send_card", 0, 0,
    G_CALLBACK(contact_send_card_cb), NULL },
  { "addr_me_send_detail", 0, 0,
    G_CALLBACK(contact_send_detail_cb), NULL }
};

OssoABookMenuEntry contact_menu_actions[CONTACT_MENU_COUNT] =
{
  { "addr_me_edit_contact", 69, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(contact_starter_edit_cb), "edit-contact-button" },

  { "addr_me_send_card", 83, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(contact_send_card_cb), "send-card-button" },

  { "addr_me_send_detail", 84, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(contact_send_detail_cb), "send-detail-button" },

  { "addr_me_merge_contact", 77, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(contact_merge_cb), "merge-button" },

  { "addr_me_remove_contact", 68, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(contact_starter_delete_cb), NULL },

  { "addr_me_create_shortcut", 66, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(contact_create_shortcut_cb), "shortcut-button" },

  { "addr_me_request_author", 82, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(contact_request_authorization_cb), "req-auth-button" },

  { "addr_me_communication_history", 72, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(contact_communication_history_cb), "comm-history-button" }
};

OssoABookMenuEntry main_menu_actions[MAIN_MENU_COUNT] =
{
  { "addr_me_new_contact", 78, GDK_CONTROL_MASK,
    G_CALLBACK(contact_new_cb), "new-contact-bt" },
  { "addr_me_remove_contacts", 68, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(view_contacts_remove_cb), "delete-bt" },
  { "addr_me_import", 73, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(import_cb), "import-bt" },
  { "addr_me_export", 88, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(export_cb), "export-bt" },
  { "addr_me_mecard", 77, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(view_mecard_cb), NULL },
  { "addr_me_groups", 71, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(view_groups_cb), "groups-bt" },
  { "addr_me_settings", 83, GDK_CONTROL_MASK | GDK_SHIFT_MASK,
    G_CALLBACK(view_settings_cb), NULL }
};

gboolean
contact_saved_cb(GtkWidget *editor, const char *uid, osso_abook_data *data)
{
  merge(data, uid);
  return TRUE;
}

static void
contact_starter_edit_cb(GtkWidget *button, osso_abook_data *data)
{
  OssoABookTouchContactStarter *starter;

  starter = g_object_get_data(G_OBJECT(data->starter_window), "starter");
  osso_abook_touch_contact_starter_start_editor(
      OSSO_ABOOK_TOUCH_CONTACT_STARTER(starter));
}

OssoABookContact *
get_starter_contact(osso_abook_data *data)
{
  OssoABookTouchContactStarter *starter;

  if (!data->starter_window)
    return NULL;

  starter = g_object_get_data(G_OBJECT(data->starter_window), "starter");
  if (!starter)
    return NULL;

  return osso_abook_touch_contact_starter_get_contact(
      OSSO_ABOOK_TOUCH_CONTACT_STARTER(starter));
}

static gint
run_dialog(osso_abook_data *data, GtkDialog *dialog)
{
  gint result;

  data->dialog_open = TRUE;
  result = gtk_dialog_run(dialog);
  gtk_widget_hide(GTK_WIDGET(dialog));
  data->dialog_open = FALSE;

  return result;
}

static void
contact_send_card_cb(GtkWidget *button, osso_abook_data *data)
{
  OssoABookContact *contact;
  GtkWidget *dialog;

  if (osso_abook_check_disc_space(GTK_WINDOW(data->starter_window)))
  {
    contact = get_starter_contact(data);
    /* TODO: Missing func in libosso-abook */
    dialog = osso_abook_send_contacts_dialog_new(
                 GTK_WINDOW(data->starter_window), contact, FALSE);
    run_dialog(data, GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }
}

static void
contact_send_detail_cb(GtkWidget *button, osso_abook_data *data)
{
  OssoABookContact *contact;
  GtkWindow *window = GTK_WINDOW(data->starter_window);

  if (osso_abook_check_disc_space(window))
  {
    contact = get_starter_contact(data);
    osso_abook_send_contacts_detail_dialog_default(contact, window);
  }
}

static void
merge_cb(const char *uid, gpointer user_data)
{
  if (uid)
    merge(user_data, uid);
}

static void
contact_merge_cb(GtkWidget *button, osso_abook_data *data)
{
  gpointer s;
  OssoABookTouchContactStarter *starter;

  s = g_object_get_data(G_OBJECT(data->starter_window), "starter");
  starter = OSSO_ABOOK_TOUCH_CONTACT_STARTER(s);
  osso_abook_touch_contact_starter_start_merge(starter, data->contact_model,
                                               merge_cb, data);
}

static void
contact_starter_delete_cb(GtkWidget *button, osso_abook_data *data)
{
  OssoABookContact *contact;

  g_return_if_fail(data->aggregator);
  g_return_if_fail(data->starter_window);

  contact = get_starter_contact(data);

  if (!contact)
  {
    g_warning("No contact selected, action should be disabled");
    return;
  }

  if (osso_abook_delete_contact_dialog_run(GTK_WINDOW(data->starter_window),
                                           data->aggregator, contact))
  {
    gtk_widget_destroy(data->starter_window);
    data->starter_window = NULL;
  }
}

static void
contact_create_shortcut_cb(GtkWidget *button, osso_abook_data *data)
{
  OssoABookContact *contact = get_starter_contact(data);

  if (!contact)
  {
    g_warning("No contact selected, action should be disabled");
    return;
  }

  if (osso_abook_contact_shortcut_create(contact))
    hildon_banner_show_information(data->starter_window, 0,
        dgettext(NULL, "addr_ib_shortcut_created"));
}

static void
contact_request_authorization_cb(GtkWidget *button, osso_abook_data *data)
{
  OssoABookContact *master_contact, *contact;
  GList *contact_list;
  const char *display_name;
  gchar *info;

  master_contact = get_starter_contact(data);

  if (!master_contact)
  {
    g_warning("No contact selected, action should be disabled");
    return;
  }

  contact_list = osso_abook_contact_get_roster_contacts(master_contact);

  if (contact_list)
  {
    do
    {
      contact = contact_list->data;
      osso_abook_contact_accept(contact, master_contact,
                                GTK_WINDOW(data->starter_window));
      contact_list = contact_list->next;
    }
    while (contact_list);
  }
  display_name = osso_abook_contact_get_display_name(master_contact);
  info = g_strdup_printf(dgettext(NULL, "addr_ib_request_author_resend"),
                         display_name);
  hildon_banner_show_information(data->starter_window, 0, info);
  g_free(info);
}

RTComElQuery *
create_communication_history_query(OssoABookContact *contact, RTComEl *rtcomel)
{
  RTComElQuery *query;
  const char *uid, *bound_name;
  GList *contact_list;
  OssoABookContact *contact_internal;
  TpAccount *account; /* McAccount *account */

  query = rtcom_el_query_new(rtcomel);
  rtcom_el_query_set_group_by(query, RTCOM_EL_QUERY_GROUP_BY_NONE);
  uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);

  if (osso_abook_is_temporary_uid(uid))
  {
    contact_list = osso_abook_contact_get_roster_contacts(contact);
    if (contact_list)
    {
      contact_internal = contact_list->data;
      g_list_free(contact_list);
      if ( contact_internal )
      {
        account = osso_abook_contact_get_account(contact_internal);
        bound_name = osso_abook_contact_get_bound_name(contact_internal);

        if (!rtcom_el_query_prepare(
              query,
              "local-uid", tp_account_get_path_suffix(account), NULL,
              "remote-uid", bound_name, NULL,
              NULL))
        {
          goto fail;
        }
      }
    }

    g_warn_if_reached();
    goto fail;
  }

  if (!rtcom_el_query_prepare(query, "remote-ebook-uid", uid, NULL, NULL));
    goto fail;

  return query;

fail:
  g_warning("error preparing communication history query");

  return NULL;
}

static void
contact_communication_history_cb(GtkWidget *button, osso_abook_data *data)
{
  OssoABookContact *contact;
  RTComLogModel *rtcom_model;
  RTComEl *eventlogger;
  RTComElQuery *query;
  GtkWidget *logview, *pan, *dialog;
  GtkObject *tree_model;

  g_return_if_fail(data);

  contact = get_starter_contact(data);
  rtcom_model = rtcom_log_model_new();
  eventlogger = rtcom_log_model_get_eventlogger(rtcom_model);
  query = create_communication_history_query(contact, eventlogger);

  if (!query)
  {
    g_warning("error preparing communication history query");
    g_object_unref(rtcom_model);
    return;
  }

  rtcom_el_query_set_group_by(query, RTCOM_EL_QUERY_GROUP_BY_NONE);
  rtcom_log_model_set_abook_aggregator(rtcom_model,
                                       OSSO_ABOOK_AGGREGATOR(data->aggregator));
  logview = rtcom_log_view_new();
  gtk_widget_set_name(logview, "osso-abook-communication-history");

  rtcom_log_view_set_highlight_new_events(RTCOM_LOG_VIEW(logview), FALSE);
  rtcom_log_view_set_show_display_names(RTCOM_LOG_VIEW(logview), FALSE);
  rtcom_log_view_set_model(RTCOM_LOG_VIEW(logview), GTK_TREE_MODEL(rtcom_model));
  rtcom_log_model_set_limit(rtcom_model, 60);
  rtcom_log_model_populate_query(rtcom_model, query);

  pan = osso_abook_pannable_area_new();
  gtk_container_add(GTK_CONTAINER(pan), logview);

  dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog),
                       dgettext(NULL, "addr_me_communication_history"));

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), pan, 1, 1, 0);
  if (data->starter_window)
  {
    if (GTK_IS_WINDOW(data->starter_window))
      gtk_window_set_transient_for(GTK_WINDOW(dialog),
                                   GTK_WINDOW(data->starter_window));
  }
  gtk_widget_show_all(dialog);
  g_object_unref(rtcom_model);
  g_object_unref(query);
}

static void
import_selected_contact(GtkWidget *button, osso_abook_data *data)
{
  OssoABookContact *contact = get_starter_contact(data);

  if (contact)
    sim_import_contact(contact);
  else
    g_warning("No contact selected, 'copy to contacts' action should be disabled");
}

static void
contact_new_cb(GtkWidget *button, osso_abook_data *data)
{
  GtkWidget *editor;

  gtk_widget_hide(data->live_search);
  editor = osso_abook_contact_editor_new_with_contact(
        GTK_WINDOW(data->window), NULL, OSSO_ABOOK_CONTACT_EDITOR_CREATE);
  g_signal_connect(editor, "contact-saved",
                   G_CALLBACK(contact_saved_cb), data);
  run_dialog(data, GTK_DIALOG(editor));
  gtk_widget_destroy(editor);
}

static void
view_contacts_remove_cb(GtkWidget *button, osso_abook_data *data)
{
  open_delete_contacts_view_window(data);
}

static void
response_cb(GtkWidget *dialog, gint response_id, osso_abook_data *data)
{
  gtk_widget_destroy(dialog);
  data->dialog_open = FALSE;
}

static void
run_dialog_async(osso_abook_data *data, GtkDialog *dialog)
{
  g_warn_if_fail(!data->dialog_open);

  if (!gtk_window_get_modal(GTK_WINDOW(dialog)))
  {
    g_warning("Please check why your dialog is not marked as modal. To make this function work it really should. Enforcing this now.");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  }

  gtk_widget_hide(data->live_search);
  data->dialog_open = TRUE;
  g_signal_connect(dialog, "response", G_CALLBACK(response_cb), data);
  gtk_widget_show(GTK_WIDGET(dialog));
}

static void
import_cb(GtkWidget *button, osso_abook_data *data)
{
  GtkWidget *dialog = osso_abook_get_your_contacts_dialog_new(data);

  run_dialog_async(data, GTK_DIALOG(dialog));
}

static void
view_mecard_cb(GtkWidget *button, osso_abook_data *data)
{
  OssoABookSelfContact *self;
  GtkWidget *view;
  HildonAppMenu *menu;

  gtk_widget_hide(data->live_search);
  self = osso_abook_self_contact_get_default();

  if (osso_abook_gconf_contact_is_vcard_empty(OSSO_ABOOK_GCONF_CONTACT(self)) ||
      osso_abook_gconf_contact_is_deleted(OSSO_ABOOK_GCONF_CONTACT(self)))
  {
    GtkWidget *editor = osso_abook_contact_editor_new_with_contact(
          GTK_WINDOW(data->window), OSSO_ABOOK_CONTACT(self),
          OSSO_ABOOK_CONTACT_EDITOR_CREATE_SELF);

    g_signal_connect(editor, "contact-saved",
                     G_CALLBACK(gtk_true), NULL);
    run_dialog(data, GTK_DIALOG(editor));
    gtk_widget_destroy(editor);
  }

  view = osso_abook_mecard_view_new();
  menu = hildon_window_get_app_menu(HILDON_WINDOW(view));
  append_menu_extension_entries(menu, OSSO_ABOOK_MENU_NAME_MECARD_VIEW,
                                GTK_WINDOW(view), OSSO_ABOOK_CONTACT(self),
                                data);
  gtk_widget_show(view);
  g_object_unref(self);
}

static void
export_cb(GtkWidget *button, osso_abook_data *data)
{
  /* implement me */
  g_assert(0);
}

static void
view_groups_cb(GtkWidget *button, osso_abook_data *data)
{
  GtkWidget *dialog;

  g_return_if_fail(data);

  gtk_widget_hide(data->live_search);
  data->groups_area = osso_abook_pannable_area_new();
  g_object_add_weak_pointer(G_OBJECT(data->groups_area),
                            (gpointer *)&data->groups_area);
  dialog = gtk_dialog_new();
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
  gtk_widget_hide(GTK_DIALOG(dialog)->action_area);
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(data->window));
  gtk_window_set_title(GTK_WINDOW(dialog),
                       dgettext(NULL, "addr_ti_groups_title"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), data->groups_area, TRUE,
                     TRUE, 0);
  update_view_groups_accounts(data);
  gtk_widget_show_all(data->groups_area);
  gtk_widget_show(dialog);
}

static void
view_settings_cb(GtkWidget *button, osso_abook_data *data)
{
  EBook *book = osso_abook_roster_get_book(data->aggregator);
  GtkWidget *dialog;

  dialog = osso_abook_settings_dialog_new(GTK_WINDOW(data->window), book);
  run_dialog_async(data, GTK_DIALOG(dialog));
}
