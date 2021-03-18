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
#include <libosso-abook/osso-abook-gconf-contact.h>
#include <libosso-abook/osso-abook-contact-view.h>
#include <libosso-abook/osso-abook-debug.h>
#include <libosso-abook/osso-abook-row-model.h>

#include <libintl.h>
#include <string.h>

#include "app.h"
#include "service.h"
#include "groups.h"
#include "contacts.h"
#include "importer.h"
#include "menu.h"
#include "hw.h"

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
  GtkObject *contact_view;
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
  gkey_set_focus(gkeyfile, "ContactView", GTK_WIDGET(contact_view));
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

static void
window_is_topmost_cb(HildonWindow *window, GParamSpec *pspec,
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

  data->field_B8 = TRUE;
  data->field_B4 = TRUE;

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
contact_view_contact_activated_cb(OssoABookContactView *view,
                                  OssoABookContact *master_contact,
                                  osso_abook_data *user_data)
{
  if (user_data->stacked_group)
    g_idle_add((GSourceFunc)widget_hide_idle_cb, user_data->field_44);

  g_idle_add((GSourceFunc)widget_hide_idle_cb, user_data->live_search);
  create_menu(user_data, contact_menu_actions[8], CONTACT_MENU_COUNT,
              master_contact);
}

static void
sim_group_available_cb(OssoABookSimGroup *sim_group, osso_abook_data *data)
{
  if ( !data->sim_group_ready )
  {
    data->sim_group_ready = TRUE;
    update_view_groups_accounts(data);
  }
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

  data->main_menu = app_menu_from_menu_entries(accel_group,
                                               main_menu_actions, 7,
                                               main_menu_filters, 3,
                                               data, 0);

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
                   G_CALLBACK(window_is_topmost_cb), data);

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
