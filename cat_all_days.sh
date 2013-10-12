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

echo Gonna remove zero-sized video hour files, if any
find $VIDEODIR -maxdepth 1 -type f -size 0 -exec rm -vf {} \;

# TODO Check for yesterday..(today-$SAVE_VIDEO_HOURS_DAYS)
for x in `seq 1 $SAVE_VIDEO_HOURS_DAYS`
do
    OLD_DATE=`date +%F --date="$x day ago"`
    for OFMT in $OFMTS
    do
        THAT_DAY_VIDEO=$DAILY_VIDEO_DIR/${OLD_DATE}.${OFMT}
        if ! [[ -e $THAT_DAY_VIDEO ]]
        then
            `dirname $0`/cat.sh $THAT_DAY_VIDEO -- $VIDEODIR/${OLD_DATE}_*.${OFMT}
        fi
    done
done

