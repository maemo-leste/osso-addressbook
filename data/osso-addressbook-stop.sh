#! /bin/sh

EDS_PID="pidof e-addressbook-factory"
DBUS_SEND="/usr/bin/dbus-send"
SHUTDOWN_FILE="/tmp/osso-abook-shutdown-in-progress"

# poor-man's IPC: create a file in the data directory to indicate that we are in
# the middle of a backup.  osso-addressbook can use the existence of this file
# to know that e-d-s is expected to exit and not indicate an error in this case
touch $SHUTDOWN_FILE
# clean up the file when the script exits
trap "rm -f $SHUTDOWN_FILE > /dev/null" 0

#let speeddial know that we are going to kill eds
$DBUS_SEND --system '/com/nokia/backup' 'com.nokia.backup.restore_start'

#give eds a second
sleep 1

i=0
while PIDS=`$EDS_PID` > /dev/null
  do
  if [ $i -ge 1 ]
  then
    sleep 1
  fi
  if [ $i -ge 10 ]
  then
    echo "killing eds..."
    kill -9 $PIDS > /dev/null
    exit 0
  else
    echo "trying to stop eds..."
    kill -15 $PIDS > /dev/null
  fi
  i=`expr $i + 1`
done

OSSO_ABOOK_PID="pidof osso-addressbook"
while PIDS=`$OSSO_ABOOK_PID` > /dev/null
do
  echo "killing addressbook..."
  kill -9 $PIDS > /dev/null
done

exit 0
