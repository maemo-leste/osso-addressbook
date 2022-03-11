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

#include <gtk/gtk.h>

void
do_import(GtkWindow *parent, const char *uri, GSourceFunc import_finished_cb,
          gpointer user_data)
{
  g_assert(0 && "implement me");
}

void
do_import_dir(GtkWindow *parent, const gchar *uri,
              GSourceFunc import_finished_cb, gpointer user_data)
{
  g_assert(0 && "implement me");
}

