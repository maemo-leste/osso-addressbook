/*
 * osso-abook-recent-view.h
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

#ifndef OSSOABOOKRECENTVIEW_H
#define OSSOABOOKRECENTVIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_RECENT_VIEW \
                (osso_abook_recent_view_get_type ())
#define OSSO_ABOOK_RECENT_VIEW(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_RECENT_VIEW, \
                 OssoABookRecentView))
#define OSSO_ABOOK_RECENT_VIEW_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_RECENT_VIEW, \
                 OssoABookRecentViewClass))
#define OSSO_ABOOK_IS_RECENT_VIEW(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_RECENT_VIEW))
#define OSSO_ABOOK_IS_RECENT_VIEW_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_RECENT_VIEW))
#define OSSO_ABOOK_RECENT_VIEW_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_RECENT_VIEW, \
                 OssoABookRecentViewClass))

typedef struct _OssoABookRecentViewClass OssoABookRecentViewClass;
typedef struct _OssoABookRecentView OssoABookRecentView;

struct _OssoABookRecentViewClass
{
  GtkVBoxClass parent_class;
};

struct _OssoABookRecentView
{
  GtkVBox parent;
};

void
osso_abook_recent_view_install_live_search(OssoABookRecentView *self,
                                           HildonWindow *window);
void
osso_abook_recent_view_hide_live_search(OssoABookRecentView *self);

G_END_DECLS

#endif // OSSOABOOKRECENTVIEW_H
