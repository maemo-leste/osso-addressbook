#!/bin/sh -e

BACKUP_DIR=$HOME/.osso-abook-backup
RESTORE_DIR=$HOME/.osso-abook-restore

RESTORE_LIST=$1
if ! [ -e $RESTORE_LIST ]; then
  echo "no restore file found"
  exit 1
fi

# Here we should check if $BACKUP_DIR is in $RESTORE_LIST but the list of
# files to restore sometimes include the content of directories and sometimes
# doesn't...
if [ ! -e $BACKUP_DIR ]; then
  # We are not restoring the contacts    
  exit 0
fi

rm -rf $RESTORE_DIR || true
mv $BACKUP_DIR $RESTORE_DIR

# The EDS factory will rename $RESTORE_DIR to the proper directory name
# at the next start.

# Make really sure that the EDS factory is really not running so we don't
# have to wait for a reboot to get the restored contacts.
# FIXME As a temporary workaround we also restart the applet as it doesn't
# seem to work properly.
set +e

HOME_APPLET_PID="pidof osso-abook-home-applet"
while PIDS=`$HOME_APPLET_PID` > /dev/null
do
  echo "killing addressbook home applet..."
  kill -9 $PIDS > /dev/null
done

/etc/osso/osso-addressbook-stop.sh

osso-abook-home-applet &
