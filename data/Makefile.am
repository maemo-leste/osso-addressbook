serviceentry_DATA   = com.nokia.osso_addressbook.service
desktopentry_DATA   = osso-addressbook.desktop
backupsettings_DATA = osso-addressbook.conf
prebackup_SCRIPTS = osso-addressbook-backup.sh
restore_SCRIPTS = osso-addressbook-restore.sh
ossocudscripts_SCRIPTS = \
	osso-addressbook-clean.sh \
	osso-addressbook-restore-compatibility.sh

schemasdir = $(GCONF_SCHEMA_FILE_DIR)
schemas_DATA = osso-addressbook.schemas

mimedir = $(datarootdir)/mime/packages
mime_DATA = osso-addressbook-mime.xml

EXTRA_DIST=\
	$(serviceentry_DATA) \
	$(desktopentry_DATA) \
	$(backupsettings_DATA) \
	$(prebackup_SCRIPTS) \
	$(restore_SCRIPTS) \
	$(ossocudscripts_SCRIPTS) \
	$(schemas_DATA) \
	$(mime_DATA)
