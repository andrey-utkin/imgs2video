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

`dirname $0`/fix_hourvideo_mtime.sh $VIDEODIR/*

DATE=`date +%F`
HOUR=`date +%H`
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
`dirname $0`/cat_all_days.sh
