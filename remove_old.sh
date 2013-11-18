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

IMGSDIR_MTIME=$(( SAVE_IMGS_DAYS - 1 ))
[[ IMGSDIR_MTIME -lt 0 ]] && IMGSDIR_MTIME=0  # avoid negative values
find $IMGSDIR/* -mtime +$IMGSDIR_MTIME -exec rm -rf {} \; &>/dev/null

VIDEODIR_MTIME=$(( SAVE_VIDEO_HOURS_DAYS - 1 ))
[[ VIDEODIR_MTIME -lt 0 ]] && VIDEODIR_MTIME=0  # avoid negative values
find $VIDEODIR/* -mtime +$VIDEODIR_MTIME -exec rm -rf {} \; &>/dev/null

DAILY_VIDEO_DIR_MTIME=$(( SAVE_VIDEO_DAYS_DAYS - 1 ))
[[ DAILY_VIDEO_DIR_MTIME -lt 0 ]] && DAILY_VIDEO_DIR_MTIME=0  # avoid negative values
find $DAILY_VIDEO_DIR/* -mtime +$DAILY_VIDEO_DIR_MTIME -exec rm -rf {} \; &>/dev/null

LOG_DIR_MTIME=$(( SAVE_LOG_DAYS - 1 ))
[[ LOG_DIR_MTIME -lt 0 ]] && LOG_DIR_MTIME=0  # avoid negative values
find $LOG_DIR/* -mtime +$LOG_DIR_MTIME -exec rm -rf {} \; &>/dev/null
