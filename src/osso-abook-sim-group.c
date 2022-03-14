/*
 * osso-abook-sim-group.c
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

#include <libosso-abook/osso-abook-aggregator.h>
#include <libosso-abook/osso-abook-waitable.h>
#include <libosso-abook/osso-abook-roster.h>
#include <libosso-abook/osso-abook-contact-model.h>
#include <libosso-abook/osso-abook-log.h>
#include <libosso-abook/osso-abook-debug.h>

#include <libintl.h>
#include <stdlib.h>

#include "osso-abook-sim-group.h"

struct sim_group_capabilities
{
  gchar *imsi;
  int max_num_of_entries;
  int max_num_of_sne_entries;
  int max_num_of_email_entries;
};

struct _OssoABookSimGroupPrivate
{
  OssoABookContact *vmbx_contact;
  OssoABookContact *emnumber_contact;
  OssoABookContactModel *contact_model;
  gboolean mbdn_ready;
  gboolean vmbx_ready;
  GSList *adn_contacts;
  GSList *sdn_contacts;
  GHashTable *contacts_by_full_name;
  GHashTable *aggregators;
  GQueue *closures;
  int aggregator_num;
  struct sim_group_capabilities *capabilities;
};

typedef struct _OssoABookSimGroupPrivate OssoABookSimGroupPrivate;

#define OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group) \
                ((OssoABookSimGroupPrivate *)osso_abook_sim_group_get_instance_private(sim_group))

static void
osso_abook_sim_group_waitable_iface_init(OssoABookWaitableIface *iface);

G_DEFINE_TYPE_WITH_CODE(
  OssoABookSimGroup,
  osso_abook_sim_group,
  OSSO_ABOOK_TYPE_GROUP,
  G_IMPLEMENT_INTERFACE(
      OSSO_ABOOK_TYPE_WAITABLE,
      osso_abook_sim_group_waitable_iface_init);
  G_ADD_PRIVATE(OssoABookSimGroup);
)

enum {
  AVAILABLE,
  VOICEMAIL_CONTACT_AVAILABLE,
  CAPABILITIES_AVAILABLE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {};


static OssoABookWaitableClosure *
osso_abook_sim_group_waitable_pop(OssoABookWaitable *waitable,
                                  OssoABookWaitableClosure *closure)
{
  OssoABookSimGroup *sim_group = OSSO_ABOOK_SIM_GROUP(waitable);
  OssoABookSimGroupPrivate *priv = OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group);
  GList *l;

  if (closure)
    l = g_queue_find(priv->closures, closure);
  else
    l = g_queue_pop_head_link(priv->closures);

  if (l)
  {
    closure = l->data;
    g_queue_unlink(priv->closures, l);
    g_list_free_1(l);
  }

  return closure;
}

static gboolean
osso_abook_sim_group_waitable_is_ready(OssoABookWaitable *waitable,
                                       const GError **error)
{
  OssoABookSimGroup *sim_group = OSSO_ABOOK_SIM_GROUP(waitable);
  OssoABookSimGroupPrivate *priv = OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group);

  return priv->aggregator_num == 0;
}

static void
osso_abook_sim_group_waitable_push(OssoABookWaitable *waitable,
                                   OssoABookWaitableClosure *closure)
{
  OssoABookSimGroup *sim_group = OSSO_ABOOK_SIM_GROUP(waitable);
  OssoABookSimGroupPrivate *priv = OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group);

  g_queue_push_head(priv->closures, closure);
}

static void
osso_abook_sim_group_waitable_iface_init(OssoABookWaitableIface *iface)
{
  iface->pop = osso_abook_sim_group_waitable_pop;
  iface->is_ready = osso_abook_sim_group_waitable_is_ready;
  iface->push = osso_abook_sim_group_waitable_push;
}

static void
osso_abook_sim_group_dispose(GObject *object)
{
  OssoABookSimGroup *sim_group = OSSO_ABOOK_SIM_GROUP(object);
  OssoABookSimGroupPrivate *priv = OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group);

  if (priv->vmbx_contact)
  {
    g_object_unref(priv->vmbx_contact);
    priv->vmbx_contact = NULL;
  }

  if (priv->emnumber_contact)
  {
    g_object_unref(priv->emnumber_contact);
    priv->emnumber_contact = NULL;
  }

  if (priv->aggregators)
    g_hash_table_remove_all(priv->aggregators);

  if (priv->contacts_by_full_name)
    g_hash_table_remove_all(priv->contacts_by_full_name);

  if (priv->contact_model)
  {
    g_object_unref(priv->contact_model);
    priv->contact_model = NULL;
  }

  g_slist_free_full(priv->adn_contacts, g_object_unref);
  g_slist_free_full(priv->sdn_contacts, g_object_unref);

  G_OBJECT_CLASS(osso_abook_sim_group_parent_class)->dispose(object);
}

static void
osso_abook_sim_group_finalize(GObject *object)
{
  OssoABookSimGroup *sim_group = OSSO_ABOOK_SIM_GROUP(object);
  OssoABookSimGroupPrivate *priv = OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group);

  if (priv->aggregators)
    g_hash_table_destroy(priv->aggregators);

  if (priv->contacts_by_full_name)
    g_hash_table_destroy(priv->contacts_by_full_name);

  g_queue_free(priv->closures);

  if (priv->capabilities)
  {
    g_free(priv->capabilities->imsi);
    g_slice_free(struct sim_group_capabilities, priv->capabilities);
  }

  G_OBJECT_CLASS(osso_abook_sim_group_parent_class)->finalize(object);
}

static gboolean
osso_abook_sim_group_includes_contact(OssoABookGroup *group,
                                      OssoABookContact *contact)
{
  return TRUE;
}

static const char *
osso_abook_sim_group_get_icon_name(OssoABookGroup *group)
{
  return "addressbook_sim_contacts_group";
}

static const char *
osso_abook_sim_group_get_name(OssoABookGroup *group)
{
  return "addr_va_groups_simcard";
}

static int
osso_abook_sim_group_get_sort_weight(OssoABookGroup *group)
{
  return -100;
}

static OssoABookListStore *
osso_abook_sim_group_get_model(OssoABookGroup *group)
{
  OssoABookSimGroupPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_SIM_GROUP(group), NULL);

  priv = OSSO_ABOOK_SIM_GROUP_PRIVATE(OSSO_ABOOK_SIM_GROUP(group));

  return OSSO_ABOOK_LIST_STORE(priv->contact_model);
}

static void
osso_abook_sim_group_class_init(OssoABookSimGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  OssoABookGroupClass *group_class = OSSO_ABOOK_GROUP_CLASS(klass);

  object_class->dispose = osso_abook_sim_group_dispose;
  object_class->finalize = osso_abook_sim_group_finalize;

  group_class->includes_contact = osso_abook_sim_group_includes_contact;
  group_class->get_icon_name = osso_abook_sim_group_get_icon_name;
  group_class->get_name = osso_abook_sim_group_get_name;
  group_class->get_sort_weight = osso_abook_sim_group_get_sort_weight;
  group_class->get_model = osso_abook_sim_group_get_model;

  signals[AVAILABLE] =
      g_signal_new("available", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(OssoABookSimGroupClass, available),
                   0, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  signals[VOICEMAIL_CONTACT_AVAILABLE] =
      g_signal_new("voicemail-contact-available", G_TYPE_FROM_CLASS(klass),
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(OssoABookSimGroupClass,
                                   voicemail_contact_available),
                   0, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  signals[CAPABILITIES_AVAILABLE] =
      g_signal_new(
        "capabilities-available", G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET(OssoABookSimGroupClass,
                                           capabilities_available),
        0, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static gchar *
_get_static_capability_string(const char *caps, const char *cap_name)
{
  gchar **caps_array = g_strsplit(caps, ",", 0);
  gchar **cap = caps_array;
  gchar *rv = NULL;

  while (*cap)
  {
    int len = strlen(cap_name);

    if (!g_ascii_strncasecmp(cap_name, *cap, len))
    {
      rv = g_strdup(*cap + len + 1);
      break;
    }
    else
      cap++;
  }

  g_strfreev(caps_array);

  return rv;
}

static int
_get_static_capability_int(const char *caps, const char *cap_name)
{
  int rv = 0;
  gchar *cap = _get_static_capability_string(caps, cap_name);

  if (cap)
  {
    rv = strtol(cap, NULL, 10);
    g_free(cap);
  }

  return rv;
}

static void
add_contacts_to_store(OssoABookListStore *store, GSList **contacts)
{
  GSList *c;
  GList *rows = NULL;

  g_return_if_fail(contacts);

  for (c = *contacts; c; c = g_slist_delete_link(c, c))
  {
    rows = g_list_prepend(rows, osso_abook_list_store_row_new(c->data));
    g_object_unref(c->data);
  }

  osso_abook_list_store_merge_rows(store, rows);
  g_list_free(rows);

  *contacts = NULL;
}

static OssoABookContact *
create_sim_contact(OssoABookContactModel *contact_model,
                   const gchar *given_name, const char *uid)
{
  OssoABookContact *contact = osso_abook_contact_new();
  EContactName *en = e_contact_name_new();
  GList *rows;

  en->given = g_strdup(given_name);
  e_contact_set(E_CONTACT(contact), E_CONTACT_UID, uid);
  e_contact_set(E_CONTACT(contact), E_CONTACT_NAME, en);
  e_contact_name_free(en);
  rows = g_list_prepend(NULL, osso_abook_list_store_row_new(contact));
  osso_abook_list_store_merge_rows(OSSO_ABOOK_LIST_STORE(contact_model), rows);
  g_list_free(rows);

  return contact;
}

static void
_copy_sim_contact_phone_numbers(OssoABookContact *contact,
                                OssoABookContact **contacts)
{
  OssoABookContact **c;

  for (c = contacts; *c; ++c)
  {
    gchar *val = osso_abook_contact_get_value(E_CONTACT(*c), "TEL");

    if (val && *val)
    {
      e_vcard_add_attribute_with_value(E_VCARD(contact),
                                       e_vcard_attribute_new(0, "TEL"), val);
    }

    g_free(val);
  }
}

static struct sim_group_capabilities *
osso_abook_sim_capabilities_new(EBook *book)
{
  struct sim_group_capabilities *capabilities = NULL;
  const char *caps;

  g_return_val_if_fail(E_IS_BOOK(book), NULL);

  caps = e_book_get_static_capabilities(book, NULL);

  g_return_val_if_fail(caps, NULL);

  capabilities = g_slice_new0(struct sim_group_capabilities);
  capabilities->imsi = _get_static_capability_string(caps, "imsi");
  capabilities->max_num_of_entries =
      _get_static_capability_int(caps, "max_num_of_entries");
  capabilities->max_num_of_sne_entries =
      _get_static_capability_int(caps, "max_num_of_sne_entries") + 1;
  capabilities->max_num_of_email_entries =
      _get_static_capability_int(caps, "max_num_of_email_entries");

  return capabilities;
}

static void
sequence_complete_cb(OssoABookRoster *roster, guint status, gpointer user_data)
{
  OssoABookSimGroup *sim_group = OSSO_ABOOK_SIM_GROUP(user_data);
  OssoABookSimGroupPrivate *priv = OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group);
  const char *uid = osso_abook_roster_get_book_uri(roster);

  g_return_if_fail(uid != NULL);

  if (!strcmp(uid, "mbdn"))
    priv->mbdn_ready = TRUE;

  if (!strcmp(uid, "vmbx"))
    priv->vmbx_ready = TRUE;

  if (priv->vmbx_ready && priv->mbdn_ready)
  {
    priv->vmbx_ready = FALSE;
    priv->mbdn_ready = FALSE;

    g_signal_emit(G_OBJECT(sim_group), signals[VOICEMAIL_CONTACT_AVAILABLE], 0);
  }

  if (!strcmp(uid, "adn"))
  {
    add_contacts_to_store(OSSO_ABOOK_LIST_STORE(priv->contact_model),
                          &priv->adn_contacts);
    g_hash_table_destroy(priv->contacts_by_full_name);
    priv->contacts_by_full_name = NULL;

    priv->capabilities = osso_abook_sim_capabilities_new(
          osso_abook_roster_get_book(roster));

    g_signal_emit(G_OBJECT(sim_group), signals[CAPABILITIES_AVAILABLE], 0);
  }
  else if (!strcmp(uid, "sdn"))
  {
    add_contacts_to_store(OSSO_ABOOK_LIST_STORE(priv->contact_model),
                          &priv->sdn_contacts);
  }
}


/*
  copied from libossoabook...

  don't really like it, but I was not able to find a way to create
  in-memory ESource
*/
static ESource *
create_roster_source(const gchar *uid)
{
  gchar *_uid = g_strdup(uid);
  GError *error = NULL;
  ESourceRegistry *registry;
  ESource *source = NULL;

  registry = e_source_registry_new_sync(NULL, &error);

  if (error)
  {
    OSSO_ABOOK_WARN("Creating ESourceRegistry for uid %s failed - %s", uid,
                    error->message);
    g_clear_error(&error);
    goto err_out;
  }

  e_filename_make_safe(_uid);
  source = e_source_registry_ref_source(registry, _uid);

  if (source)
  {
    g_warn_if_fail(e_source_has_extension(source,
                                          E_SOURCE_EXTENSION_ADDRESS_BOOK));
    g_warn_if_fail(e_source_has_extension(source, E_SOURCE_EXTENSION_RESOURCE));
  }
  else
  {
    OSSO_ABOOK_NOTE(TP, "creating new EDS source %s for %s", _uid, uid);
    source = e_source_new_with_uid(_uid, NULL, &error);

    if (source)
    {
      ESourceBackend *backend =
          e_source_get_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK);
      ESourceResource *resource =
          e_source_get_extension (source, E_SOURCE_EXTENSION_RESOURCE);
      GList *sources = NULL;

      e_source_resource_set_identity(resource, uid);
      e_source_backend_set_backend_name (backend, "sim");
      e_source_set_display_name(source, uid);

      sources = g_list_append(sources, source);
      e_source_registry_create_sources_sync(registry, sources, NULL, &error);

      g_list_free(sources);
    }

    if (error)
    {
      OSSO_ABOOK_WARN("Creating ESource for uid %s failed - %s", uid,
                      error->message);
      g_clear_error(&error);

      if (source)
      {
        g_object_unref(source);
        source = NULL;
      }
    }
  }

  g_object_unref(registry);

err_out:

  g_free(_uid);

  return source;
}

static void
osso_abook_sim_group_waitable_notify(OssoABookSimGroup *sim_group)
{
  OssoABookSimGroupPrivate *priv = OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group);

  priv->aggregator_num--;

  if (priv->aggregator_num == 0)
  {
    osso_abook_waitable_notify(OSSO_ABOOK_WAITABLE(sim_group), NULL);
  }
}

static void
book_view_changed_cb(GObject *gobject, GParamSpec *pspec, gpointer user_data)
{
  g_signal_emit(G_OBJECT(user_data), signals[AVAILABLE], 0);
}

static gboolean
ebook_status_cb(OssoABookAggregator *aggregator, EBookStatus status,
                gpointer user_data)
{
  OssoABookSimGroup *sim_group = OSSO_ABOOK_SIM_GROUP(user_data);
  OssoABookSimGroupPrivate *priv = OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group);
  const char *uid;

  uid = osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator));

  g_return_val_if_fail(uid != NULL, FALSE);

  if (status)
  {
    if (!strcmp(uid, "mbdn"))
      priv->mbdn_ready = TRUE;

    if (!strcmp(uid, "vmbx"))
      priv->vmbx_ready = TRUE;

    if (status != E_BOOK_ERROR_NO_SUCH_BOOK)
    {
      OSSO_ABOOK_WARN(
            "There was a problem while opening the %s book: status %d", uid,
            status);
    }

    g_hash_table_remove(priv->aggregators, uid);
    osso_abook_sim_group_waitable_notify(sim_group);
  }

  return TRUE;
}

static void
aggregator_ready_cb(OssoABookWaitable *waitable, const GError *error,
                    gpointer data)
{
  osso_abook_sim_group_waitable_notify(data);
}

#define E_VCARD_FLD_ID(attr) \
  e_contact_field_id_from_vcard(e_vcard_attribute_get_name(attr))

static gboolean
_copy_sim_contact_details(EContact *dest_contact, EContact *source_contact)
{
  GList *src_attr;
  gboolean copied = FALSE;

  g_return_val_if_fail(E_IS_CONTACT(source_contact), FALSE);
  g_return_val_if_fail(E_IS_CONTACT(dest_contact), FALSE);

  for (src_attr = e_vcard_get_attributes(E_VCARD(source_contact)); src_attr;
       src_attr = src_attr->next)
  {
    const char *attr_name = e_vcard_attribute_get_name(src_attr->data);

    if (!g_strcmp0(attr_name, "TEL") || !g_strcmp0(attr_name, "EMAIL"))
    {
      GList *val = e_vcard_attribute_get_values(src_attr->data);

      if (val && val->data && *(const char *)val->data)
      {
        GList *attr = e_contact_get_attributes(dest_contact,
                                               E_VCARD_FLD_ID(src_attr->data));
        GList *a;

        for (a = attr; a; a = a->next)
        {
          GList *dest_val = e_vcard_attribute_get_values(a->data);

          if (!g_strcmp0(dest_val->data, val->data))
            break;
        }

        if (!a)
        {
          e_vcard_add_attribute(E_VCARD(dest_contact),
                                e_vcard_attribute_copy(src_attr->data));
          copied = TRUE;
        }

        g_list_free_full(attr, (GDestroyNotify)e_vcard_attribute_free);
      }
    }
    else if (!g_strcmp0(attr_name, "NICKNAME"))
    {
      GList *val = e_vcard_attribute_get_values(src_attr->data);

      if (val)
      {
        const char *id = e_contact_get_const(dest_contact,
                                             E_VCARD_FLD_ID(src_attr->data));

        if (!id)
        {
          e_vcard_add_attribute(E_VCARD(dest_contact),
                                e_vcard_attribute_copy(src_attr->data));
          copied = TRUE;
        }
      }
    }
  }

  return copied;
}

static void
contacts_added_cb(OssoABookRoster *roster, OssoABookContact **contacts,
                  gpointer user_data)
{
  OssoABookSimGroup *sim_group = OSSO_ABOOK_SIM_GROUP(user_data);
  OssoABookSimGroupPrivate *priv = OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group);
  const char *uid;

  g_return_if_fail(uid != NULL);

  if (!strcmp(uid, "adn"))
  {
    OssoABookContact *c;

    while((c = *contacts))
    {
      const char *full_name = e_contact_get_const(&c->parent,
                                                  E_CONTACT_FULL_NAME);
      if (full_name)
      {
        OssoABookContact *existing_contact =
            g_hash_table_lookup(priv->contacts_by_full_name, full_name);

        if (existing_contact)
          _copy_sim_contact_details(E_CONTACT(existing_contact), E_CONTACT(c));
        else
        {
          priv->adn_contacts =
              g_slist_prepend(priv->adn_contacts, g_object_ref(c));

          g_hash_table_insert(priv->contacts_by_full_name, g_strdup(full_name),
                              g_object_ref(c));
        }
      }
      else
      {
        priv->adn_contacts =
            g_slist_prepend(priv->adn_contacts, g_object_ref(c));
      }
    }

    contacts++;
  }
  else if (!strcmp(uid, "sdn"))
  {
    OssoABookContact *c;

    while ((c = *contacts))
    {
      priv->sdn_contacts = g_slist_prepend(priv->sdn_contacts, g_object_ref(c));

      contacts++;
    }
  }
  else if (!strcmp(uid, "mbdn") || !strcmp(uid, "vmbx"))
  {
    if (!priv->vmbx_contact)
    {
      priv->vmbx_contact =
          create_sim_contact(priv->contact_model,
                             dgettext(NULL, "addr_fi_voicemailbox"),
                             "osso-abook-vmbx");
    }

    _copy_sim_contact_phone_numbers(priv->vmbx_contact, contacts);
  }
  else if (!strcmp(uid, "en"))
  {
    if (!priv->emnumber_contact)
    {
      priv->emnumber_contact =
          create_sim_contact(priv->contact_model,
                             dgettext(NULL, "addr_va_emergency_number"),
                             "syntesized-emnumber-contact");;
    }

    _copy_sim_contact_phone_numbers(priv->emnumber_contact, contacts);
  }
}

static void
osso_abook_sim_group_init(OssoABookSimGroup *sim_group)
{
  OssoABookSimGroupPrivate *priv = OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group);
  static const char *uids[] =
  {
    "adn",
    "sdn",
    "mbdn",
    "vmbx",
    "en",
    NULL
  };
  const char **uid = uids;

  priv->contact_model = osso_abook_contact_model_new();
  priv->closures = g_queue_new();
  priv->aggregators =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
  priv->contacts_by_full_name =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);

  while (*uid)
  {
      GError *error = NULL;
      ESource *source = create_roster_source(*uid);

      if (source)
      {

        EBook *book = e_book_new(source, &error);

        g_object_unref(source);

        if (book && e_book_open(book, TRUE, &error))
        {
          OssoABookRoster *aggregator = osso_abook_aggregator_new(book, NULL);

          osso_abook_aggregator_set_roster_manager(
                OSSO_ABOOK_AGGREGATOR(aggregator), NULL);
          g_object_unref(book);

          g_hash_table_insert(priv->aggregators, g_strdup(*uid), aggregator);

          g_signal_connect(aggregator, "contacts-added",
                           G_CALLBACK(contacts_added_cb), sim_group);
          g_signal_connect(aggregator, "sequence-complete",
                           G_CALLBACK(sequence_complete_cb), sim_group);
          g_signal_connect(aggregator, "notify::book-view",
                           G_CALLBACK(book_view_changed_cb), sim_group);
          g_signal_connect(aggregator, "ebook-status",
                           G_CALLBACK(ebook_status_cb), sim_group);

          osso_abook_roster_start(aggregator);
          priv->aggregator_num++;
          osso_abook_waitable_call_when_ready(OSSO_ABOOK_WAITABLE(aggregator),
                                              aggregator_ready_cb, sim_group,
                                              NULL);
        }
        else
        {
          OSSO_ABOOK_WARN("Cannot create SIM address book for (%s): %s", *uid,
                          error->message);
          g_clear_error(&error);
        }
      }

      uid++;
  }
}

OssoABookGroup *
osso_abook_sim_group_new()
{
  return g_object_new(OSSO_ABOOK_TYPE_SIM_GROUP, NULL);
}

OssoABookContact *
osso_abook_sim_group_get_voicemail_contact(OssoABookSimGroup *sim_group)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_SIM_GROUP(sim_group), NULL);

  return OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group)->vmbx_contact;
}

const gchar *
osso_abook_sim_group_get_imsi(OssoABookSimGroup *sim_group)
{
  OssoABookSimGroupPrivate *priv;
  struct sim_group_capabilities *capabilities;

  g_return_val_if_fail(OSSO_ABOOK_IS_SIM_GROUP(sim_group), NULL);

  priv = OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group);

  g_return_val_if_fail(priv->capabilities, NULL);

  return capabilities->imsi;
}

GList *
osso_abook_sim_group_list_adn_contacts(OssoABookSimGroup *sim_group)
{
  OssoABookSimGroupPrivate *priv;
  OssoABookAggregator *aggregator;

  g_return_val_if_fail(OSSO_ABOOK_IS_SIM_GROUP(sim_group), NULL);

  priv = OSSO_ABOOK_SIM_GROUP_PRIVATE(sim_group);

  aggregator = g_hash_table_lookup(priv->aggregators, "adn");
  g_return_val_if_fail(aggregator, NULL);

  return osso_abook_aggregator_list_master_contacts(aggregator);
}
