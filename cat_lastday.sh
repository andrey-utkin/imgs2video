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

if [[ $# -ne 0 ]]
then
    echo 'Usage: no args'
    echo 'This script merges available recordings of last 24 hours.'
    echo 'Options are configured in config file, pass filename as $IMGS2VIDEO_CFGFILE env var'
    exit 1
fi

echo Gonna remove zero-sized video hour files, if any
find $VIDEODIR -maxdepth 1 -type f -size 0 -exec rm -vf {} \;

for OFMT in $OFMTS
do
    CATLIST=""
    for x in $VIDEODIR/*.$OFMT
    do
        if [[ -n "`find $x -mmin -$((25*60))`" ]]
        then
            CATLIST="$x $CATLIST"
        fi
    done
    if [[ -z "$CATLIST" ]]
    then
        echo Nothing to concatenate, no videos in last 24h
        continue
    fi

    CATLIST=`ls -rt $CATLIST`
    echo Catlist generated by cat_lastday.sh: $CATLIST
    export CAT_EVEN_IF_SHORTER=1
    `dirname $0`/cat.sh ${DAYFILE}_part.$OFMT -- $CATLIST
    if [[ $? != 0 ]]
    then
        echo "ERROR: cat.sh ${DAYFILE}_part.$OFMT failed (from cat_lastday.sh)"
        rm ${DAYFILE}_part.$OFMT
        continue
    fi

# temporary _part file will be absent if .src matches with previous
    if [[ -e ${DAYFILE}_part.$OFMT ]]
    then
        mv ${DAYFILE}_part.$OFMT ${DAYFILE}.$OFMT
    fi
done
