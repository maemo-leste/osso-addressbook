/*
 * importer.c
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

#include <hildon/hildon.h>

#include <libebook/libebook.h>
#include <libosso-abook/osso-abook-debug.h>
#include <libosso-abook/osso-abook-log.h>
#include <libosso-abook/osso-abook-util.h>

#include <libintl.h>

#include "importer.h"

#define FILES_PER_BATCH 20

typedef enum
{
  IMPORT_START,
  IMPORT_OPEN_FILE,
  IMPORT_READ_NEXT,
  IMPORT_READ_FINISH,
  IMPORT_ADD_CONTACTS,
  IMPORT_NEXT,
  IMPORT_FINISH
} OssoAddressbookImportState;

typedef struct
{
  GtkWindow *parent;
  GList *files;
  GFileInputStream *is;
  GCancellable *cancellable;
  GString *s;
  GSourceFunc cb;
  gpointer user_data;
  GtkWidget *cancel_note;
  GtkWidget *progress_bar;
  guint pulse_id;
  guint state_id;
  OssoAddressbookImportState state;
  GError *error;
  gchar *error_message;
  GList *contacts;
  EBook *book;
  EBookStatus status;
  int imported_contacts;
} import_file_data;

static gboolean
state_selector(gpointer user_data);

static void
cancel_import_response_cb(GtkWidget *dialog, gint response_id,
                          import_file_data *ifd)
{
  if (ifd->pulse_id)
  {
    g_source_remove(ifd->pulse_id);
    ifd->pulse_id = 0;
  }

  g_cancellable_cancel(ifd->cancellable);
  gtk_widget_destroy(ifd->cancel_note);
  ifd->progress_bar = NULL;
  ifd->cancel_note = NULL;
}

static gboolean
import_progress_bar_pulse_cb(gpointer user_data)
{
  import_file_data *ifd = user_data;

  gtk_progress_bar_pulse(GTK_PROGRESS_BAR(ifd->progress_bar));

  return TRUE;
}

static gboolean
import_select_state_cb(gpointer user_data)
{
  import_file_data *ifd = user_data;

  /* FIXME - ummm, what?!? */
  if (ifd->state == IMPORT_ADD_CONTACTS)
    gdk_threads_add_idle(state_selector, ifd);

  ifd->state_id = 0;

  return FALSE;
}

static gboolean
idle_file_import(gpointer user_data)
{
  import_file_data *ifd = user_data;

  ifd->progress_bar = gtk_progress_bar_new();
  ifd->cancel_note = hildon_note_new_cancel_with_progress_bar(
      ifd->parent, dgettext(NULL, "addr_pb_notification13"),
      GTK_PROGRESS_BAR(ifd->progress_bar));

  g_signal_connect(ifd->cancel_note, "response",
                   G_CALLBACK(cancel_import_response_cb), ifd);
  gtk_widget_show(ifd->cancel_note);

  ifd->pulse_id = gdk_threads_add_timeout_full(
      G_PRIORITY_HIGH_IDLE, 200, import_progress_bar_pulse_cb, ifd, 0);
  ifd->state_id =
    gdk_threads_add_timeout_seconds(3, import_select_state_cb, ifd);

  return FALSE;
}

static import_file_data *
import_file_start(GtkWindow *parent, GSourceFunc cb, gpointer user_data)
{
  import_file_data *data = g_new0(import_file_data, 1);
  GError *error = NULL;

  data->cb = cb;
  data->parent = parent;
  data->user_data = user_data;
  data->book = osso_abook_system_book_dup_singleton(1, &error);

  if (!error)
  {
    data->state = IMPORT_START;
    data->status = E_BOOK_ERROR_OK;
    data->error_message = NULL;
    data->error = NULL;
    data->cancellable = g_cancellable_new();
    gdk_threads_add_idle(idle_file_import, data);
  }
  else
  {
    OSSO_ABOOK_WARN("cannot get system book [%s]", error->message);
    g_error_free(error);
    g_free(data);
    data = NULL;
  }

  return data;
}

static void
import_file_data_free(import_file_data *ifd)
{
  if (ifd->state_id)
    g_source_remove(ifd->state_id);

  if (ifd->pulse_id)
    g_source_remove(ifd->pulse_id);

  if (ifd->cancel_note)
    gtk_widget_destroy(ifd->cancel_note);

  if (ifd->s)
    g_string_free(ifd->s, TRUE);

  if (ifd->cb)
    ifd->cb(ifd->user_data);

  if (ifd->cancellable)
    g_object_unref(ifd->cancellable);

  g_list_free_full(ifd->files, g_object_unref);

  if (ifd->book)
    g_object_unref(ifd->book);

  g_free(ifd);
}

static void
async_add_contact_cb(EBook *book, EBookStatus status, const gchar *id,
                     gpointer closure)
{
  import_file_data *ifd = closure;

  if (status)
  {
    OSSO_ABOOK_WARN("Cannot import contact %s: %d", id, status);
    ifd->status = status;
  }

  g_object_unref(ifd->contacts->data);
  ifd->contacts = g_list_delete_link(ifd->contacts, ifd->contacts);

  if (ifd->contacts)
  {
    e_book_async_add_contact(ifd->book, ifd->contacts->data,
                             async_add_contact_cb, ifd);
  }
  else
    gdk_threads_add_idle(state_selector, ifd);
}

static gboolean
is_vcard_or_directory(GFileInfo *info)
{
  const char *content_type = NULL;

  if (info)
    content_type = g_file_info_get_content_type(info);

  return g_content_type_is_mime_type(content_type, "text/x-vcard") ||
         !g_strcmp0("text/directory", content_type);
}

static void
import_next_chunk(import_file_data *ifd, gboolean erase_all)
{
  GList *cards;
  gsize len = 0;

  for (cards = osso_abook_e_vcard_util_split_cards(ifd->s->str, &len); cards;
       cards = g_list_delete_link(cards, cards))
  {
    EContact *contact = e_contact_new_from_vcard(cards->data);

    if (contact)
    {
      if (e_vcard_get_attributes(E_VCARD(contact)))
      {
        osso_abook_e_contact_persist_data(contact, NULL);
        ifd->imported_contacts++;
        ifd->contacts = g_list_prepend(ifd->contacts, contact);
      }
      else
        g_object_unref(contact);
    }

    g_free(cards->data);
  }

  g_string_erase(ifd->s, 0, erase_all ? -1 : len);
}

static gboolean
state_selector(gpointer user_data)
{
  import_file_data *ifd = user_data;
  gboolean rv = FALSE;

  g_warn_if_fail(ifd->error == NULL);

  if (g_cancellable_is_cancelled(ifd->cancellable))
    ifd->state = IMPORT_FINISH;

  switch (ifd->state)
  {
    case IMPORT_START:
    {
      GFileInfo *info = NULL;

      if (ifd->files)
      {
        info = g_file_query_info(ifd->files->data,
                                 G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                 G_FILE_QUERY_INFO_NONE, ifd->cancellable,
                                 &ifd->error);
      }

      if (is_vcard_or_directory(info))
        ifd->state = IMPORT_OPEN_FILE;
      else
      {
        if (ifd->files)
        {
          g_object_unref(ifd->files->data);
          ifd->files = g_list_delete_link(ifd->files, ifd->files);
        }

        ifd->state = IMPORT_NEXT;
        ifd->error_message = dgettext(NULL, "addr_ni_importing_fail_format");
      }

      if (info)
        g_object_unref(info);

      rv = TRUE;
      break;
    }
    case IMPORT_OPEN_FILE:
    {
      if (OSSO_ABOOK_DEBUG_FLAGS(CONTACT_ADD))
      {
        gchar *uri = g_file_get_uri(ifd->files->data);

        OSSO_ABOOK_NOTE(CONTACT_ADD, "importing file: %s", uri);
        g_free(uri);
      }

      ifd->is = g_file_read(ifd->files->data, ifd->cancellable, &ifd->error);
      g_object_unref(ifd->files->data);
      ifd->files = g_list_delete_link(ifd->files, ifd->files);

      if (ifd->is)
      {
        ifd->state = IMPORT_READ_NEXT;
        ifd->s = g_string_new(NULL);
      }
      else
        ifd->state = IMPORT_NEXT;

      rv = TRUE;
      break;
    }
    case IMPORT_READ_NEXT:
    {
      gchar buf[8192];
      gssize chunk = g_input_stream_read(G_INPUT_STREAM(ifd->is), buf,
                                         sizeof(buf), ifd->cancellable,
                                         &ifd->error);

      if (chunk <= 0)
      {
        import_next_chunk(ifd, TRUE);
        ifd->state = IMPORT_READ_FINISH;
        rv = TRUE;
      }
      else
      {
        g_string_append_len(ifd->s, buf, chunk);
        import_next_chunk(ifd, FALSE);

        if (ifd->s->len > 512000)
        {
          g_string_erase(ifd->s, 0, -1);
          ifd->error_message = dgettext(0, "addr_ni_importing_fail_size");
        }

        if (ifd->imported_contacts > 1000)
        {
          ifd->state = IMPORT_READ_FINISH;
          ifd->error_message = dgettext(0, "addr_ni_importing_fail_size");
          rv = TRUE;
        }
        else
          rv = TRUE;
      }

      break;
    }
    case IMPORT_READ_FINISH:
    {
      g_input_stream_close(G_INPUT_STREAM(ifd->is), ifd->cancellable,
                           &ifd->error);
      g_object_unref(ifd->is);
      ifd->is = NULL;

      if (ifd->error || !ifd->contacts)
      {
        ifd->state = IMPORT_NEXT;
        rv = TRUE;
      }
      else if (!ifd->files)
        goto add_contacts;

      /* FIXME - do we really want that limit? */
      if (ifd->imported_contacts >= 1000)
      {
        if (ifd->imported_contacts == 1000)
          ifd->error_message = dgettext(0, "addr_ni_importing_fail_size");

add_contacts:
        ifd->state = IMPORT_ADD_CONTACTS;
        g_string_free(ifd->s, TRUE);
        ifd->s = NULL;
        rv = !ifd->state_id;
      }
      else
      {
        ifd->state = IMPORT_START;
        rv = TRUE;
      }

      break;
    }
    case IMPORT_ADD_CONTACTS:
    {
      if (ifd->contacts)
      {
        GtkWidget *action_area = gtk_dialog_get_action_area(
            GTK_DIALOG(ifd->cancel_note));
        GList *l = gtk_container_get_children(GTK_CONTAINER(action_area));

        if (l)
          gtk_widget_set_sensitive(GTK_WIDGET(l->data), FALSE);

        g_list_free(l);
        e_book_async_add_contact(ifd->book, ifd->contacts->data,
                                 async_add_contact_cb, ifd);
      }
      else if (ifd->status)
      {
        ifd->state = IMPORT_NEXT;
        rv = TRUE;
      }
      else
      {
        ifd->state = IMPORT_FINISH;
        rv = TRUE;
      }

      break;
    }
    case IMPORT_NEXT:
    {
      if (ifd->error &&
          g_error_matches(ifd->error, E_BOOK_ERROR, E_BOOK_ERROR_NO_SPACE))
      {
        ifd->error_message = dgettext(NULL, "addr_ni_importing_fail_mem");
      }
      else if (!ifd->error_message)
        ifd->error_message = dgettext(NULL, "addr_ni_importing_fail");

      if (ifd->files)
        ifd->state = IMPORT_START;
      else
        ifd->state = IMPORT_FINISH;

      g_clear_error(&ifd->error);
      rv = TRUE;
      break;
    }
    case IMPORT_FINISH:
    {
      if (ifd->error_message)
      {
        hildon_banner_show_information(GTK_WIDGET(ifd->parent), NULL,
                                       ifd->error_message);
      }
      else
      {
        hildon_banner_show_information(
          GTK_WIDGET(ifd->parent), NULL,
          dgettext(NULL, "addr_ib_imported_successfully"));
      }

      import_file_data_free(ifd);
      break;
    }
    default:
    {
      OSSO_ABOOK_WARN("Invalid importer state: %d", ifd->state);
      break;
    }
  }

  return rv;
}

void
do_import(GtkWindow *parent, const char *uri, GSourceFunc import_finished_cb,
          gpointer user_data)
{
  import_file_data *ifd;

  g_return_if_fail(uri);

  ifd = import_file_start(parent, import_finished_cb, user_data);
  ifd->files = g_list_prepend(NULL, g_file_new_for_uri(uri));
  gdk_threads_add_idle(state_selector, ifd);
}

static void
enumerator_next_files_cb(GObject *source_object, GAsyncResult *res,
                         gpointer user_data)
{
  import_file_data *ifd = user_data;
  GFileEnumerator *enumerator = G_FILE_ENUMERATOR(source_object);
  GList *infos;

  infos = g_file_enumerator_next_files_finish(enumerator, res, &ifd->error);

  if (infos)
  {
    while (infos)
    {
      GFileInfo *info = infos->data;

      if (is_vcard_or_directory(info))
      {
        GFile *container = g_file_enumerator_get_container(enumerator);
        const char *name = g_file_info_get_name(info);

        ifd->files = g_list_prepend(ifd->files,
                                    g_file_get_child(container, name));
      }

      infos = g_list_delete_link(infos, infos);
      g_object_unref(info);
    }

    g_file_enumerator_next_files_async(enumerator, FILES_PER_BATCH, 0,
                                       ifd->cancellable,
                                       enumerator_next_files_cb, ifd);
  }
  else if (ifd->error)
  {
    if (ifd->files)
    {
      ifd->error_message = dgettext(NULL, "addr_ni_importing_fail");
      gdk_threads_add_idle(state_selector, ifd);
    }
    else
    {
      hildon_banner_show_information(GTK_WIDGET(ifd->parent), NULL,
                                     dgettext(NULL, "addr_ni_importing_fail"));
      import_file_data_free(ifd);
    }
  }
  else
    gdk_threads_add_idle(state_selector, ifd);
}

static void
enumerate_children_cb(GObject *source_object, GAsyncResult *res,
                      gpointer user_data)
{
  import_file_data *ifd = user_data;
  GFileEnumerator *enumerator;

  enumerator = g_file_enumerate_children_finish(G_FILE(source_object),
                                                res, &ifd->error);

  if (enumerator)
  {
    g_file_enumerator_next_files_async(enumerator, FILES_PER_BATCH, 0,
                                       ifd->cancellable,
                                       enumerator_next_files_cb, ifd);
    g_object_unref(enumerator);
  }
  else
  {
    hildon_banner_show_information(GTK_WIDGET(ifd->parent), NULL,
                                   dgettext(NULL, "addr_ni_importing_fail"));
    import_file_data_free(ifd);
  }
}

void
do_import_dir(GtkWindow *parent, const gchar *uri,
              GSourceFunc import_finished_cb, gpointer user_data)
{
  import_file_data *ifd;
  GFile *file;

  g_return_if_fail(uri);

  ifd = import_file_start(parent, import_finished_cb, user_data);
  file = g_file_new_for_uri(uri);

  g_file_enumerate_children_async(file, G_FILE_ATTRIBUTE_STANDARD_NAME
                                  "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                  G_FILE_QUERY_INFO_NONE, 0, ifd->cancellable,
                                  enumerate_children_cb, ifd);
  g_object_unref(file);
}
