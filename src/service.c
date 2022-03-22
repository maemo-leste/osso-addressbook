/*
 * service.c
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

#include <libosso-abook/osso-abook-contact-chooser.h>
#include <libosso-abook/osso-abook-init.h>
#include <libosso-abook/osso-abook-log.h>
#include <libosso-abook/osso-abook-service-group.h>
#include <libosso-abook/osso-abook-temporary-contact-dialog.h>
#include <libosso-abook/osso-abook-waitable.h>

#include "app.h"
#include "menu.h"

#include "service.h"

static GQuark com_nokia_osso_addressbook_quark;
static GQuark top_application_quark;
static GQuark add_account_quark;
static GQuark add_shortcut_quark;
static GQuark can_add_shortcut_quark;
static GQuark search_append_quark;
static GQuark select_contacts_quark;
static GQuark open_group_quark;
static GQuark response_quark;
static GQuark release_quark;
static GQuark osso_abook_object_owner_quark;
static GQuark osso_abook_object_path_quark;

void
desktop_service_finalize()
{}

static const char *
dbus_message_get_type_string(DBusMessage *message)
{
  static const char *type_string[] =
  {
    "invalid",
    "call",
    "return",
    "error",
    "signal"
  };
  int type = dbus_message_get_type(message);

  if (type >= G_N_ELEMENTS(type_string))
    return "unkown";

  return type_string[type];
}

static void
restore_focus(GKeyFile *key_file, const gchar *group_name, GtkWidget *widget)
{
  GError *error = NULL;
  gboolean focused = g_key_file_get_boolean(key_file, group_name, "Focused",
                                            &error);

  if (error)
    g_error_free(error);
  else if (focused)
    gtk_widget_grab_focus(widget);
}

static void
restore_contact_view(GKeyFile *key_file, osso_abook_data *data)
{
  GtkTreeView *tree_view;
  gint cursor_row;
  gint *selection;
  GError *error = NULL;
  gsize selection_length;

  restore_focus(key_file, "ContactView", data->contact_view);
  tree_view = osso_abook_tree_view_get_tree_view(
      OSSO_ABOOK_TREE_VIEW(data->contact_view));
  cursor_row = g_key_file_get_integer(key_file, "ContactView", "CursorRow",
                                      &error);

  if (error)
    g_clear_error(&error);
  else
  {
    GtkTreePath *path = gtk_tree_path_new_from_indices(cursor_row, -1);

    gtk_tree_view_set_cursor(tree_view, path, NULL, FALSE);
    gtk_tree_path_free(path);
  }

  selection = g_key_file_get_integer_list(key_file, "ContactView", "Selection",
                                          &selection_length, &error);

  if (error)
    g_clear_error(&error);
  else
  {
    GtkTreeSelection *tree_selection = gtk_tree_view_get_selection(tree_view);

    for (int i = 0; i < selection_length; i++)
    {
      GtkTreePath *path = gtk_tree_path_new_from_indices(selection[i], -1);

      gtk_tree_selection_select_path(tree_selection, path);
      gtk_tree_path_free(path);
    }

    g_free(selection);
  }

  g_key_file_free(key_file);
}

struct restore_contact_view_data
{
  GKeyFile *key_file;
  osso_abook_data *app_data;
  gulong id;
};

static void
notify_loading_cb(OssoABookContactModel *contact_model, GParamSpec *arg1,
                  struct restore_contact_view_data *data)
{
  if (!osso_abook_list_store_is_loading(&contact_model->parent))
  {
    restore_contact_view(data->key_file, data->app_data);
    g_signal_handler_disconnect(contact_model, data->id);
  }
}

static void
state_restore(osso_abook_data *data)
{
  GKeyFile *key_file;
  osso_state_t state = {};

  if (osso_state_read(data->osso, &state))
  {
    g_critical("Failed to read state");
    return;
  }

  key_file = g_key_file_new();

  if (g_key_file_load_from_data(key_file, state.state_data, state.state_size,
                                G_KEY_FILE_NONE, NULL))
  {
    g_free(state.state_data);

    if (gtk_widget_get_mapped(data->live_search))
    {
      restore_focus(key_file, "SearchEntry", data->live_search);
      hildon_live_search_restore_state(HILDON_LIVE_SEARCH(data->live_search),
                                       key_file);
    }

    if (osso_abook_list_store_is_loading(
          OSSO_ABOOK_LIST_STORE(data->contact_model)))
    {
      struct restore_contact_view_data *cb_data =
        g_new(struct restore_contact_view_data, 1);

      cb_data->key_file = key_file;
      cb_data->app_data = data;
      cb_data->id =
        g_signal_connect_data(data->contact_model, "notify::loading",
                              G_CALLBACK(notify_loading_cb), cb_data,
                              (GClosureNotify)&g_free, 0);
    }
    else
      restore_contact_view(key_file, data);
  }
  else
  {
    g_critical("Failed to parse state");
    g_free(state.state_data);
    g_key_file_free(key_file);
  }
}

static DBusMessage *
top_application(DBusMessage *message, osso_abook_data *data)
{
  const gchar *s = NULL;
  DBusMessageIter iter;

  if (dbus_message_iter_init(message, &iter))
  {
    if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING)
      dbus_message_iter_get_basic(&iter, &s);
  }

  if (s && !g_ascii_strcasecmp(s, "restored"))
    state_restore(data);

  app_show(data);

  return dbus_message_new_method_return(message);
}

static DBusMessage *
add_account(DBusMessage *message)
{
  DBusError error;
  const gchar *s;
  const char *vcard_field;

  dbus_error_init(&error);

  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_STRING, &s,
                             DBUS_TYPE_INVALID))
  {
    return dbus_message_new_error(message, error.name, error.message);
  }

  vcard_field = _get_vcard_field_from_uri(s);

  if (vcard_field)
  {
    EVCardAttribute *attr;
    GStrv tmpv;
    GtkWidget *dialog;
    gchar *tmp = g_strdup(g_strstr_len(s, -1, ":") + 1);

    s = index(tmp, '?');

    if (s)
      tmp[s - tmp] = 0;

    tmpv = g_strsplit_set(tmp, ",;", -1);
    g_free(tmp);

    tmp = g_strjoinv(" ", tmpv);
    g_strfreev(tmpv);

    tmpv = g_strsplit(tmp, "%20", -1);
    g_free(tmp);

    tmp = g_strjoinv(" ", tmpv);
    g_strfreev(tmpv);

    attr = e_vcard_attribute_new(NULL, vcard_field);
    e_vcard_attribute_add_value(attr, tmp);
    g_free(tmp);

    dialog = osso_abook_temporary_contact_dialog_new(NULL, NULL, attr, NULL);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    gtk_widget_show(dialog);
    e_vcard_attribute_free(attr);
  }

  return dbus_message_new_method_return(message);

  ;
}

static GList *
get_applets_contacts()
{
  GList *contacts = NULL;
  OssoABookAggregator *aggregator = OSSO_ABOOK_AGGREGATOR(
      osso_abook_aggregator_get_default(NULL));
  GSList *applets = osso_abook_settings_get_home_applets();
  GSList *l;

  for (l = applets; l; l = l->next)
  {
    if (g_str_has_prefix(l->data, OSSO_ABOOK_HOME_APPLET_PREFIX))
    {
      GList *master = osso_abook_aggregator_lookup(
          aggregator, ((char *)(l->data)) +
          strlen(OSSO_ABOOK_HOME_APPLET_PREFIX));

      for (; master; master = g_list_delete_link(master, master))
      {
        const char *uid = e_contact_get_const(E_CONTACT(master->data),
                                              E_CONTACT_UID);

        contacts = g_list_prepend(contacts, (gpointer)uid);
      }
    }
  }

  g_slist_free_full(applets, g_free);

  return contacts;
}

static void
applet_contact_chooser_response_cb(OssoABookContactChooser *chooser,
                                   gint response_id, osso_abook_data *data)
{
  if (response_id == GTK_RESPONSE_OK)
  {
    GList *contacts = osso_abook_contact_chooser_get_selection(chooser);

    if (contacts)
      osso_abook_contact_shortcut_create(contacts->data);

    g_list_free(contacts);
  }

  gtk_widget_destroy(GTK_WIDGET(chooser));
}

static DBusMessage *
add_shortcut(DBusMessage *message, osso_abook_data *data)
{
  GtkWidget *chooser;
  GList *excluded_contacts;

  excluded_contacts = get_applets_contacts();
  chooser = osso_abook_contact_chooser_new(
      NULL, g_dgettext("maemo-af-desktop", "home_ti_select_contact"));
  osso_abook_contact_chooser_set_excluded_contacts(
    OSSO_ABOOK_CONTACT_CHOOSER(chooser), excluded_contacts);

  g_signal_connect(chooser, "response",
                   G_CALLBACK(applet_contact_chooser_response_cb), data);

  gtk_widget_show(chooser);
  g_list_free(excluded_contacts);

  return dbus_message_new_method_return(message);
}

static DBusMessage *
search_append(DBusMessage *message, osso_abook_data *data)
{
  DBusError error;
  const gchar *s;

  dbus_error_init(&error);

  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_STRING, &s,
                             DBUS_TYPE_INVALID))
  {
    return dbus_message_new_error(message, error.name, error.message);
  }

  switch_to_abc_view(data);
  app_show(data);
  hildon_live_search_append_text(HILDON_LIVE_SEARCH(data->live_search), s);

  return dbus_message_new_method_return(message);
}

static void
aggregator_ready_cb(OssoABookWaitable *waitable, const GError *error,
                    gpointer data)
{
  DBusMessage *reply;
  GList *contact;
  dbus_bool_t result = FALSE;

  if (!error)
  {
    GList *master_contacts = osso_abook_aggregator_list_master_contacts(
        OSSO_ABOOK_AGGREGATOR(waitable));
    GList *applet_contacts = get_applets_contacts();

    if (master_contacts)
    {
      for (contact = master_contacts; contact; contact = contact->next)
      {
        const char *uid = e_contact_get_const(E_CONTACT(contact->data),
                                              E_CONTACT_UID);

        if (!g_list_find_custom(applet_contacts, uid, (GCompareFunc)&g_strcmp0))
        {
          result = TRUE;
          break;
        }
      }
    }

    g_list_free(applet_contacts);
    g_list_free(master_contacts);
  }

  reply = dbus_message_new_method_return(data);
  dbus_message_append_args(reply,
                           DBUS_TYPE_BOOLEAN, &result,
                           DBUS_TYPE_INVALID);
  dbus_connection_send(
    osso_get_dbus_connection(osso_abook_get_osso_context()), reply, NULL);
  dbus_message_unref(reply);
}


static DBusMessage *
open_group(DBusMessage *message, osso_abook_data *data)
{
  char *requested_protocol = NULL;
  gchar *requested_group = NULL;
  dbus_bool_t select_account = FALSE;
  OssoABookGroup *group = NULL;
  DBusError error;

  dbus_error_init(&error);

  if (!dbus_message_get_args(message, &error,
                             DBUS_TYPE_STRING, &requested_group,
                             DBUS_TYPE_STRING, &requested_protocol,
                             DBUS_TYPE_BOOLEAN, &select_account,
                             DBUS_TYPE_INVALID))
  {
    return dbus_message_new_error(message, error.name, error.message);
  }

  if (!IS_EMPTY(requested_group))
    group = osso_abook_service_group_lookup_by_name(requested_group);

  if (!group && !IS_EMPTY(requested_protocol))
  {
    GSList *l;

    for (l = data->service_groups; l; l = l->next)
    {
      if (l->data && OSSO_ABOOK_IS_SERVICE_GROUP(l->data))
      {
        TpAccount *account = osso_abook_service_group_get_account(
            OSSO_ABOOK_SERVICE_GROUP(l->data));

        if (account)
        {
          if (!strcmp(tp_account_get_protocol_name(account),
                      requested_protocol))
          {
            group = l->data;
            break;
          }
        }
      }
    }
  }

  if (group)
  {
    app_show(data);
    switch_to_group_subview(data, group);
  }
  else if (select_account)
  {
#if 0
    GtkWidget *parent;
    GList *accounts = NULL;
    AuicClient *auic;

    app_show(data);
    parent = hildon_window_stack_peek(hildon_window_stack_get_default());

    if (!IS_EMPTY(requested_protocol))
    {
      accounts = osso_abook_account_manager_list_by_protocol(
          NULL, requested_protocol);
    }

    auic = auic_client_new(GTK_WINDOW(parent));

    if (!IS_EMPTY(requested_protocol) && !accounts)
      auic_client_open_new_account(auic, requested_protocol);
    else
      auic_client_open_accounts_list(auic);

    g_list_free(accounts);
    g_object_unref(auic);
#else
  /* FIXME finish it */
  g_assert(0);
#endif
  }

  return dbus_message_new_method_return(message);
}

static DBusHandlerResult
message_filter(DBusConnection *connection, DBusMessage *message,
               gpointer user_data)
{
  osso_abook_data *data = user_data;
  const char *iface;
  const char *member;
  GQuark member_quark = 0;
  GQuark iface_quark = 0;
  int type;
  DBusHandlerResult rv;
  DBusMessage *reply = NULL;
  const gchar *s;

  iface = dbus_message_get_interface(message);

  if (iface)
    iface_quark = g_quark_try_string(iface);
  else
    iface_quark = 0;

  member = dbus_message_get_member(message);

  if (member)
    member_quark = g_quark_try_string(member);
  else
    member_quark = 0;

  type = dbus_message_get_type(message);

  OSSO_ABOOK_NOTE(DBUS, "%s %s => %s: %s.%s(%s) (path=%s)\n",
                  dbus_message_get_type_string(message),
                  dbus_message_get_sender(message),
                  dbus_message_get_destination(message),
                  dbus_message_get_interface(message),
                  dbus_message_get_member(message),
                  dbus_message_get_signature(message),
                  dbus_message_get_path(message));

  if (iface_quark != com_nokia_osso_addressbook_quark)
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  if (type != DBUS_MESSAGE_TYPE_METHOD_CALL)
  {
    reply = dbus_message_new_error_printf(
        message, DBUS_ERROR_NOT_SUPPORTED, "Unexpected message type: %s",
        dbus_message_get_type_string(message));
  }
  else if (member_quark == top_application_quark)
    reply = top_application(message, data);
  else if (member_quark == add_account_quark)
    reply = add_account(message);
  else if (member_quark == add_shortcut_quark)
    reply = add_shortcut(message, data);
  else if (member_quark == can_add_shortcut_quark)
  {
    osso_abook_waitable_call_when_ready(
      OSSO_ABOOK_WAITABLE(osso_abook_aggregator_get_default(NULL)),
      aggregator_ready_cb, dbus_message_ref(message),
      (GDestroyNotify)&dbus_message_unref);
    return DBUS_HANDLER_RESULT_HANDLED;
  }
  else if (member_quark == search_append_quark)
    reply = search_append(message, data);
  else if (member_quark == open_group_quark)
    reply = open_group(message, data);
  else
  {
    reply = dbus_message_new_error(message, DBUS_ERROR_UNKNOWN_METHOD,
                                   dbus_message_get_member(message));
  }

  if (reply)
  {
    if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR)
    {
      dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING, &s,
                            DBUS_TYPE_INVALID);
      OSSO_ABOOK_WARN("%s (%s)", dbus_message_get_error_name(reply), s);
    }

    dbus_connection_send(connection, reply, NULL);
    dbus_message_unref(reply);
    rv = DBUS_HANDLER_RESULT_HANDLED;
  }
  else
  {
    g_assert(dbus_message_get_no_reply(message));
    rv = DBUS_HANDLER_RESULT_HANDLED;
  }

  return rv;
}

dbus_bool_t
desktop_service_init(osso_abook_data *data)
{
  com_nokia_osso_addressbook_quark =
    g_quark_from_static_string("com.nokia.osso_addressbook");
  top_application_quark = g_quark_from_static_string("top_application");
  add_account_quark = g_quark_from_static_string("add_account");
  add_shortcut_quark = g_quark_from_static_string("add_shortcut");
  can_add_shortcut_quark = g_quark_from_static_string("can_add_shortcut");
  search_append_quark = g_quark_from_static_string("search_append");
  select_contacts_quark = g_quark_from_static_string("select_contacts");
  open_group_quark = g_quark_from_static_string("open_group");
  response_quark = g_quark_from_static_string("Response");
  release_quark = g_quark_from_static_string("Release");
  osso_abook_object_owner_quark =
    g_quark_from_static_string("osso-abook-object-owner");
  osso_abook_object_path_quark =
    g_quark_from_static_string("osso-abook-object-path");

  return dbus_connection_add_filter(osso_get_dbus_connection(data->osso),
                                    message_filter, data, NULL);
}
