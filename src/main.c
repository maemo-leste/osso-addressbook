#include <string.h>
#include <libintl.h>

#include <libosso-abook/osso-abook-init.h>

#include "app.h"

#ifdef OSSO_ABOOK_DEBUG
void
list_leaked_windows()
{
  GList *l = gtk_window_list_toplevels();

  OSSO_ABOOK_NOTE(GTK,"%d toplevel window(s) leaked", g_list_length(l));

  for (; l; l = g_list_delete_link(l, l))
  {
    const gchar *title = gtk_window_get_title((GtkWindow *)l->data);

    OSSO_ABOOK_NOTE(GTK, "%s@%p (%s)\n", G_OBJECT_TYPE_NAME(l->data), l->data,
                    title ? title : "<untitled>");
  }
}
#endif

static gboolean
backend_died_cb(EBook *book, gpointer user_data)
{
  osso_abook_data *data = user_data;
  int status = 0;

  if (!g_file_test("/tmp/osso-abook-shutdown-in-progress", G_FILE_TEST_EXISTS))
  {
    g_critical("%s: Backend of %s died. Cannot recover. Aborting.",
               __FUNCTION__, e_source_get_uid(e_book_get_source(book)));

    if (gtk_widget_get_visible(GTK_WIDGET(data->window)))
      status = 23;
  }

  exit(status);
}

int
main(int argc, char **argv)
{
  osso_abook_data data;
  GOptionEntry entries[] =
  {
    {
      .long_name = "show-ui",
      .description = "Show the application window on startup",
      .arg_data = &data.show_ui
    },
    {
      .long_name = "quit-on-close",
      .description = "Quit when the application window is closed",
      .arg_data = &data.quit_on_close
    },
    {
    }
  };
  osso_context_t *osso;
  GtkIconSource *icon_source;
  GtkIconSet *icon_set;
  GtkIconFactory *icon_factory;
  int res = 1;
  GError *error = NULL;

#if !GLIB_CHECK_VERSION(2,32,0)
  g_thread_init(NULL);
#endif
  gdk_threads_init();
  bindtextdomain("osso-addressbook", "/usr/share/locale");
  bind_textdomain_codeset("osso-addressbook", "UTF-8");
  textdomain("osso-addressbook");
  osso = osso_initialize("osso_addressbook", "4.20100629", 1, 0);

  if (!osso)
  {
    g_critical("Error initializing osso\n");
    res = 1;
    goto err;
  }

  memset(&data, 0, sizeof(data));
  gtk_rc_parse_string("style \"default\" {\n}\nclass \"HildonAppMenu\" style \"default\"\n");

  if (!osso_abook_init_with_args(&argc, &argv, osso, 0, entries, 0, &error))
  {
    if (error && g_option_error_quark() == error->domain)
    {
      g_printerr("Usage error: %s\n", error->message);
      g_error_free(error);
      res = 2;
    }
    else
    {
      g_critical("Unable to initialize libosso-abook");
      g_clear_error(&error);
    }

    goto err_osso;
  }

  osso_abook_set_backend_died_func(backend_died_cb, &data);

  OSSO_ABOOK_NOTE(STARTUP, STARTUP_PROGRESS_SEPARATOR);

  icon_source = gtk_icon_source_new();
  gtk_icon_source_set_icon_name(icon_source, "qgn_addr_icon_search_in_group");
  icon_set = gtk_icon_set_new();
  gtk_icon_set_add_source(icon_set, icon_source);
  gtk_icon_source_free(icon_source);
  icon_factory = gtk_icon_factory_new();
  gtk_icon_factory_add(icon_factory, "qgn_addr_icon_search_in_group", icon_set);
  gtk_icon_set_unref(icon_set);
  gtk_icon_factory_add_default(icon_factory);
  g_object_unref(icon_factory);
  gtk_window_set_default_icon_name("qgn_list_addressbook");

  OSSO_ABOOK_NOTE(STARTUP, STARTUP_PROGRESS_SEPARATOR);

  if (app_create(osso, argc < 2 ? NULL : argv[1] , &data))
  {
    OSSO_ABOOK_NOTE(STARTUP, STARTUP_PROGRESS_SEPARATOR);

    g_object_set(gtk_settings_get_default(), "gtk-button-images", TRUE, NULL);
    gtk_main();
    app_destroy(&data);

#ifdef OSSO_ABOOK_DEBUG
    list_leaked_windows();
#endif

    res = 0;
  }

err_osso:
  osso_deinitialize(osso);

err:
  return res;
}
