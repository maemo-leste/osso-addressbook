bin_PROGRAMS = osso-addressbook

osso_addressbook_CFLAGS = \
			$(OSSO_ABOOK_CFLAGS) \
			-DOSSO_ABOOK_DEBUG \
			$(MAEMO_LAUNCHER_CFLAGS)

osso_addressbook_LDFLAGS = \
			$(OSSO_ABOOK_LIBS) \
			-Wl,--as-needed $(MAEMO_LAUNCHER_LDFLAGS)

osso_addressbook_SOURCES = \
			hw.c \
			sim.c \
			importer.c \
			service.c \
			groups.c \
			osso-abook-get-your-contacts-dialog.c \
			osso-abook-sim-group.c \
			osso-abook-recent-view.c \
			contacts.c \
			menu.c \
			app.c \
			actions.c \
			main.c
