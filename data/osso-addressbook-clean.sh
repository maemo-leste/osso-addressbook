#! /bin/sh

SHUTDOWN_FILE="/tmp/osso-abook-shutdown-in-progress"
# poor-man's IPC: create a file in the data directory to indicate that we are in
# the middle of a backup.  osso-addressbook can use the existence of this file
# to know that e-d-s is expected to exit and not indicate an error in this case
touch $SHUTDOWN_FILE
# clean up the file when the script exits
trap "rm -f $SHUTDOWN_FILE > /dev/null" 0

# clean up osso-addressbook evolution data
PIDS=`pidof e-addressbook-factory` && kill -15 $PIDS
rm -rf $HOME/.osso-abook
rm -rf $HOME/.config/evolution-data-server
