/*
 * osso-abook-recent-view.c
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

#include <hildon/hildon.h>
#include <gtk/gtkprivate.h>

#include <rtcom-eventlogger/eventlogger.h>
#include <rtcom-eventlogger-ui/rtcom-log-model.h>
#include <rtcom-eventlogger-ui/rtcom-log-view.h>

#include <libosso-abook/osso-abook-contact.h>
#include <libosso-abook/osso-abook-aggregator.h>
#include <libosso-abook/osso-abook-util.h>

#include <libintl.h>

#include "osso-abook-recent-view.h"

struct _OssoABookRecentViewPrivate
{
  RTComEl *el;
  GtkWidget *pannable_area;
  GtkWidget *no_contacts_label;
  GtkWidget *el_view;
  GtkWidget *live_search;
  OssoABookAggregator *aggregator;
  HildonWindow *window;
};

typedef struct _OssoABookRecentViewPrivate OssoABookRecentViewPrivate;

#define OSSO_ABOOK_RECENT_VIEW_PRIVATE(recent_view) \
                ((OssoABookRecentViewPrivate *)osso_abook_recent_view_get_instance_private(recent_view))

enum
{
  PROP_AGGREGATOR = 1,
};

enum {
  SHOW_CONTACT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {};

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookRecentView,
  osso_abook_recent_view,
  GTK_TYPE_VBOX
);

static void
osso_abook_recent_view_get_property(GObject *object, guint property_id,
                                    GValue *value, GParamSpec *pspec)
{
  OssoABookRecentView *self = OSSO_ABOOK_RECENT_VIEW(object);
  OssoABookRecentViewPrivate *priv = OSSO_ABOOK_RECENT_VIEW_PRIVATE(self);

  switch(property_id)
  {
    case PROP_AGGREGATOR:
    {
      g_value_set_object(value, priv->aggregator);
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_recent_view_set_property(GObject *object, guint property_id,
                                    const GValue *value, GParamSpec *pspec)
{
  OssoABookRecentView *self = OSSO_ABOOK_RECENT_VIEW(object);
  OssoABookRecentViewPrivate *priv = OSSO_ABOOK_RECENT_VIEW_PRIVATE(self);

  switch(property_id)
  {
    case PROP_AGGREGATOR:
    {
      if (priv->aggregator)
        g_object_unref(priv->aggregator);

      priv->aggregator = g_value_dup_object(value);
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_recent_view_dispose(GObject *object)
{
  OssoABookRecentView *view = OSSO_ABOOK_RECENT_VIEW(object);
  OssoABookRecentViewPrivate *priv = OSSO_ABOOK_RECENT_VIEW_PRIVATE(view);

  if (priv->live_search)
  {
    g_object_unref(priv->live_search);
    priv->live_search = NULL;
  }

  if (priv->el)
  {
    g_object_unref(priv->el);
    priv->el = NULL;
  }

  if (priv->aggregator)
  {
    g_object_unref(priv->aggregator);
    priv->aggregator = NULL;
  }

  G_OBJECT_CLASS(osso_abook_recent_view_parent_class)->dispose(object);
}

static void
hide_no_contacts_label(OssoABookRecentView *self)
{
  OssoABookRecentViewPrivate *priv = OSSO_ABOOK_RECENT_VIEW_PRIVATE(self);

  gtk_widget_hide(priv->no_contacts_label);
  gtk_widget_set_no_show_all(priv->pannable_area, FALSE);
  gtk_widget_show_all(priv->pannable_area);
  gtk_widget_set_no_show_all(priv->pannable_area, TRUE);
}

static void
show_no_contacts_label(OssoABookRecentView *self, const gchar *text)
{
  OssoABookRecentViewPrivate *priv = OSSO_ABOOK_RECENT_VIEW_PRIVATE(self);

  gtk_widget_hide(priv->pannable_area);
  gtk_label_set_text(GTK_LABEL(priv->no_contacts_label), text);
  gtk_widget_show(priv->no_contacts_label);
}

static void
recent_row_inserted_cb(GtkTreeModel *tree_model, GtkTreePath *path,
                       GtkTreeIter *iter, gpointer user_data)
{
  hide_no_contacts_label(user_data);
}

static void
recent_row_deleted_cb(GtkTreeModel *tree_model, GtkTreePath *path,
                      gpointer user_data)
{
  OssoABookRecentView *recent_view = user_data;
  GtkTreeModel *model =
      gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(tree_model));
  const char *msgid;
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter_first(model, &iter))
    msgid = "addr_ia_no_contacts";
  else if (!gtk_tree_model_get_iter_first(tree_model, &iter))
    msgid = "addr_ia_search_not_found";
  else
    return;

  show_no_contacts_label(recent_view, dgettext(NULL, msgid));
}

static void
recent_show_cb(GtkWidget *widget, gpointer user_data)
{
  OssoABookRecentViewPrivate *priv =
      OSSO_ABOOK_RECENT_VIEW_PRIVATE(OSSO_ABOOK_RECENT_VIEW(widget));

  hildon_live_search_widget_hook(HILDON_LIVE_SEARCH(priv->live_search),
                                 gtk_widget_get_toplevel(widget),
                                 priv->el_view);
}

static void
recent_hide_cb(GtkWidget *widget, gpointer user_data)
{
  OssoABookRecentViewPrivate *priv =
      OSSO_ABOOK_RECENT_VIEW_PRIVATE(OSSO_ABOOK_RECENT_VIEW(widget));

  gtk_widget_hide(priv->live_search);
  hildon_live_search_widget_unhook(HILDON_LIVE_SEARCH(priv->live_search));
}

static void
recent_row_activated_cb(GtkTreeView *tree_view, GtkTreePath *path,
                        GtkTreeViewColumn *column, gpointer user_data)
{
  g_assert(0);
}

static void
osso_abook_recent_view_constructed(GObject *object)
{
  OssoABookRecentView *self = OSSO_ABOOK_RECENT_VIEW(object);
  OssoABookRecentViewPrivate *priv = OSSO_ABOOK_RECENT_VIEW_PRIVATE(self);
  RTComLogModel *el_model = rtcom_log_model_new();
  GtkTreeModel *tree_model_filter;
  GtkTreeIter iter;

  priv->el = g_object_ref(rtcom_log_model_get_eventlogger(el_model));

  g_warn_if_fail(priv->aggregator);

  rtcom_log_model_set_abook_aggregator(el_model,
                                       OSSO_ABOOK_AGGREGATOR(priv->aggregator));
  rtcom_log_model_set_group_by(el_model, RTCOM_EL_QUERY_GROUP_BY_CONTACT);
  rtcom_log_model_set_limit(el_model, 60);
  rtcom_log_model_populate(el_model, NULL);

  priv->el_view = rtcom_log_view_new();
  rtcom_log_view_set_highlight_new_events(RTCOM_LOG_VIEW(priv->el_view), 0);

  priv->live_search = hildon_live_search_new();
  g_object_ref_sink(priv->live_search);

  tree_model_filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(el_model), NULL);
  hildon_live_search_set_filter(HILDON_LIVE_SEARCH(priv->live_search),
                                GTK_TREE_MODEL_FILTER(tree_model_filter));

  rtcom_log_view_set_model(RTCOM_LOG_VIEW(priv->el_view), tree_model_filter);
  g_object_unref(tree_model_filter);

  hildon_live_search_set_visible_func(
        HILDON_LIVE_SEARCH(priv->live_search),
        rtcom_log_model_filter_visible_func, NULL, NULL);

  priv->no_contacts_label =
      gtk_label_new(dgettext(NULL, "addr_ia_no_contacts"));

  gtk_misc_set_alignment(GTK_MISC(priv->no_contacts_label), 0.5, 0.5);
  hildon_helper_set_logical_font(priv->no_contacts_label, "LargeSystemFont");
  hildon_helper_set_logical_color(priv->no_contacts_label, GTK_RC_FG,
                                  GTK_STATE_NORMAL, "SecondaryTextColor");
  priv->pannable_area = osso_abook_pannable_area_new();
  gtk_container_add(GTK_CONTAINER(priv->pannable_area), priv->el_view);

  gtk_box_pack_start(GTK_BOX(self), priv->no_contacts_label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(self), priv->pannable_area, TRUE, TRUE, 0);
  gtk_widget_set_no_show_all(priv->no_contacts_label, TRUE);
  gtk_widget_set_no_show_all(priv->pannable_area, TRUE);

  if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(tree_model_filter), &iter) )
    hide_no_contacts_label(self);
  else
    show_no_contacts_label(self, dgettext(NULL, "addr_ia_no_contacts"));

  g_object_unref(el_model);

  g_signal_connect(priv->el_view, "row-activated",
                   G_CALLBACK(recent_row_activated_cb), self);

  g_signal_connect(tree_model_filter, "row-inserted",
                   G_CALLBACK(recent_row_inserted_cb), self);
  g_signal_connect(tree_model_filter, "row-deleted",
                   G_CALLBACK(recent_row_deleted_cb), self);

  g_signal_connect(self, "hide", G_CALLBACK(recent_hide_cb), NULL);
  g_signal_connect(self, "show", G_CALLBACK(recent_show_cb), NULL);
}

static void
osso_abook_recent_view_class_init(OssoABookRecentViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->get_property = osso_abook_recent_view_get_property;
  object_class->set_property = osso_abook_recent_view_set_property;
  object_class->constructed = osso_abook_recent_view_constructed;
  object_class->dispose = osso_abook_recent_view_dispose;

  g_object_class_install_property(
        object_class, PROP_AGGREGATOR,
        g_param_spec_object(
                 "aggregator",
                 "Aggregator",
                 "The aggregator",
                 OSSO_ABOOK_TYPE_AGGREGATOR,
                 GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));

  signals[SHOW_CONTACT] =
      g_signal_new("show-contact", OSSO_ABOOK_TYPE_RECENT_VIEW,
                   G_SIGNAL_RUN_LAST, 0, 0, NULL,
                   g_cclosure_marshal_VOID__OBJECT,
                   G_TYPE_NONE, 1, OSSO_ABOOK_TYPE_CONTACT);
}

static void
osso_abook_recent_view_init(OssoABookRecentView *self)
{
}

void
osso_abook_recent_view_install_live_search(OssoABookRecentView *self,
                                           HildonWindow *window)
{
  OssoABookRecentViewPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_RECENT_VIEW(self));

  priv = OSSO_ABOOK_RECENT_VIEW_PRIVATE(self);

  g_return_if_fail(priv->window == NULL);

  priv->window = window;
  hildon_window_add_toolbar(window, GTK_TOOLBAR(priv->live_search));
}

void
osso_abook_recent_view_hide_live_search(OssoABookRecentView *self)
{
  OssoABookRecentViewPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_RECENT_VIEW(self));

  priv = OSSO_ABOOK_RECENT_VIEW_PRIVATE(self);

  if (priv->window)
    gtk_widget_hide(priv->live_search);
}

void
osso_abook_recent_view_remove_live_search(OssoABookRecentView *self)
{
  OssoABookRecentViewPrivate *priv;
  HildonWindow *window;

  g_return_if_fail(OSSO_ABOOK_IS_RECENT_VIEW(self));

  priv = OSSO_ABOOK_RECENT_VIEW_PRIVATE(self);

  g_return_if_fail(HILDON_IS_WINDOW (priv->window));

  hildon_window_remove_toolbar(window, GTK_TOOLBAR(priv->live_search));

  /* FIXME - Umm... what? */
  priv->window = NULL;
}

OssoABookRecentView *
osso_abook_recent_view_new(OssoABookAggregator *aggregator)
{
  return g_object_new(OSSO_ABOOK_TYPE_RECENT_VIEW,
                      "aggregator", aggregator,
                      NULL);
}

void
osso_abook_recent_view_scroll_to_top(OssoABookRecentView *self)
{
  OssoABookRecentViewPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_RECENT_VIEW(self));

  priv = OSSO_ABOOK_RECENT_VIEW_PRIVATE(self);

  if (gtk_widget_get_realized(GTK_WIDGET(priv->pannable_area)))
  {
    hildon_pannable_area_jump_to(HILDON_PANNABLE_AREA(priv->pannable_area),
                                 -1, 0);
  }
}
