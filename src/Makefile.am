bin_PROGRAMS = osso-addressbook

osso_addressbook_CFLAGS = \
			$(OSSO_ABOOK_CFLAGS) \
			-DOSSO_ABOOK_DEBUG \
			$(MAEMO_LAUNCHER_CFLAGS)

osso_addressbook_LDFLAGS = \
			-Wl,--as-needed $(OSSO_ABOOK_LIBS) \
			$(MAEMO_LAUNCHER_LDFLAGS)

osso_addressbook_SOURCES = \
			hw.c \
			utils.c \
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
