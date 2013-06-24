#!/bin/bash

if [[ -z $IMGS2VIDEO_CFGFILE ]]
then
    echo 'IMGS2VIDEO_CFGFILE env var must be available'
    exit 1
fi
if ! [[ -f $IMGS2VIDEO_CFGFILE ]]
then
    echo "Cfg file $IMGS2VIDEO_CFGFILE does not exist"
    exit 1
fi
source $IMGS2VIDEO_CFGFILE
export IMGS2VIDEO_CFGFILE

mkdir -p $DAILY_VIDEO_DIR

`dirname $0`/remove_old.sh

# copy-paste from daemon.sh hourly()
LATEST_VIDEO_HOUR_FILE=`ls -t $VIDEODIR/* | head -1`
DATE=`date --reference=$LATEST_VIDEO_HOUR_FILE +%F`
HOUR=`date --reference=$LATEST_VIDEO_HOUR_FILE +%H`

CAT_LOGFILE=$LOG_DIR/cat__${DATE}_${HOUR}.log
`dirname $0`/cat_lastday.sh &> $CAT_LOGFILE
if [[ $? != 0 ]]
then
    echo "Concatenation failed"
    if [[ -n "$NOTIF_EMAILS" ]]
    then
      cat $CAT_LOGFILE | mail -s "Video concatenation failed on $NAME" -a $CAT_LOGFILE $NOTIF_EMAILS
    fi
fi
echo "Concatenation succeed."
if [[ $HOUR == 23 ]]
then
    for OFMT in $OFMTS
    do
        cp -v ${DAYFILE}.$OFMT $DAILY_VIDEO_DIR/${DATE}.$OFMT
    done
fi
`dirname $0`/cat_all_days.sh
