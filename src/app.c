/*
 * app.c
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

#include <libosso-abook/osso-abook-service-group.h>
#include <libosso-abook/osso-abook-waitable.h>
#include <libosso-abook/osso-abook-contact.h>
#include <libosso-abook/osso-abook-gconf-contact.h>
#include <libosso-abook/osso-abook-voicemail-contact.h>
#include <libosso-abook/osso-abook-contact-view.h>
#include <libosso-abook/osso-abook-debug.h>
#include <libosso-abook/osso-abook-row-model.h>
#include <libosso-abook/osso-abook-util.h>
#include <libosso-abook/osso-abook-log.h>
#include <libosso-abook/osso-abook-enums.h>
#include <libosso-abook/osso-abook-touch-contact-starter.h>

#include <libintl.h>
#include <string.h>

#include "osso-abook-sim-group.h"
#include "osso-abook-get-your-contacts-dialog.h"

#include "app.h"
#include "actions.h"
#include "service.h"
#include "groups.h"
#include "contacts.h"
#include "importer.h"
#include "menu.h"
#include "hw.h"
#include "utils.h"

static gboolean idle_import(gpointer user_data);

static gint
_compare_service_group(gconstpointer a, gconstpointer b)
{
  return strcmp(osso_abook_group_get_display_title(OSSO_ABOOK_GROUP(a)),
                osso_abook_group_get_display_title(OSSO_ABOOK_GROUP(b)));
}

static void
roster_created_cb(OssoABookRosterManager *manager, OssoABookRoster *roster,
                  gpointer user_data)
{
  osso_abook_data *data = user_data;
  TpAccount *account = osso_abook_roster_get_account(roster);
  OssoABookGroup *group = osso_abook_service_group_get(account);

  g_return_if_fail(NULL != group);
  g_return_if_fail(NULL == g_slist_find (data->service_groups, group));

  data->service_groups = g_slist_insert_sorted(
        data->service_groups, group, _compare_service_group);
  update_view_groups_accounts(data);
}

static void
roster_removed_cb(OssoABookRosterManager *manager, OssoABookRoster *roster,
                  gpointer user_data)
{
  osso_abook_data *data = user_data;
  TpAccount *account = osso_abook_roster_get_account(roster);
  OssoABookGroup *group = osso_abook_service_group_lookup_by_name(
        tp_account_get_path_suffix(account));

  data->service_groups = g_slist_remove(data->service_groups, group);
  update_view_groups_accounts(data);
}


static void
account_removed_cb(OssoABookAccountManager *manager, TpAccount *account,
                   osso_abook_data *data)
{
  OssoABookGroup *group = osso_abook_service_group_lookup_by_name(
        tp_account_get_path_suffix(account));

  data->service_groups = g_slist_remove(data->service_groups, group);
  update_view_groups_accounts(data);
}

static gboolean
one_day_expired_cb(gpointer data)
{
  app_exit_main_loop(data);

  return FALSE;
}

static void
app_select_all_group(osso_abook_data *data)
{
  OssoABookGroup *group = osso_abook_all_group_get();
  OssoABookListStore *model;

  if (group == osso_abook_filter_model_get_group(data->filter_model))
    return;

  gtk_widget_hide(data->live_search);
  model = osso_abook_group_get_model(group);

  if (model)
  {
    osso_abook_tree_view_set_filter_model(
          OSSO_ABOOK_TREE_VIEW(data->contact_view), NULL);
    osso_abook_tree_view_set_base_model(
          OSSO_ABOOK_TREE_VIEW(data->contact_view), model);
  }
  else
  {
    osso_abook_tree_view_set_base_model(
          OSSO_ABOOK_TREE_VIEW(data->contact_view),
          OSSO_ABOOK_LIST_STORE(data->contact_model));
    osso_abook_tree_view_set_filter_model(
          OSSO_ABOOK_TREE_VIEW(data->contact_view), data->filter_model);
  }

  osso_abook_filter_model_freeze_refilter(data->filter_model);
  osso_abook_filter_model_set_text(data->filter_model, NULL);
  osso_abook_filter_model_set_group(data->filter_model, group);
  osso_abook_filter_model_thaw_refilter(data->filter_model);

  gtk_tree_selection_unselect_all(
        gtk_tree_view_get_selection(
          osso_abook_tree_view_get_tree_view(
            OSSO_ABOOK_TREE_VIEW(data->contact_view))));

  gtk_window_set_title(GTK_WINDOW(data->window),
                       osso_abook_group_get_display_title(group));

  if (OSSO_ABOOK_IS_SERVICE_GROUP(group))
  {
    gchar *s;
    gchar *service_name;

    g_object_get(group,
                 "service-name", &service_name,
                 NULL);

    s = g_strconcat(G_OBJECT_TYPE_NAME(group), ":", service_name, NULL);
    gconf_client_set_string(
          osso_abook_get_gconf_client(),
          "/apps/osso-addressbook/main-window/selected-group", s, NULL);
    g_free(service_name);
    g_free(s);
  }
  else
  {
    gconf_client_set_string(
          osso_abook_get_gconf_client(),
          "/apps/osso-addressbook/main-window/selected-group",
          G_OBJECT_TYPE_NAME(group), NULL);
  }
}

static void
_clean_something(osso_abook_data *data)
{
  if (data->rows_reorder_id)
  {
    g_source_remove(data->rows_reorder_id);
    data->rows_reorder_id = 0;
  }

  if (data->row)
  {
    gtk_tree_row_reference_free(data->row);
    data->row = 0;
  }

  g_free(data->selected_row_uid);
  data->selected_row_uid = NULL;
}

static void
set_scroll_adjustments_cb(GtkTreeView *tree_view, GtkAdjustment *hadjustment,
                          GtkAdjustment *vadjustment, gpointer user_data)
{
  osso_abook_data *data = user_data;

  if (gtk_tree_view_get_vadjustment(tree_view))
  {
    g_signal_handlers_disconnect_matched(
          gtk_tree_view_get_vadjustment(tree_view),
          G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
          0, 0, NULL, set_scroll_adjustments_cb, data);
  }

  if (vadjustment)
  {
    g_signal_connect_swapped(vadjustment, "value-changed",
                             G_CALLBACK(_clean_something), data);
  }
}

static gboolean
model_row_reorder_idle_cb(gpointer user_data)
{
  osso_abook_data *data = user_data;
  GtkTreeRowReference *row;
  GtkTreePath *path;
  GtkTreeView *view;
  GtkWidget *area;
  GdkRectangle visible_rect;
  GdkRectangle rect;
  gint y;

  row = data->row;
  data->rows_reorder_id = 0;

  if (!row)
    return FALSE;

  path = gtk_tree_row_reference_get_path(row);

  if (!path)
    return FALSE;

  view = osso_abook_tree_view_get_tree_view(
        OSSO_ABOOK_TREE_VIEW(data->contact_view));

  gtk_tree_view_get_background_area(view, path, NULL, &rect);
  gtk_tree_view_get_visible_rect(view, &visible_rect);

  if (rect.y < visible_rect.y ||
      rect.y + rect.height > visible_rect.y + visible_rect.height)
  {
    gtk_tree_view_convert_bin_window_to_tree_coords(view, 0, rect.y, NULL, &y);
    area = gtk_widget_get_ancestor(GTK_WIDGET(view), HILDON_TYPE_PANNABLE_AREA);
    hildon_pannable_area_jump_to(HILDON_PANNABLE_AREA(area), -1, y);
  }

  gtk_tree_path_free(path);

  return FALSE;
}

static void
model_rows_reorder_idle(osso_abook_data *data)
{
  if (!data->rows_reorder_id)
  {
    data->rows_reorder_id =
        gdk_threads_add_idle(model_row_reorder_idle_cb, data);
  }
}

void
select_contact_row(osso_abook_data *data, GtkTreePath *path)
{
  GtkTreeView *view = osso_abook_tree_view_get_tree_view(
        OSSO_ABOOK_TREE_VIEW(data->contact_view));

  _clean_something(data);

  data->row = gtk_tree_row_reference_new(gtk_tree_view_get_model(view), path);
  model_rows_reorder_idle(data);
}

static void
model_row_inserted_cb(OssoABookRowModel *tree_model, GtkTreePath *path,
                      GtkTreeIter *iter, osso_abook_data *user_data)
{
  if (user_data->selected_row_uid)
  {
    OssoABookListStoreRow *row =
        osso_abook_row_model_iter_get_row(tree_model, iter);

    if (row)
    {
      const char *uid =
          e_contact_get_const(E_CONTACT(row->contact), E_CONTACT_UID);

      if (uid)
      {
        if (!strcmp(uid, user_data->selected_row_uid))
        {
          select_contact_row(user_data, path);
          g_free(user_data->selected_row_uid);
          user_data->selected_row_uid = NULL;
        }
      }
    }
  }
}

static void
model_row_reordered_cb(GtkTreeModel *tree_model, GtkTreePath *path,
                       GtkTreeIter *iter, gpointer new_order,
                       gpointer user_data)
{
  model_rows_reorder_idle(user_data);
}

static void
notify_model_cb(GtkTreeView *tree_view, GParamSpec *pspec,
                osso_abook_data *data)
{
  if (data->model)
  {
    g_object_remove_weak_pointer(G_OBJECT(data->model),
                                 (gpointer *)&data->model);
    g_signal_handlers_disconnect_matched(
          data->model, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
          model_row_inserted_cb, data);
    g_signal_handlers_disconnect_matched(
          data->model, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
          model_row_reordered_cb, data);
  }

  if (data->row)
  {
    gtk_tree_row_reference_free(data->row);
    data->row = NULL;
  }

  if (data->rows_reorder_id)
  {
    g_source_remove(data->rows_reorder_id);
    data->rows_reorder_id = 0;
  }

  data->model = gtk_tree_view_get_model(tree_view);

  if (data->model)
  {
    g_object_add_weak_pointer(G_OBJECT(data->model), (gpointer *)&data->model);
    g_signal_connect(data->model, "row-inserted",
                     G_CALLBACK(model_row_inserted_cb), data);
    g_signal_connect(data->model, "rows-reordered",
                     G_CALLBACK(model_row_reordered_cb), data);
  }
}

static void
cursor_changed_cb(GtkTreeView *tree_view, osso_abook_data *data)
{
  g_free(data->selected_row_uid);
  data->selected_row_uid = NULL;
}

static gboolean
window_delete_event_cb(GtkWidget *widget, GdkEvent *event,
                       osso_abook_data *data)
{
  if (data->quit_on_close)
    app_exit_main_loop(data);

  gtk_widget_hide(widget);
  data->startup_complete = FALSE;
  data->one_day_timer_id =
      gdk_threads_add_timeout_seconds(900, one_day_expired_cb, data);
  gtk_widget_hide(data->live_search);

  if (data->recent_view)
    osso_abook_recent_view_hide_live_search(data->recent_view);

  data->recent_view_scroll_once = TRUE;
  data->contact_view_scroll_once = TRUE;

  return TRUE;
}

struct import_operation
{
  gchar **argv;
  int argc;
  int idx;
};

static void
import_operation_free(struct import_operation *op)
{
  int i;

  for (i = 0; i < op->argc; i++)
    g_free(op->argv[i]);

  g_free(op->argv);
  g_free(op);
}

static gboolean
import_operation_equal(struct import_operation *op1,
                       struct import_operation *op2)
{
  int i;

  g_return_val_if_fail(op1 && op2, FALSE);

  if (op1->argc != op2->argc)
    return FALSE;

  for (i = 0; i < op1->argc; i++)
  {
    if (!g_str_equal(op1->argv[i], op2->argv[i]))
      return FALSE;
  }

  return TRUE;
}

static gboolean
import_finished_cb(gpointer user_data)
{
  return idle_import(user_data);
}

static gboolean
idle_import(gpointer user_data)
{
  osso_abook_data *data = user_data;
  GtkWidget *note;

  while (data->import_operations)
  {
    gboolean import_started = FALSE;
    struct import_operation *op = data->import_operations->data;

    note = hildon_note_new_confirmation(NULL,
                                        dgettext(0, "addr_nc_notification15"));

    if (gtk_dialog_run(GTK_DIALOG(note)) == GTK_RESPONSE_OK)
    {
      gtk_widget_destroy(note);
      do_import(0, op->argv[op->idx], import_finished_cb, data);
      import_started = TRUE;
    }
    else
      gtk_widget_destroy(note);

    op->idx++;

    if (op->idx >= op->argc)
    {
      import_operation_free(op);
      data->import_operations = g_list_remove_link(data->import_operations,
                                                   data->import_operations);
    }

    if (import_started)
      return FALSE;
  }

  data->import_started = FALSE;

  return FALSE;
}

static void
new_import_operation(gpointer user_data, int argc, gchar **argv)
{
  osso_abook_data *data = user_data;
  struct import_operation *op;
  GList *l;
  int i;

  if (!osso_abook_check_disc_space(GTK_WINDOW(data->window)))
    return;

  g_return_if_fail(argc > 0);

  op = g_new(struct import_operation, 1);
  op->argc = argc;
  op->idx = 0;
  op->argv = g_new(gchar *, argc);

  for (i = 0; i < argc; i++)
  {
    char *arg = argv[i];

    if (strncmp(arg, "file://", 7))
      op->argv[i] = g_strdup(arg);
    else
      op->argv[i] = g_filename_to_uri(arg, NULL, NULL);
  }

  qsort(op->argv, op->argc, sizeof(op->argv[0]), (__compar_fn_t)g_strcmp0);

  for (l = data->import_operations; l; l = l->next)
  {
    if (import_operation_equal(op, l->data))
    {
      import_operation_free(op);
      return;
    }
  }

  data->import_operations = g_list_append(data->import_operations, op);

  if (data->unk1)
  {
    if (!data->import_started)
    {
      data->import_started = TRUE;
      gdk_threads_add_idle(idle_import, data);
    }
  }
}

static void
aggregator_ebook_status_cb(OssoABookAggregator *aggregator, EBookStatus status,
                           osso_abook_data *data)
{
  if (status == E_BOOK_ERROR_OK)
    return;

  osso_abook_handle_estatus(
        GTK_WINDOW(data->window), status,
        osso_abook_roster_get_book(OSSO_ABOOK_ROSTER(aggregator)));
  osso_abook_list_store_cancel_loading(
        OSSO_ABOOK_LIST_STORE(data->contact_model));
}

static void
set_title(osso_abook_data *data)
{
  if (data->contacts_mode != 1)
  {
    gtk_window_set_title(
          GTK_WINDOW(data->window),
          osso_abook_group_get_display_title(osso_abook_all_group_get()));
  }
}

static void
aggregator_sequence_complete_cb(OssoABookRoster *roster, guint status,
                                gpointer user_data)
{
  osso_abook_data *data = user_data;

  g_signal_handler_disconnect(data->aggregator, data->sequence_complete_id);

  data->unk1 = 1;

  if (data->import_operations && !data->import_started )
  {
    data->import_started = TRUE;
    gdk_threads_add_idle(idle_import, data);
  }

  set_title(data);
  update_menu(data);
}

static void
aggregator_master_contact_count_cb(GObject *gobject, GParamSpec *pspec,
                                   gpointer user_data)
{
  osso_abook_data *data = user_data;

  set_title(data);
  update_menu(data);
}

static void
window_realize_cb(GtkWidget *widget, gpointer user_data)
{
  osso_abook_data *data = user_data;

  set_active_toggle_button(data);

  if (!osso_abook_waitable_is_ready(
        OSSO_ABOOK_WAITABLE(data->aggregator), NULL))
  {
    hildon_gtk_window_set_progress_indicator(GTK_WINDOW(data->window), TRUE);
  }
}

static gboolean
widget_hide_idle_cb(GtkWidget *widget)
{
  gtk_widget_hide(widget);
  return FALSE;
}

static void
starter_window_destroy_cb(int unused, osso_abook_data *data)
{
  if (data->starter_window != NULL)
    data->starter_window = NULL;
}

static void
action_started_cb(OssoABookTouchContactStarter *starter, osso_abook_data *data)
{
  OssoABookContactAction action;
  const char *action_name;

  action = osso_abook_touch_contact_starter_get_started_action(starter);

  action_name = osso_abook_contact_action_get_name(action);
  OSSO_ABOOK_NOTE(GENERIC, "TouchContactStarter::action-started for %s",
                  action_name);

  if (action == OSSO_ABOOK_CONTACT_ACTION_CREATE_ACCOUNT)
    return;

  if (data->starter_window)
  {
    gtk_widget_destroy(data->starter_window);
    data->starter_window = NULL;
  }
}

static void
contact_deleted_cb(int unused, osso_abook_data *data)
{
  if (data->starter_window)
  {
    gtk_widget_destroy(data->starter_window);
    data->starter_window = NULL;
  }
}

static void
editor_started_cb(int unused, gpointer instance, gpointer data)
{
  g_signal_connect_after(instance, "contact-saved",
                         G_CALLBACK(contact_saved_cb), data);
  g_signal_connect(instance, "contact-deleted",
                   G_CALLBACK(contact_deleted_cb), data);
}

static void
gobj_set_visible(GObject *obj, const gchar *key, gboolean state)
{
  gpointer data = g_object_get_data(G_OBJECT(obj), key);
  if (data)
    g_object_set(data, "visible", state, NULL);
}

static gboolean
should_auth_request(OssoABookContact *master_contact)
{
  const char *st_pen, *st_no;
  char *contact_val;
  GList *contact_list;
  OssoABookContact *contact;
  gboolean ret;

  st_pen = osso_abook_presence_state_get_nick(
      OSSO_ABOOK_PRESENCE_STATE_REMOTE_PENDING);

  st_no = osso_abook_presence_state_get_nick(
      OSSO_ABOOK_PRESENCE_STATE_NO);

  if (osso_abook_contact_is_roster_contact(master_contact))
  {
    contact_val = osso_abook_contact_get_value(E_CONTACT(master_contact),
                                               "X-TELEPATHY-SUBSCRIBED");
    if (!contact_val)
      ret = FALSE;
    else
    {
      ret = g_str_equal(contact_val, st_pen) || g_str_equal(contact_val, st_no);
      g_debug("Show authorization request? %d (%s)", ret, contact_val);
      g_free(contact_val);
    }
  }
  else
  {
    contact_list = osso_abook_contact_get_roster_contacts(master_contact);

    if (contact_list && (contact = contact_list->data) != 0)
    {
      while ( !should_auth_request(contact) )
      {
        contact_list = contact_list->next;

        if (contact_list)
        {
          contact = contact_list->data;
          if (contact_list->data)
            continue;
        }
        g_list_free(contact_list);
        return FALSE;
      }
      ret = TRUE;
    }

    g_list_free(contact_list);
  }
  return ret;
}

static void
history_query(GObject *obj, osso_abook_data *data)
{
  OssoABookContact *starter_contact, *c;
  gboolean merge_btn, history_btn;
  RTComEl *rtcomel;
  RTComElQuery *comm_hist_query;
  RTComElIter *events;
  GList *master_contacts_list;

  starter_contact = get_starter_contact(data);

  gobj_set_visible(obj, "send-card-button",
                   !OSSO_ABOOK_IS_VOICEMAIL_CONTACT(starter_contact));

  gobj_set_visible(obj, "send-detail-button",
                   !OSSO_ABOOK_IS_VOICEMAIL_CONTACT(starter_contact));

  if (starter_contact && OSSO_ABOOK_IS_VOICEMAIL_CONTACT(starter_contact))
    merge_btn = FALSE;
  else
  {
    master_contacts_list = osso_abook_aggregator_list_master_contacts(
        OSSO_ABOOK_AGGREGATOR(data->aggregator));

    if (!master_contacts_list)
    {
      merge_btn = FALSE;
      goto listfree;
    }

    while (1)
    {
      c = master_contacts_list->data;

      if (starter_contact != c && (!c || !OSSO_ABOOK_IS_VOICEMAIL_CONTACT(c)))
        break;

      master_contacts_list = master_contacts_list->next;
      if (!master_contacts_list)
      {
        merge_btn = FALSE;
        goto listfree;
      }
    }
    merge_btn = TRUE;

listfree:
    g_list_free(master_contacts_list);
  }

  gobj_set_visible(obj, "merge-button", merge_btn);
  gobj_set_visible(obj, "req-auth-button", should_auth_request(starter_contact));
  gobj_set_visible(obj, "shortcut-button", osso_abook_contact_shortcut_exists(
                                              starter_contact, NULL) == FALSE);

  rtcomel = rtcom_el_get_shared();
  comm_hist_query = create_communication_history_query(starter_contact,
                                                       rtcomel);

  if (comm_hist_query)
  {
    events = rtcom_el_get_events(rtcomel, comm_hist_query);

    if (events)
    {
      history_btn = rtcom_el_iter_first(events) != FALSE;
      g_object_unref(comm_hist_query);
      g_object_unref(events);
    }
    else
    {
      g_object_unref(comm_hist_query);
      history_btn = FALSE;
    }
  }
  else
  {
    g_warning("error preparing communication history query");
    history_btn = FALSE;
  }

  g_object_unref(rtcomel);
  gobj_set_visible(obj, "comm-history-button", history_btn);
}

static void
toggle_menu(GObject *obj, osso_abook_data *data)
{
  GtkWidget *wid = g_object_get_data(G_OBJECT(obj), "sim-merge-bt");

  if (osso_abook_aggregator_get_master_contact_count(
          OSSO_ABOOK_AGGREGATOR(data->aggregator)))
    gtk_widget_show(wid);
  else
    gtk_widget_hide(wid);
}

void
create_menu(osso_abook_data *data, OssoABookMenuEntry *entries,
            int entries_count, OssoABookContact *contact)
{
  HildonAppMenu *menu;
  GtkWidget *alignment, *contact_starter;
  void (*map_cb)(void);
  GtkAccelGroup *accel_group;

  if (data->starter_window)
    return;

  data->starter_window = hildon_stackable_window_new();
  accel_group = gtk_accel_group_new();
  gtk_window_set_transient_for(GTK_WINDOW(data->starter_window),
                               GTK_WINDOW(data->window));
  gtk_window_add_accel_group(GTK_WINDOW(data->window), accel_group);

  g_signal_connect(data->starter_window, "destroy",
                   G_CALLBACK(starter_window_destroy_cb), data);

  menu = app_menu_from_menu_entries(accel_group,
                                    entries, entries_count,
                                    0, 0, data, 0);

  append_menu_extension_entries(menu,
                                "osso-abook-contact-view",
                                GTK_WINDOW(data->starter_window),
                                contact, data);

  if (entries == contact_menu_actions)
    map_cb = (void (*)(void))history_query;
  else
    map_cb = (void (*)(void))toggle_menu;

  g_signal_connect(menu, "map", map_cb, data);

  if (entries == sim_bt_menu_actions)
    return;

  hildon_window_set_app_menu(HILDON_WINDOW(data->starter_window), menu);
  g_object_unref(accel_group);
  alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0, 16u, 16u);
  gtk_container_add(GTK_CONTAINER(data->starter_window), alignment);
  gtk_widget_show(alignment);

  app_menu_set_disable_on_lowmem(menu, "edit-contact-button", TRUE);

  if (hw_is_lowmem_mode())
    contact_starter = osso_abook_touch_contact_starter_new_with_contact(
                          GTK_WINDOW(data->starter_window), contact);
  else
    contact_starter = osso_abook_touch_contact_starter_new_with_editor(
                          GTK_WINDOW(data->starter_window), contact);

  g_signal_connect(contact_starter, "action-started",
                   G_CALLBACK(action_started_cb), data);
  g_signal_connect(contact_starter, "editor-started",
                   G_CALLBACK(editor_started_cb), data);

  g_object_set_data(G_OBJECT(data->starter_window), "starter",
                    contact_starter);

  gtk_container_add(GTK_CONTAINER(alignment), contact_starter);
  gtk_widget_show(contact_starter);
  gtk_widget_show(data->starter_window);
}

static void
contact_view_contact_activated_cb(OssoABookContactView *view,
                                  OssoABookContact *master_contact,
                                  osso_abook_data *user_data)
{
  if (user_data->stacked_group)
    g_idle_add((GSourceFunc)widget_hide_idle_cb, user_data->field_44);

  g_idle_add((GSourceFunc)widget_hide_idle_cb, user_data->live_search);
  create_menu(user_data, contact_menu_actions,
              G_N_ELEMENTS(contact_menu_actions), master_contact);
}

static void
sim_group_available_cb(OssoABookSimGroup *sim_group, osso_abook_data *data)
{
  if (!data->sim_group_ready)
  {
    data->sim_group_ready = TRUE;
    update_view_groups_accounts(data);
  }
}

static void
vm_contact_async_commit_cb(EBook *book, EBookStatus status, gpointer closure)
{
  gchar *imsi = closure;

  if (!status && imsi && *imsi)
  {
    gconf_client_set_string(osso_abook_get_gconf_client(),
                            OSSO_ABOOK_SETTINGS_KEY_LAST_IMSI, imsi, NULL);
  }
  else
    g_warning("failed to commit (new) voicemail contact");

  g_free(imsi);
}

static void
sim_import_contact_as_voicemail(OssoABookContact *contact, const char *imsi)
{
  GSList *numbers = osso_abook_settings_get_voicemail_numbers();
  GList *attrs = NULL;
  GList *l;
  GList *tel = NULL;
  OssoABookVoicemailContact *vmc;

  if (contact)
    attrs = e_vcard_get_attributes(E_VCARD(contact));

  for (l = g_list_last(attrs); l; l = l->prev)
  {
    if (!g_strcmp0(e_vcard_attribute_get_name(l->data), EVC_TEL))
    {
      GList *v = e_vcard_attribute_get_values(l->data);

      if (v)
      {
        const char *phone_number = v->data;

        if (phone_number && *phone_number)
        {
          OssoABookVoicemailNumber *vmn;

          vmn = osso_abook_voicemail_number_new(phone_number, imsi, NULL);
          numbers = g_slist_prepend(numbers, vmn);
          break;
        }
      }
    }
  }

  if (numbers)
  {
    tel = g_list_prepend(
          NULL, ((OssoABookVoicemailNumber *)(numbers->data))->phone_number);
  }

  if (osso_abook_settings_set_voicemail_numbers(numbers) &&
      (vmc = osso_abook_voicemail_contact_get_default()))
  {
    e_contact_set(E_CONTACT(vmc), E_CONTACT_TEL, tel);
    osso_abook_contact_async_commit(OSSO_ABOOK_CONTACT(vmc), NULL,
                                    vm_contact_async_commit_cb, g_strdup(imsi));
    g_object_unref(vmc);
  }

  osso_abook_voicemail_number_list_free(numbers);
  g_list_free(tel);
}

static void
import_voicemail_contact(osso_abook_data *data)
{
  OssoABookContact *sim_vm_contact;
  const gchar *imsi;
  gchar *last_imsi;

  if (!data->voicemail_contact_available || !data->sim_capabilities_available)
    return;

  sim_vm_contact = osso_abook_sim_group_get_voicemail_contact(
        OSSO_ABOOK_SIM_GROUP(data->sim_group));

  imsi = osso_abook_sim_group_get_imsi(OSSO_ABOOK_SIM_GROUP(data->sim_group));

  last_imsi = gconf_client_get_string(osso_abook_get_gconf_client(),
                                      OSSO_ABOOK_SETTINGS_KEY_LAST_IMSI, NULL);
  if (imsi && last_imsi && g_strcmp0(last_imsi, imsi))
  {
    if (!osso_abook_gconf_contact_is_deleted(
          OSSO_ABOOK_GCONF_CONTACT(osso_abook_voicemail_contact_get_default())))
    {
      sim_import_contact_as_voicemail(sim_vm_contact, imsi);
    }
  }

  g_free(last_imsi);
}

static void
sim_group_voicemail_contact_available_cb(OssoABookSimGroup *sim_group,
                                         osso_abook_data *data)
{
  OssoABookContact *voicemail_contact;

  data->voicemail_contact_available = TRUE;
  import_voicemail_contact(data);
  voicemail_contact = osso_abook_sim_group_get_voicemail_contact(sim_group);

  if (voicemail_contact)
    OSSO_ABOOK_DUMP_VCARD(EDS, voicemail_contact, "voicemail contact");
  else
    OSSO_ABOOK_NOTE(EDS, "SIM card voicemail box empty");
}

static void
sim_group_capabilities_available_cb(OssoABookSimGroup *sim_group,
                                    osso_abook_data *data)
{
  data->sim_capabilities_available = TRUE;
  import_voicemail_contact(data);

  OSSO_ABOOK_NOTE(EDS, "SIM IMSI available: %s",
                  osso_abook_sim_group_get_imsi(sim_group));
}

static void
sim_group_ready_cb(OssoABookWaitable *waitable, const GError *error,
                   gpointer data)
{
  OSSO_ABOOK_NOTE(EDS, "SIM group ready (all SIM books available)");
}

static void
live_search_data_free(live_search_data *data)
{
  if (data->idle_scroll_id)
    g_source_remove(data->idle_scroll_id);

  g_signal_handlers_disconnect_by_data(data->live_search, data);
  g_signal_handlers_disconnect_by_data(data->tree_view, data);

  if (data->row_ref)
    gtk_tree_row_reference_free(data->row_ref);

  g_free(data);
}

static void
row_tapped_cb(GtkTreeView *tree_view, GtkTreePath *treepath,
              live_search_data *data)
{
  GtkTreeModel *model;
  GdkRectangle visible_rect, rect;
  float rect_h, rect_y;

  if (data->row_ref)
  {
    gtk_tree_row_reference_free(data->row_ref);
    data->row_ref = NULL;
  }

  model = gtk_tree_view_get_model(tree_view);
  data->row_ref = gtk_tree_row_reference_new(model, treepath);
  gtk_tree_view_get_visible_rect(tree_view, &visible_rect);
  gtk_tree_view_get_background_area(tree_view, treepath, NULL, &rect);

  if (gtk_widget_get_mapped(data->live_search))
    visible_rect.height += data->live_search->allocation.height;

  rect_h = visible_rect.height - rect.height;

  if (rect_h <= 0.0)
    rect_y = 0.0;
  else
    rect_y = rect.y;

  if (rect_h > 0.0)
    rect_y = rect_y / rect_h;

  data->row_align = rect_y;
}


static int
live_search_scroll(live_search_data *data)
{
  GtkTreeView *tv;
  GtkTreePath *treepath;

  data->idle_scroll_id = 0;
  tv = osso_abook_tree_view_get_tree_view(data->tree_view);
  g_signal_handlers_disconnect_matched(tv,
                                       G_SIGNAL_MATCH_DATA|G_SIGNAL_MATCH_FUNC,
                                       0, 0, NULL, row_tapped_cb, data);

  if (data->row_ref)
  {
    treepath = gtk_tree_row_reference_get_path(data->row_ref);
    if (treepath)
    {
      tv = osso_abook_tree_view_get_tree_view(data->tree_view);
      gtk_tree_view_scroll_to_cell(tv, treepath, 0, 1, data->row_align, 0.0);
      gtk_tree_path_free(treepath);
    }
  }

  return 0;
}

static gulong
live_search_show_cb(int unused, live_search_data *data)
{
  if (data->row_ref)
  {
    gtk_tree_row_reference_free(data->row_ref);
    data->row_ref = NULL;
  }

  return g_signal_connect(osso_abook_tree_view_get_tree_view(data->tree_view),
                          "hildon-row-tapped",
                          G_CALLBACK(row_tapped_cb),
                          data);
}

static void
live_search_hide_cb(int unused, live_search_data *data)
{
  if (!data->idle_scroll_id)
    data->idle_scroll_id = g_idle_add((GSourceFunc)live_search_scroll, data);
}

GtkWidget *
_setup_live_search(HildonWindow *parent, OssoABookTreeView *tree_view)
{
  OssoABookFilterModel *filter_model;
  live_search_data *data;

  filter_model = osso_abook_tree_view_get_filter_model(tree_view);
  data = g_malloc0(sizeof(live_search_data));
  data->tree_view = tree_view;
  data->live_search = osso_abook_live_search_new_with_filter(filter_model);
  hildon_window_add_toolbar(parent, GTK_TOOLBAR(data->live_search));
  hildon_live_search_widget_hook(HILDON_LIVE_SEARCH(data->live_search),
                                 GTK_WIDGET(parent),
                                 GTK_WIDGET(osso_abook_tree_view_get_tree_view(
                                            tree_view)));

  g_signal_connect_data(data->live_search, "show",
                        G_CALLBACK(live_search_show_cb),
                        data,
                        (GClosureNotify)live_search_data_free,
                        0);

  g_signal_connect_after(data->live_search, "hide",
                         G_CALLBACK(live_search_hide_cb), data);

  return data->live_search;
}

gboolean
app_create(osso_context_t *osso, const gchar *arg1, osso_abook_data *data)
{
  GtkAccelGroup *accel_group;
  GtkTreeView *tree_view;
  GConfClient *gconf;
  int list_mode;
  GConfValue *val;
  int contacts_mode;
  GError *error = NULL;

  OSSO_ABOOK_LOCAL_TIMER_START(STARTUP, NULL);
  OSSO_ABOOK_NOTE(STARTUP, STARTUP_PROGRESS_SEPARATOR);

  g_set_application_name(dgettext(0, "addr_ap_address_book"));
  data->osso = osso;
  data->sim_group_ready = FALSE;
  osso_mime_set_cb(osso, new_import_operation, data);

  hw_start_monitor(data);

  if (arg1)
    data->arg1 = g_strdup(arg1);

  g_signal_connect(osso_abook_roster_manager_get_default(), "roster-created",
                   G_CALLBACK(roster_created_cb), data);
  g_signal_connect(osso_abook_roster_manager_get_default(), "roster-removed",
                   G_CALLBACK(roster_removed_cb), data);
  g_signal_connect(osso_abook_account_manager_get_default(), "account-removed",
                   G_CALLBACK(account_removed_cb), data);

  OSSO_ABOOK_NOTE(STARTUP, STARTUP_PROGRESS_SEPARATOR);

  data->aggregator = osso_abook_aggregator_get_default(&error);

  if (data->aggregator)
  {
    g_signal_connect(data->aggregator, "notify::master-contact-count",
                     G_CALLBACK(aggregator_master_contact_count_cb), data);
    g_signal_connect(data->aggregator, "ebook-status",
                     G_CALLBACK(aggregator_ebook_status_cb), data);
    data->sequence_complete_id =
        g_signal_connect(data->aggregator, "sequence-complete",
                         G_CALLBACK(aggregator_sequence_complete_cb), data);
    g_object_set(osso_abook_all_group_get(),
                 "aggregator", data->aggregator,
                 NULL);
  }
  else
    osso_abook_handle_gerror(GTK_WINDOW(data->window), error);

  desktop_service_init(data);

  OSSO_ABOOK_NOTE(STARTUP, STARTUP_PROGRESS_SEPARATOR);

  data->contacts_mode = 0;
  accel_group = gtk_accel_group_new();
  data->window = HILDON_STACKABLE_WINDOW(hildon_stackable_window_new());
  gtk_window_add_accel_group(GTK_WINDOW(data->window), accel_group);
  g_signal_connect(data->window, "realize",
                   G_CALLBACK(window_realize_cb), data);

  data->main_menu = app_menu_from_menu_entries(
        accel_group,
        main_menu_actions,  MENU_ACTIONS_COUNT,
        main_menu_filters, 3,
        data, NULL);

  append_menu_extension_entries(data->main_menu, "osso-abook-main-view",
                                GTK_WINDOW(data->window), 0, data);

  app_menu_set_disable_on_lowmem(data->main_menu, "export-bt", TRUE);
  app_menu_set_disable_on_lowmem(data->main_menu, "delete-bt", TRUE);
  app_menu_set_disable_on_lowmem(data->main_menu, "groups-bt", TRUE);
  app_menu_set_disable_on_lowmem(data->main_menu, "new-contact-bt", TRUE);
  app_menu_set_disable_on_lowmem(data->main_menu, "import-bt", TRUE);

  g_object_unref(accel_group);

  hildon_program_add_window(hildon_program_get_instance(),
                            HILDON_WINDOW(data->window));
  gtk_widget_set_size_request(GTK_WIDGET(data->window), 720, -1);
  g_signal_connect(data->window, "delete-event",
                   G_CALLBACK(window_delete_event_cb), data);
  g_signal_connect(data->window, "notify::is-topmost",
                   G_CALLBACK(_window_is_topmost_cb), data);

  OSSO_ABOOK_NOTE(STARTUP, STARTUP_PROGRESS_SEPARATOR);

  data->sim_group = osso_abook_sim_group_new();

  g_signal_connect(data->sim_group, "available",
                   G_CALLBACK(sim_group_available_cb), data);
  g_signal_connect(data->sim_group, "voicemail-contact-available",
                   G_CALLBACK(sim_group_voicemail_contact_available_cb), data);
  g_signal_connect(data->sim_group, "capabilities-available",
                   G_CALLBACK(sim_group_capabilities_available_cb),data);
  osso_abook_waitable_call_when_ready(OSSO_ABOOK_WAITABLE(data->sim_group),
                                      sim_group_ready_cb, data, NULL);

  OSSO_ABOOK_NOTE(STARTUP, STARTUP_PROGRESS_SEPARATOR);

  data->contact_model = osso_abook_contact_model_get_default();
  data->filter_model = osso_abook_filter_model_new(
        OSSO_ABOOK_LIST_STORE(data->contact_model));
  data->align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);

  gtk_alignment_set_padding(GTK_ALIGNMENT(data->align), 4, 0, 16, 8);

  data->contact_view = osso_abook_contact_view_new(HILDON_UI_MODE_NORMAL,
                                                   data->contact_model,
                                                   data->filter_model);

  g_object_ref_sink(data->contact_view);
  g_object_unref(data->filter_model);
  gtk_container_add(GTK_CONTAINER(data->align), GTK_WIDGET(data->contact_view));
  g_object_set(data->contact_view, "can-focus", FALSE, NULL);
  g_signal_connect(data->contact_view, "contact-activated",
    G_CALLBACK(contact_view_contact_activated_cb), data);
  gtk_widget_grab_focus(GTK_WIDGET(data->contact_view));

  data->live_search =
      _setup_live_search(HILDON_WINDOW(data->window),
                         OSSO_ABOOK_TREE_VIEW(data->contact_view));
  tree_view = osso_abook_tree_view_get_tree_view(
        OSSO_ABOOK_TREE_VIEW(data->contact_view));

  g_signal_connect(tree_view, "set-scroll-adjustments",
                   G_CALLBACK(set_scroll_adjustments_cb), data);
  g_signal_connect(tree_view, "notify::model",
                   G_CALLBACK(notify_model_cb), data);
  g_signal_connect(tree_view, "cursor-changed",
                   G_CALLBACK(cursor_changed_cb), data);

  set_scroll_adjustments_cb(tree_view, gtk_tree_view_get_hadjustment(tree_view),
                            gtk_tree_view_get_vadjustment(tree_view), data);
  notify_model_cb(tree_view, NULL, data);

  gtk_container_add(GTK_CONTAINER(data->window), data->align);

  OSSO_ABOOK_NOTE(STARTUP, STARTUP_PROGRESS_SEPARATOR);

  app_select_all_group(data);

  gconf = osso_abook_get_gconf_client();

  val = gconf_client_get(gconf, "/apps/osso-addressbook/list-mode", NULL);

  if (val)
  {
    list_mode = gconf_value_get_int(val);
    gconf_value_free(val);
  }
  else
    list_mode = 1;

  osso_abook_tree_view_set_avatar_view(OSSO_ABOOK_TREE_VIEW(data->contact_view),
                                       list_mode);

  OSSO_ABOOK_NOTE(STARTUP, STARTUP_PROGRESS_SEPARATOR);
  OSSO_ABOOK_NOTE(STARTUP, STARTUP_PROGRESS_SEPARATOR);

  data->dialog_open = FALSE;

  gtk_widget_show_all(data->align);
  data->one_day_timer_id =
      gdk_threads_add_timeout_seconds(24 * 60 * 60, one_day_expired_cb, NULL);

  if (data->show_ui)
    app_show(data);

  gtk_widget_hide(data->live_search);

  val = gconf_client_get(gconf, "/apps/osso-addressbook/contacts-mode", NULL);

  if (val)
  {
    contacts_mode = gconf_value_get_int(val);
    gconf_value_free(val);
  }
  else
    contacts_mode = 0;

  set_contacts_mode(data, contacts_mode);

  OSSO_ABOOK_NOTE(STARTUP, STARTUP_PROGRESS_SEPARATOR);
  OSSO_ABOOK_LOCAL_TIMER_END();

  return TRUE;
}

void
app_destroy(osso_abook_data *data)
{
  if (data->aggregator)
    osso_abook_roster_stop(data->aggregator);

  if (data->contact_view)
    g_object_unref(data->contact_view);

  if (data->recent_view)
    g_object_unref(data->recent_view);

  if (hw_is_under_valgrind())
  {
    g_message("%s: Valgrind detected, so destroying app window.", __FUNCTION__);
    gtk_widget_destroy(GTK_WIDGET(data->window));
  }

  g_slist_free(data->service_groups);

  if (data->arg1)
    g_free(data->arg1);

  _clean_something(data);

  if (data->plugin_manager)
    g_object_unref(data->plugin_manager);

  desktop_service_finalize();
  hw_stop_monitor(data);
}

void
app_exit_main_loop(osso_abook_data *data)
{
  gtk_main_quit();
}

static void
get_your_contacts_dialog_show(GtkWidget *live_search, osso_abook_data *data)
{
  GtkWidget *dialog;

  g_signal_handlers_disconnect_matched(
        live_search, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
        get_your_contacts_dialog_show, data);

  dialog = osso_abook_get_your_contacts_dialog_new(data);
  g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  gtk_widget_show(dialog);
}

static void
check_if_no_contacts(OssoABookWaitable *waitable, const GError *error,
                     gpointer user_data)
{
  osso_abook_data *data = user_data;

  if (gtk_widget_get_realized(GTK_WIDGET(data->window)))
    hildon_gtk_window_set_progress_indicator(GTK_WINDOW(data->window), FALSE);

  g_return_if_fail(data->startup_complete);

  if (data->dialog_open || osso_abook_get_your_contacts_dialog_already_shown())
    return;

  if (osso_abook_waitable_is_ready(
        OSSO_ABOOK_WAITABLE(data->aggregator), NULL))
  {
    GList *contacts = osso_abook_aggregator_list_master_contacts(
          OSSO_ABOOK_AGGREGATOR(data->aggregator));
    GList *l;

    for (l = contacts; l; l = l->next)
    {
      if (!OSSO_ABOOK_IS_GCONF_CONTACT(l->data))
        goto out;
    }

    if (data->live_search && gtk_widget_get_visible(data->live_search))
    {
      g_signal_connect(data->live_search, "hide",
                       G_CALLBACK(get_your_contacts_dialog_show), data);
      goto out;
    }

    get_your_contacts_dialog_show(data->live_search, data);

out:
    g_list_free(contacts);
    hildon_window_set_app_menu(HILDON_WINDOW(data->window), data->main_menu);
    set_title(data);
  }
}

void
app_show(osso_abook_data *data)
{
  if (data->one_day_timer_id)
  {
    g_source_remove(data->one_day_timer_id);
    data->one_day_timer_id = 0;
  }

  gtk_window_present(GTK_WINDOW(data->window));

  scroll_to_top_if_needed(data);

  if (!data->startup_complete)
  {
    data->startup_complete = TRUE;
    osso_abook_waitable_call_when_ready(OSSO_ABOOK_WAITABLE(data->aggregator),
                                        check_if_no_contacts, data, NULL);
  }
}

const char *
_get_vcard_field_from_uri(const gchar *uri)
{
  if (g_str_has_prefix(uri, "mailto:"))
    return "email";

  if (g_str_has_prefix(uri, "xmpp:"))
    return "x-jabber";

  if (g_str_has_prefix(uri, "sipto:") || g_str_has_prefix(uri, "sip:"))
    return "x-sip";

  if (g_str_has_prefix(uri, "callto:") || g_str_has_prefix(uri, "tel:") ||
      g_str_has_prefix(uri, "sms:"))
    return "tel";

  OSSO_ABOOK_WARN("Unsupported URI: %s", uri);

  return NULL;
}
