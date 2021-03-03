/*
 * hw.c
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

#include <libosso.h>

#include "app.h"
#include "hw.h"

static gboolean memory_low_ind;
static gboolean system_inactivity_ind;
static gboolean shutdown_ind;
static gboolean is_monitoring;
static osso_hw_state_t hw_state =
{
  TRUE,
  FALSE,
  TRUE,
  TRUE,
  OSSO_DEVMODE_NORMAL
};

static void
hw_event_cb(osso_hw_state_t *state, gpointer data)
{
  memory_low_ind = state->memory_low_ind;
  system_inactivity_ind = state->system_inactivity_ind;
  shutdown_ind = state->shutdown_ind;

  if (state->shutdown_ind)
    exit(0);
}

void
hw_start_monitor(osso_abook_data *data)
{
  g_return_if_fail(!is_monitoring);
  g_return_if_fail(data != NULL);

  osso_hw_set_event_cb(data->osso, &hw_state, hw_event_cb, data);
  is_monitoring = TRUE;
}

void
hw_stop_monitor(osso_abook_data *data)
{
  g_return_if_fail(is_monitoring);
  g_return_if_fail(data != NULL);

  osso_hw_unset_event_cb(data->osso, &hw_state);
  is_monitoring = FALSE;
}

gboolean
hw_is_under_valgrind()
{
  const char *s = g_getenv("LD_PRELOAD");

  if (s)
    return strstr(s, "/vgpreload_core.so") != NULL;

  return FALSE;
}

gboolean
hw_is_lowmem_mode()
{
  g_return_val_if_fail(is_monitoring, FALSE);

  return memory_low_ind;
}
