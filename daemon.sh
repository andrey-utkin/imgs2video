#!/bin/bash

if [[ $# -ne 1 ]]
then
    echo "Usage: $0 <config file>"
    echo "Config file should be derived from config.inc.sh.sample"
    exit 1
fi
export IMGS2VIDEO_CFGFILE=$1 # exporting for routines in sub-shells
if ! [[ -f $IMGS2VIDEO_CFGFILE ]]
then
    echo "Cfg file $IMGS2VIDEO_CFGFILE does not exist"
    exit 1
fi
source $IMGS2VIDEO_CFGFILE

mkdir -p $IMGSDIR
mkdir -p $VIDEODIR
mkdir -p $DAILY_VIDEO_DIR
mkdir -p $LOG_DIR

function hourly {
    hourly_work "$@"
    $AFTER_HOUR_PROC_HOOK
    if [[ $3 == 23 ]]
    then
        $DAILY_HOOK
    fi
}

function hourly_work {
# args:
# $1 dir with images for this hour
# $2 resulting file
# $3 actual hour, for filtering logics
# $4 actual date (YYYY-MM-DD), for daily video saving filename
    echo Gonna remove old files
    find $IMGSDIR -mtime +$SAVE_IMGS_DAYS -exec rm -rf {} \;
    find $VIDEODIR -mtime +$SAVE_VIDEO_HOURS_DAYS -exec rm -rf {} \;
    find $DAILY_VIDEO_DIR -mtime +$SAVE_VIDEO_DAYS_DAYS -exec rm -rf {} \;
    find $LOG_DIR -mtime +$SAVE_LOG_DAYS -exec rm -rf {} \;

    echo Gonna assemble $1 to $2
    if [[ -z "`ls $1`" ]]
    then
        echo This dir is empty, removing it and skipping
        rmdir $1
        return
    fi
# check if the hour belongs to night, to apply denoise filter
    for x in 18 19 20 21 22 23 00 01 02 03 04 05
    do
        if [[ $x == $3 ]]
        then
            FILTER='hqdn3d=20'
            break
        fi
    done

    `dirname $0`/assemble_and_compress.sh $1 $2 $FILTER &> $LOG_DIR/assemble__${4}_${3}.log
    if [[ $? -ne 0 ]]
    then
        echo Assembling failed, skipping catenation
        return
    fi
    echo "Assembling succeed."
    LAST24=`ls -rt \`find $VIDEODIR/* -mtime -1\` ` # may be not exactly 24 pieces
    if [[ -z "$LAST24" ]]
    then
        echo "No pieces to concatenate, surprisingly. Skipping"
        return
    fi
    `dirname $0`/cat ${DAYFILE}_part.$OFMT -- $LAST24 &> $LOG_DIR/cat__${4}_${3}.log
    if [[ $? -ne 0 ]]
    then
        echo "Concatenation of files $LAST24 failed" >&2
        return
    fi
    echo "Concatenation succeed."
    mv ${DAYFILE}_part.$OFMT ${DAYFILE}.$OFMT
    if [[ "$3" == 23 ]]
    then
        cp -v ${DAYFILE}.$OFMT $DAILY_VIDEO_DIR/${4}.$OFMT
    fi
    echo "Hourly job succeed."
}

PREV_LAP_HOUR='unknown'
PREV_LAP_DAY='unknown'

while true
do
    DATE=`date --utc +'%F %H %M%S'`
    DAY=`echo $DATE | awk '{ printf $1 }'`
    HOUR=`echo $DATE | awk '{ printf $2 }'`
    MINSEC=`echo $DATE | awk '{ printf $3 }'`

#assertion
    if [[ -e $IMGSDIR/$DAY/$HOUR ]] && ! [[ -d $IMGSDIR/$DAY/$HOUR ]]
    then
        echo "Unneeded file exists: $IMGSDIR/$DAY/$HOUR - must be dir" >&2
        exit 1
    fi

    if ! [[ -e $IMGSDIR/$DAY/$HOUR ]]
    then
        mkdir -p $IMGSDIR/$DAY/$HOUR
    fi

    FILENAME=$IMGSDIR/$DAY/$HOUR/${MINSEC}.jpg
    echo filename is $FILENAME
    wget --no-verbose $URL -O $FILENAME 2>&1
    if [[ $? -ne 0 ]]
    then
        echo "wget $FILENAME failed, surviving" >&2
        rm $FILENAME
    fi

    if [[ $PREV_LAP_HOUR != 'unknown' ]] && [[ $PREV_LAP_HOUR != $HOUR ]]
    then
        echo "Hour has ticked from $PREV_LAP_HOUR to $HOUR, launching video assembling"
        hourly $IMGSDIR/$PREV_LAP_DAY/$PREV_LAP_HOUR $VIDEODIR/${PREV_LAP_DAY}_${PREV_LAP_HOUR}.$OFMT $PREV_LAP_HOUR $PREV_LAP_DAY & #executes in subshell, no back data flow is possible
    fi

    $AFTER_GET_IMAGE_HOOK

    PREV_LAP_HOUR=$HOUR
    PREV_LAP_DAY=$DAY
done

