/*
 * sim.c
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

#include <libosso-abook/osso-abook-util.h>

#include "app.h"
#include "sim.h"

void
sim_import_all(GtkWidget *parent, OssoABookSimGroup *sim_group, GtkWidget *note,
               GtkWidget *progress_bar)

{
  /* implement me */
  g_assert(0);
}

static void
_import_contact(EBook *book, EContact *simcontact)
{
  /* implement me */
  g_assert(0);
}

void
sim_import_contact(OssoABookContact *contact)
{
  EBook *book;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT(contact));

  book = osso_abook_system_book_dup_singleton(FALSE, NULL);
  _import_contact(book, E_CONTACT(contact));
  g_object_unref(book);
}

void
open_sim_view_window(osso_abook_data *data, OssoABookGroup *group)
{
  /* implement me */
  g_assert(0);
}
