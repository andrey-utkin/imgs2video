#!/bin/bash

IMGSDIR=imgs_by_hour_dir
VIDEODIR=video_hours
DAYFILE=last_day #prefix without extension
URL='http://upload.wikimedia.org/wikipedia/commons/1/1e/Stonehenge.jpg'

mkdir -p $IMGSDIR
mkdir -p $VIDEODIR

function hourly {
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

    ./assemble_and_compress.sh $1 $2 $FILTER
    LAST24=`ls -t $VIDEODIR/* | head -n 24 | tac`
    if [[ -z "$LAST24" ]]
    then
        echo Assembling failed, skipping catenation
        return
    fi
    ./cat ${DAYFILE}_part.flv -- $LAST24
    if [[ $? -eq 0 ]]
    then
        mv ${DAYFILE}_part.flv ${DAYFILE}.flv
    else
        echo "Concatenation of files $LAST24 failed" >&2
    fi
}

PREV_LAP_HOUR='unknown'
PREV_LAP_DAY='unknown'

while true
do
    DATE=`date +'%F %H %M%S'`
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
    wget $URL -O $FILENAME 2>&1
    if [[ $? -ne 0 ]]
    then
        echo "wget $FILENAME failed, surviving" >&2
        rm $FILENAME
    fi

    if [[ $PREV_LAP_HOUR != 'unknown' ]] && [[ $PREV_LAP_HOUR != $HOUR ]]
    then
        echo "Hour has ticked from $PREV_LAP_HOUR to $HOUR, launching video assembling"
        hourly $IMGSDIR/$PREV_LAP_DAY/$PREV_LAP_HOUR $VIDEODIR/${PREV_LAP_DAY}_${PREV_LAP_HOUR}.flv $PREV_LAP_HOUR & #executes in subshell, no back data flow is possible
    fi

    PREV_LAP_HOUR=$HOUR
    PREV_LAP_DAY=$DAY
done

