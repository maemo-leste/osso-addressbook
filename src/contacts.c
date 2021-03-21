/*
 * contacts.c
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
#include <libosso-abook/osso-abook-errors.h>
#include <libosso-abook/osso-abook-touch-contact-starter.h>
#include <libosso-abook/osso-abook-util.h>
#include <rtcom-eventlogger-ui/rtcom-log-model.h>
#include <rtcom-eventlogger-ui/rtcom-log-view.h>
#include <libebook-contacts/libebook-contacts.h>

#include "app.h"
#include "actions.h"
#include "contacts.h"

static gboolean your_contacts_dialog_shown = FALSE;

gboolean
osso_abook_get_your_contacts_dialog_already_shown()
{
  return your_contacts_dialog_shown;
}

/* TODO: Move what's necessary to actions.c */

void
merge(osso_abook_data *data, const char *uid)
{
  OssoABookTreeView *contact_view;
  GtkTreeView *tree_view;
  GtkTreeModel *tree_model;
  GtkTreeModelFilter *tf;
  GtkTreePath *model_path;
  GtkTreeIter iter, filter_iter;

  if (!osso_abook_contact_model_find_contact(data->contact_model, uid, &iter))
  {
    g_free(data->selected_row_uid);
    data->selected_row_uid = g_strdup(uid);
    return;
  }

  contact_view = OSSO_ABOOK_TREE_VIEW(data->contact_view);
  tree_view = osso_abook_tree_view_get_tree_view(contact_view);
  tree_model = gtk_tree_view_get_model(tree_view);
  tf = GTK_TREE_MODEL_FILTER(tree_model);
  gtk_tree_model_filter_convert_child_iter_to_iter(tf, &filter_iter, &iter);
  model_path = gtk_tree_model_get_path(tree_model, &filter_iter);
  select_contact_row(data, model_path);
  gtk_tree_path_free(model_path);
}

static void
merge_cb(const char *uid, gpointer user_data)
{
  if (uid)
    merge(user_data, uid);
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

