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

IMGSDIR_MMIN=$(( SAVE_IMGS_DAYS * 60 * 24 + 60 ))
find $IMGSDIR/* -mmin +$IMGSDIR_MMIN -exec rm -rf {} \; &>/dev/null

VIDEODIR_MMIN=$(( SAVE_VIDEO_HOURS_DAYS * 60 * 24 + 60 ))
find $VIDEODIR/* -mmin +$VIDEODIR_MMIN -exec rm -rf {} \; &>/dev/null

DAILY_VIDEO_DIR_MMIN=$(( SAVE_VIDEO_DAYS_DAYS * 60 * 24 + 60 ))
find $DAILY_VIDEO_DIR/* -mmin +$DAILY_VIDEO_DIR_MMIN -exec rm -rf {} \; &>/dev/null

LOG_DIR_MMIN=$(( SAVE_LOG_DAYS * 60 * 24 + 60 ))
find $LOG_DIR/* -mmin +$LOG_DIR_MMIN -exec rm -rf {} \; &>/dev/null
