#!/bin/sh

BACKUP_CONTACTS=false
while [ -n "$1" -a $BACKUP_CONTACTS = false ]; do
    if [ "$1" = "comm_and_cal" ]; then
        BACKUP_CONTACTS=true
    fi
    shift
done
if [ $BACKUP_CONTACTS = false ]; then
    # We are not doing a backup of contacts
    exit 0
fi

ABOOK_DIR=$HOME/.osso-abook
BACKUP_DIR=$HOME/.osso-abook-backup
TP_BACKUP_DIR=$BACKUP_DIR/db/tp-cache

# Be sure EDS is not running
/etc/osso/osso-addressbook-stop.sh

rm -rf $BACKUP_DIR

cp -r $ABOOK_DIR $BACKUP_DIR
if [ $? != 0 ]
then
    exit 1
fi

# The DBs for the Telepathy backend are renamed to .restore files. This
# way they are not automatically deleted when cleaning up databases for
# non-existent accounts. The backend renames them before opening the
# corresponding book.
for db_cache_file in `ls $TP_BACKUP_DIR`
do
  db_cache_file="$TP_BACKUP_DIR/$db_cache_file"
  db_restore_file=`echo $db_cache_file | sed 's/\.restore//'`.restore

  if [ "$db_restore_file" = "$db_cache_file" ]; then
      # We are making the backup of a restore file, so no need to
      # rename it
      continue
  fi

  if [ -e $db_restore_file ]; then
      # We have both a DB file for an account and a restore file for
      # the same account, throw away the normal one and keep the
      # restore file
      rm -f $db_cache_file || true
  else
      mv -f $db_cache_file $db_restore_file
  fi
done
