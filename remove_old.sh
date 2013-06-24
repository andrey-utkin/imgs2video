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

rm -rf `find $IMGSDIR/* -mtime +$SAVE_IMGS_DAYS`
rm -rf `find $VIDEODIR/* -mtime +$SAVE_VIDEO_HOURS_DAYS`
rm -rf `find $DAILY_VIDEO_DIR/* -mtime +$SAVE_VIDEO_DAYS_DAYS`
rm -rf `find $LOG_DIR/* -mtime +$SAVE_LOG_DAYS`
