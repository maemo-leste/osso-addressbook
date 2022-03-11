/*
 * osso-abook-sim-group.h
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

#ifndef OSSOABOOKSIMGROUP_H
#define OSSOABOOKSIMGROUP_H

#include <libosso-abook/osso-abook-group.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_SIM_GROUP \
                (osso_abook_sim_group_get_type ())
#define OSSO_ABOOK_SIM_GROUP(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_SIM_GROUP, \
                 OssoABookSimGroup))
#define OSSO_ABOOK_SIM_GROUP_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_SIM_GROUP, \
                 OssoABookSimGroupClass))
#define OSSO_ABOOK_IS_SIM_GROUP(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_SIM_GROUP))
#define OSSO_ABOOK_IS_SIM_GROUP_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_SIM_GROUP))
#define OSSO_ABOOK_SIM_GROUP_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_SIM_GROUP, \
                 OssoABookSimGroupClass))

typedef struct _OssoABookSimGroupClass OssoABookSimGroupClass;
typedef struct _OssoABookSimGroup OssoABookSimGroup;

struct _OssoABookSimGroupClass
{
  OssoABookGroupClass parent_class;
  void (*available)(OssoABookSimGroup *sim_group);
  void (*voicemail_contact_available)(OssoABookSimGroup *sim_group);
  void (*capabilities_available)(OssoABookSimGroup *sim_group);

};

struct _OssoABookSimGroup
{
  OssoABookGroup parent;
};

GType
osso_abook_sim_group_get_type(void) G_GNUC_CONST;

OssoABookSimGroup *
osso_abook_sim_group_new();

OssoABookContact *
osso_abook_sim_group_get_voicemail_contact(OssoABookSimGroup *sim_group);

const gchar *
osso_abook_sim_group_get_imsi(OssoABookSimGroup *sim_group);

GList *
osso_abook_sim_group_list_adn_contacts(OssoABookSimGroup *sim_group);

G_END_DECLS

#endif // OSSOABOOKSIMGROUP_H
