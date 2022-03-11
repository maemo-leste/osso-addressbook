/*
 * osso-abook-get-your-contacts-dialog.c
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

#include <hildon/hildon-file-chooser-dialog.h>
#include <gtk/gtkprivate.h>
#include <libintl.h>

#include <libosso-abook/osso-abook-debug.h>
#include <libosso-abook/osso-abook-dialogs.h>
#include <libosso-abook/osso-abook-waitable.h>

#include "osso-abook-get-your-contacts-dialog.h"
#include "hw.h"
#include "sim.h"
#include "importer.h"

enum
{
  PROP_APP_DATA = 1
};

struct _OssoABookGetYourContactsDialogPrivate
{
  osso_abook_data *app_data;
  GtkWidget *get_sim_contacts_button;
  OssoABookWaitableClosure *sim_group_closure;
  GtkWidget *progress_bar;
  GtkWidget *add_contacts_note;
  guint pulse_progress_bar_id;
  guint sim_group_timeout_id;
  gulong sim_group_available_id;
};

typedef struct _OssoABookGetYourContactsDialogPrivate \
  OssoABookGetYourContactsDialogPrivate;

#define PRIVATE(dialog) \
  ((OssoABookGetYourContactsDialogPrivate *) \
   osso_abook_get_your_contacts_dialog_get_instance_private( \
     (OssoABookGetYourContactsDialog *)dialog))

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookGetYourContactsDialog,
  osso_abook_get_your_contacts_dialog,
  GTK_TYPE_DIALOG
);

static gboolean your_contacts_dialog_shown = FALSE;

gboolean
osso_abook_get_your_contacts_dialog_already_shown()
{
  return your_contacts_dialog_shown;
}

GtkWidget *
osso_abook_get_your_contacts_dialog_new(osso_abook_data *data)
{
  static GtkWidget *your_contacts_dialog = NULL;

  g_return_val_if_fail(data != NULL, NULL);

  if (!your_contacts_dialog)
  {
    your_contacts_dialog_shown = TRUE;

    your_contacts_dialog = g_object_new(
          OSSO_ABOOK_TYPE_GET_YOUR_CONTACTS_DIALOG,
          "transient-for", data->window,
          "app-data", data,
          "modal", TRUE,
          NULL);
    g_object_add_weak_pointer(G_OBJECT(your_contacts_dialog),
                              (gpointer *)&your_contacts_dialog);
  }

  return your_contacts_dialog;
}

static gboolean
check_low_disk_or_memory(GtkWindow *window)
{
  if (!osso_abook_check_disc_space(window))
    return FALSE;

  if (hw_is_lowmem_mode())
  {
      hildon_banner_show_information(
            GTK_WIDGET(window), NULL,
            dgettext("ke-recv", "memr_ib_operation_disabled"));
      return FALSE;
  }

  return TRUE;
}

static void
get_contacts_sync_clicked_cb(GtkButton *button,
                             OssoABookGetYourContactsDialog *dialog)
{
  if (!check_low_disk_or_memory(GTK_WINDOW(dialog)))
    return ;

  gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
  osso_abook_launch_applet(
        gtk_window_get_transient_for(GTK_WINDOW(dialog)),
        "libosso_maesync_ui.so");
}

static void
get_contacts_im_account_clicked_cb(GtkButton *button,
                                   OssoABookGetYourContactsDialog *dialog)
{
  if (!check_low_disk_or_memory(GTK_WINDOW(dialog)))
    return;

  gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
  osso_abook_add_im_account_dialog_run(
        gtk_window_get_transient_for(GTK_WINDOW(dialog)));

}

static void
add_contacts_note_response_cb(GtkDialog *note, gint response_id,
                              OssoABookGetYourContactsDialog *dialog)
{
  OssoABookGetYourContactsDialogPrivate *priv = PRIVATE(dialog);

  osso_abook_waitable_cancel(OSSO_ABOOK_WAITABLE(priv->app_data->sim_group),
                             priv->sim_group_closure);
  priv->sim_group_closure = 0;

  if (priv->sim_group_timeout_id)
  {
    g_source_remove(priv->sim_group_timeout_id);
    priv->sim_group_timeout_id = 0;
  }

  if (response_id == GTK_RESPONSE_CANCEL)
  {
    gtk_widget_destroy(GTK_WIDGET(note));
    gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
  }
  else
    g_warning("SIM 'waiting on card' progress bar got a non-cancel response (this shouldn't happen)");
}

static gboolean
pulse_progress_bar_cb(gpointer user_data)
{
  GtkWidget *progress_bar = PRIVATE(user_data)->progress_bar;

  if (!progress_bar)
    return FALSE;

  gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress_bar));

  return TRUE;
}

static gboolean
sim_group_timeout_cb(gpointer user_data)
{
  OssoABookGetYourContactsDialogPrivate *priv = PRIVATE(user_data);
  osso_abook_data *app_data = priv->app_data;

  if (priv->sim_group_closure)
  {
    osso_abook_waitable_cancel(OSSO_ABOOK_WAITABLE(app_data->sim_group),
                               priv->sim_group_closure);
    priv->sim_group_closure = NULL;

    if (priv->add_contacts_note)
    {
      gtk_widget_destroy(priv->add_contacts_note);
      priv->progress_bar = NULL;
      priv->add_contacts_note = NULL;
    }

    hildon_banner_show_information(GTK_WIDGET(app_data->window), NULL,
                                   dgettext("osso-connectivity-ui",
                                            "conn_ni_no_sim_card_in"));
    gtk_dialog_response(GTK_DIALOG(user_data), GTK_RESPONSE_CANCEL);
  }

  return FALSE;
}

static void
sim_group_ready_cb(OssoABookWaitable *waitable, const GError *error,
                   gpointer data)
{
  OssoABookGetYourContactsDialogPrivate *priv = PRIVATE(data);
  osso_abook_data *app_data = priv->app_data;

  if (error)
  {
    osso_abook_handle_gerror(GTK_WINDOW(app_data->window), g_error_copy(error));
    gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_CANCEL);
  }
  else
  {
    if (priv->sim_group_timeout_id)
    {
      g_source_remove(priv->sim_group_timeout_id);
      priv->sim_group_timeout_id = 0;
    }

    priv->sim_group_closure = NULL;

    if (priv->add_contacts_note)
    {
      g_signal_handlers_disconnect_matched(
            priv->add_contacts_note, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
            0, 0, NULL, add_contacts_note_response_cb, data);
    }

    if (priv->pulse_progress_bar_id)
    {
      g_source_remove(priv->pulse_progress_bar_id);
      priv->pulse_progress_bar_id = 0;
    }

    sim_import_all(GTK_WIDGET(app_data->window), OSSO_ABOOK_SIM_GROUP(waitable),
                   priv->add_contacts_note, priv->progress_bar);
    priv->progress_bar = NULL;
    priv->add_contacts_note = NULL;

    gtk_dialog_response(GTK_DIALOG(data), GTK_RESPONSE_OK);
  }
}

static void
get_contacts_sim_card_clicked_cb(GtkButton *button,
                                 OssoABookGetYourContactsDialog *dialog)
{
  OssoABookGetYourContactsDialogPrivate *priv = PRIVATE(dialog);
  osso_abook_data *app_data = priv->app_data;
  GError *error = NULL;

  if (!check_low_disk_or_memory(GTK_WINDOW(dialog)))
    return;

  gtk_widget_hide(GTK_WIDGET(dialog));

  if (!osso_abook_waitable_is_ready(OSSO_ABOOK_WAITABLE(app_data->sim_group),
                                    &error))
  {
    if (error)
    {
      osso_abook_handle_gerror(GTK_WINDOW(dialog), error);
      gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
      return;
    }

    priv->progress_bar = gtk_progress_bar_new();
    priv->pulse_progress_bar_id = gdk_threads_add_timeout(
          175, pulse_progress_bar_cb, dialog);
    priv->add_contacts_note = hildon_note_new_cancel_with_progress_bar(
        GTK_WINDOW(app_data->window), dgettext(NULL, "addr_pb_notification13"),
        GTK_PROGRESS_BAR(priv->progress_bar));
    g_signal_connect(priv->add_contacts_note, "response",
                     G_CALLBACK(add_contacts_note_response_cb), dialog);
    gtk_widget_show(priv->add_contacts_note);

    priv->sim_group_timeout_id =
        gdk_threads_add_timeout(60000, sim_group_timeout_cb, dialog);
  }

  priv->sim_group_closure = osso_abook_waitable_call_when_ready(
        OSSO_ABOOK_WAITABLE(app_data->sim_group),
        sim_group_ready_cb, dialog, NULL);

}

static void
sim_group_available_cb(OssoABookSimGroup *sim_group,
                       OssoABookGetYourContactsDialog *dialog)
{
  gtk_widget_show(PRIVATE(dialog)->get_sim_contacts_button);
}

static void
import_contacts_response_cb(GtkDialog *import_dialog, gint response_id,
                            OssoABookGetYourContactsDialog *dialog)
{
  gtk_widget_destroy(GTK_WIDGET(import_dialog));
  gtk_dialog_response(GTK_DIALOG(dialog), response_id);
}

static void
select_files_to_import(GtkWindow *parent, GtkFileChooserAction action,
                       gpointer response_cb)
{
  GtkWidget *chooser;
  const gchar *docs_dir;
  GtkFileFilter *filter;

  if (!check_low_disk_or_memory(parent))
    return;

  gtk_widget_hide(GTK_WIDGET(parent));
  chooser = hildon_file_chooser_dialog_new_with_properties(
        gtk_window_get_transient_for(parent),
        "title", dgettext(NULL, "addr_ti_import_contacts"),
        "action", action,
        NULL);
  docs_dir = g_get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);

  if (g_file_test(docs_dir, G_FILE_TEST_IS_DIR) )
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), docs_dir);

  filter = gtk_file_filter_new();
  gtk_file_filter_add_pattern(filter, "*.vcf");
  gtk_file_filter_add_pattern(filter, "*.VCF");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
  gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(chooser), filter);
  g_object_set(chooser, "show-folder-button", FALSE, NULL);
  g_signal_connect(chooser, "response",
                   G_CALLBACK(response_cb), parent);
  gtk_widget_show(chooser);
}

static void
import_contacts_file_response_cb(GtkWidget *chooser, gint response_id,
                                 GtkDialog *dialog)
{
  if (response_id == GTK_RESPONSE_OK)
  {
    GtkWindow *parent = gtk_window_get_transient_for(GTK_WINDOW(chooser));
    gchar *uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(chooser));

    gtk_dialog_response(dialog, GTK_RESPONSE_OK);
    gtk_widget_destroy(GTK_WIDGET(chooser));

    if (uri)
    {
      do_import(parent, uri, 0, 0);
      g_free(uri);
    }
  }
  else
  {
    gtk_widget_show(GTK_WIDGET(dialog));
    gtk_widget_destroy(GTK_WIDGET(chooser));
  }
}

static void
import_contacts_folder_response_cb(GtkWidget *chooser, gint response_id,
                                   GtkDialog *dialog)
{
  if (response_id == GTK_RESPONSE_OK)
  {
    GtkWindow *parent = gtk_window_get_transient_for(GTK_WINDOW(chooser));
    gchar *uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(chooser));

    gtk_dialog_response(dialog, GTK_RESPONSE_OK);
    gtk_widget_destroy(GTK_WIDGET(chooser));

    if (uri)
    {
      OSSO_ABOOK_NOTE(CONTACT_ADD, "importing folder: %s", uri);
      do_import_dir(parent, uri, 0, 0);
      g_free(uri);
    }
  }
  else
  {
    gtk_widget_show(GTK_WIDGET(dialog));
    gtk_widget_destroy(GTK_WIDGET(chooser));
  }
}

static void
get_from_file_clicked_cb(GtkWidget *button, GtkWidget *dialog)
{
  select_files_to_import(GTK_WINDOW(dialog),
                         GTK_FILE_CHOOSER_ACTION_OPEN,
                         import_contacts_file_response_cb);
}

static void
get_from_folder_clicked_cb(GtkWidget *button, GtkWidget *dialog)
{
  select_files_to_import(GTK_WINDOW(dialog),
                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                         import_contacts_folder_response_cb);
}

static void
get_contacts_file_clicked_cb(GtkButton *widget,
                             OssoABookGetYourContactsDialog *dialog)
{
  gtk_widget_hide(GTK_WIDGET(dialog));

  if (check_low_disk_or_memory(GTK_WINDOW(dialog)))
  {
    GtkWindow *parent = gtk_window_get_transient_for(GTK_WINDOW(dialog));
    GtkWidget *import_dialog;
    GtkWidget *button;
    GtkWidget *content_area;

    import_dialog = gtk_widget_new(
                      GTK_TYPE_DIALOG,
                      "title", dgettext(NULL, "addr_ti_import_contacts"),
                      "transient-for", parent,
                      "modal", parent ? TRUE : FALSE,
                      "destroy-with-parent", TRUE,
                      "has-separator", FALSE,
                      NULL);
    g_signal_connect(import_dialog, "response",
                     G_CALLBACK(import_contacts_response_cb), dialog);
    gtk_widget_hide(gtk_dialog_get_action_area(GTK_DIALOG(import_dialog)));
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(import_dialog));

    button = hildon_gtk_button_new(HILDON_SIZE_FINGER_HEIGHT);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(get_from_file_clicked_cb), import_dialog);
    gtk_button_set_label(GTK_BUTTON(button),
                         dgettext(NULL, "addr_bd_get_from_file"));
    gtk_container_add(GTK_CONTAINER(content_area), button);
    gtk_widget_show(button);

    button = hildon_gtk_button_new(HILDON_SIZE_FINGER_HEIGHT);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(get_from_folder_clicked_cb), import_dialog);
    gtk_button_set_label(GTK_BUTTON(button),
                         dgettext(NULL, "addr_bd_get_from_folder"));
    gtk_container_add(GTK_CONTAINER(content_area), button);
    gtk_widget_show(button);

    gtk_widget_show(import_dialog);
  }
}

static void
create_buttons(OssoABookGetYourContactsDialog *dialog, GtkBox *box)
{
  OssoABookGetYourContactsDialogPrivate *priv = PRIVATE(dialog);
  GtkWidget *button;

  button = hildon_button_new_with_text(
        HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
        dgettext(0, "addr_bd_get_contacts_sync"), NULL);
  g_signal_connect(button, "clicked",
                   G_CALLBACK(get_contacts_sync_clicked_cb), dialog);
  gtk_box_pack_start(box, button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = hildon_button_new_with_text(
        HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
        dgettext(0, "addr_bd_get_contacts_im_account"), NULL);
  g_signal_connect(button, "clicked",
                   G_CALLBACK(get_contacts_im_account_clicked_cb), dialog);
  gtk_box_pack_start(box, button, TRUE, TRUE, 0);
  gtk_widget_show(button);

  button = hildon_button_new_with_text(
        HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
        dgettext(0, "addr_bd_get_contacts_sim_card"), NULL);
  priv->get_sim_contacts_button = button;
  g_signal_connect(button, "clicked",
                   G_CALLBACK(get_contacts_sim_card_clicked_cb), dialog);
  gtk_box_pack_start(box, button, TRUE, TRUE, 0);

  if (!priv->app_data->sim_group_ready)
  {
    gtk_widget_hide(priv->get_sim_contacts_button);
    priv->sim_group_available_id = g_signal_connect(
          priv->app_data->sim_group, "available",
          G_CALLBACK(sim_group_available_cb), dialog);
  }
  else
    gtk_widget_show(priv->get_sim_contacts_button);

  button = hildon_button_new_with_text(
        HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
        dgettext(0, "addr_bd_get_contacts_file"), NULL);
  g_signal_connect(button, "clicked",
                   G_CALLBACK(get_contacts_file_clicked_cb), dialog);
  gtk_box_pack_start(box, button, TRUE, TRUE, 0);
  gtk_widget_show(button);

#if 0
  if (AsConfigAccountExists((int)"ActiveSyncAccount1") )
  {
    button = hildon_button_new_with_text(
          HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
          dgettext(0, "addr_bd_import_exchange_contacts"), NULL);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(import_exchange_contacts_clicked_cb), dialog);
    gtk_box_pack_start(box, button, TRUE, TRUE, 0);
    gtk_widget_show(button);
  }
#endif
}

static void
osso_abook_get_your_contacts_dialog_constructed(GObject *object)
{
  OssoABookGetYourContactsDialog *dialog =
      OSSO_ABOOK_GET_YOUR_CONTACTS_DIALOG(object);
  OssoABookGetYourContactsDialogPrivate *priv = PRIVATE(dialog);
  GtkWidget *vbox;

  g_return_if_fail(priv->app_data != NULL);

  priv->get_sim_contacts_button = NULL;
  priv->sim_group_closure = 0;
  priv->progress_bar = 0;
  priv->add_contacts_note = NULL;

  gtk_window_set_title(GTK_WINDOW(dialog),
                       dgettext(NULL, "addr_ti_get_contacts"));
  gtk_widget_hide(GTK_DIALOG(dialog)->action_area);
  vbox = gtk_vbox_new(TRUE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show(vbox);

  create_buttons(dialog, GTK_BOX(vbox));
}

static void
osso_abook_get_your_contacts_dialog_set_property(GObject *object,
                                                 guint property_id,
                                                 const GValue *value,
                                                 GParamSpec *pspec)
{
  switch (property_id)
  {
    case PROP_APP_DATA:
    {
      PRIVATE(object)->app_data = g_value_get_pointer(value);
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_get_your_contacts_dialog_get_property(GObject *object,
                                                 guint property_id,
                                                 GValue *value,
                                                 GParamSpec *pspec)
{
  switch (property_id)
  {
    case PROP_APP_DATA:
    {
      g_value_set_pointer(value, PRIVATE(object)->app_data);
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_get_your_contacts_dialog_dispose(GObject *object)
{
  OssoABookGetYourContactsDialogPrivate *priv = PRIVATE(object);

  if (priv->pulse_progress_bar_id)
  {
    g_source_remove(priv->pulse_progress_bar_id);
    priv->pulse_progress_bar_id = 0;
  }

  if (priv->sim_group_timeout_id)
  {
    g_source_remove(priv->sim_group_timeout_id);
    priv->sim_group_timeout_id = 0;
  }

  if (priv->sim_group_available_id)
  {
    g_signal_handler_disconnect(priv->app_data->sim_group,
                                priv->sim_group_available_id);
    priv->sim_group_available_id = 0;
  }

  if (priv->sim_group_closure)
  {
    osso_abook_waitable_cancel(OSSO_ABOOK_WAITABLE(priv->app_data->sim_group),
                               priv->sim_group_closure);
    priv->sim_group_closure = NULL;
  }

  G_OBJECT_CLASS(osso_abook_get_your_contacts_dialog_parent_class)->
    dispose(object);
}

static void
osso_abook_get_your_contacts_dialog_finalize(GObject *object)
{
  your_contacts_dialog_shown = FALSE;

  G_OBJECT_CLASS(osso_abook_get_your_contacts_dialog_parent_class)->
    finalize(object);
}

static void
osso_abook_get_your_contacts_dialog_class_init(
    OssoABookGetYourContactsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->constructed = osso_abook_get_your_contacts_dialog_constructed;
  object_class->set_property = osso_abook_get_your_contacts_dialog_set_property;
  object_class->get_property = osso_abook_get_your_contacts_dialog_get_property;
  object_class->dispose = osso_abook_get_your_contacts_dialog_dispose;
  object_class->finalize = osso_abook_get_your_contacts_dialog_finalize;

  g_object_class_install_property(
    object_class, PROP_APP_DATA,
    g_param_spec_pointer(
      "app-data",
      "The app data",
      "The AppData",
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
osso_abook_get_your_contacts_dialog_init(OssoABookGetYourContactsDialog *dialog)
{
}
