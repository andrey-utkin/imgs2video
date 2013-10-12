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

find $IMGSDIR/* -mtime +$SAVE_IMGS_DAYS -exec rm -rf {} \; &>/dev/null
find $VIDEODIR/* -mtime +$SAVE_VIDEO_HOURS_DAYS -exec rm -rf {} \; &>/dev/null
find $DAILY_VIDEO_DIR/* -mtime +$SAVE_VIDEO_DAYS_DAYS -exec rm -rf {} \; &>/dev/null
find $LOG_DIR/* -mtime +$SAVE_LOG_DAYS -exec rm -rf {} \; &>/dev/null
