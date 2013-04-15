#!/bin/bash

# This script is meant to be launched separately
# It checks if monitored file is modified recently
# If modification time of file is too old, it sends email

if [[ $# -ne  4 ]]
then
    echo "Usage: $0 <monitored object name> <abs file path> <oldness threshold, in minutes> <email addresses>"
    exit 1
fi

NAME="$1"
FILE="$2"
TIME_THRESHOLD="$3"
EMAILS="$4"

STATUS_FILE=/tmp/watchdog__"$NAME".status
SENDS_INTERVAL_SECONDS=$(( $TIME_THRESHOLD * 60 ))

while true
do
    if [[ `find "$FILE" -mmin +$TIME_THRESHOLD | wc -l` != "0" ]]
    then
        if [[ -e "$STATUS_FILE" ]]
        then
            source "$STATUS_FILE"
        else
            LAST_SEND=0
        fi
        NOW=`date +%s`
        NEXT_SEND_ALLOWED=$(( $LAST_SEND + $SENDS_INTERVAL_SECONDS ))
        if [[ $NOW -gt $NEXT_SEND_ALLOWED ]]
        then
            #(echo "$NAME problem - check";) | mail -s "Video problem" $EMAILS # Simple way. EMAILS MUST BE SPACE-SEPARATED!
            echo "$NAME problem - check" | mailsend -smtp smtp_server,_e.g._smtp.gmail.com -port port,_e.g._587 -t "$EMAILS" -f sender_email -user sender_email -pass your_password -starttls -auth-login -sub "Video problem" # EMAILS MUST BE COMMA-SEPARATED!
            echo "LAST_SEND=$NOW" > "$STATUS_FILE"
        fi
    fi
    sleep 10m;
done
