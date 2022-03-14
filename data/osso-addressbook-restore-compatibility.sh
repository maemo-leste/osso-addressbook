#!/bin/sh

# This script is kept only for compatibility with backups made with a
# pre-release version of the address book and does nothing with newer
# backups.

DBUS_SEND="/usr/bin/dbus-send"

DB_DIR=$HOME/.osso-abook/db
BACKUP_VCF=$DB_DIR/backup.vcf

RESTORE_LIST=$1
if ! [ -e $RESTORE_LIST ]; then
  echo "no restore file found"
  exit 1
fi

if ! grep -q $BACKUP_VCF $RESTORE_LIST; then
  # We are not restoring the contacts
  exit 0
fi

if ! [ -d $HOME/.osso-abook ]; then
  echo "no $HOME/.osso-abook directory found"
  exit 1
fi

trap "$DBUS_SEND --system '/com/nokia/backup' 'com.nokia.backup.restore_finish'" 0

exec 1>/dev/null
exec 2>/dev/null

# always remove the summary
rm -f $DB_DIR/addressbook.db.summary

# restore the backed up data
/usr/bin/osso-addressbook-backup -i $BACKUP_VCF
if [ $? -ne 0 ]
then
  echo "cannot import backup file"
  exit 1
fi

# remove the backup file since its contents went to db
rm -f $BACKUP_VCF


# restore the .changes.db files
CHANGES_BAK_DIR=$DB_DIR/changes-bak

for bak_file in `ls $CHANGES_BAK_DIR/*`
do
  dest=$DB_DIR/`basename $bak_file`
  # This is atomic so just moving the file is enough
  mv -f $bak_file $dest
done

rm -rf $CHANGES_BAK_DIR

exit 0
