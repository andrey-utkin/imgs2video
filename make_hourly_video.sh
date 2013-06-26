#!/bin/bash

if [[ $# -ne 1 ]]
then
    echo "Usage: $0 <dir with hour images>"
    exit 1
fi

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

DIR=$1
FIRSTFILE=`ls -rt $DIR/* | head -1`
HOUR=`date --reference=$FIRSTFILE +%H`
DATE=`date --reference=$FIRSTFILE +%F`

# Fill all hour timespace
LASTFILE=`ls -t $DIR/* | head -1`
FILEEXTENSION=`echo $FIRSTFILE | awk -F. '{print $NF}'`
if [[ $FIRSTFILE != $DIR/0000.${FILEEXTENSION} ]]
then
    cp $FIRSTFILE $DIR/0000.${FILEEXTENSION}
    touch --date="$DATE $HOUR:00:00" $DIR/0000.${FILEEXTENSION}
fi
if [[ $LASTFILE != $DIR/5959.${FILEEXTENSION} ]]
then
    cp $LASTFILE $DIR/5959.${FILEEXTENSION}
    touch --date="$DATE $HOUR:59:59" $DIR/5959.${FILEEXTENSION}
fi

# check if the hour belongs to night, to apply denoise filter
if [[ 10#$HOUR -ge 18 || 10#$HOUR -le 5 ]]
then
    FILTER="$FILTER,hqdn3d=20"
fi

for OFMT in $OFMTS
do
    export OFMT
    export VIDEO_ENCODING_OPTS=${VIDEO_ENCODING_OPTS_ARRAY[$OFMT]}
    DSTFILE=$VIDEODIR/${DATE}_${HOUR}.$OFMT
    if [[ -e $DSTFILE ]]
    then
        continue
    fi
    `dirname $0`/assemble_and_compress.sh $DIR $DSTFILE $FILTER
    if [[ $? -ne 0 ]]
    then
        echo "Assembling failed"
        exit 1
    fi
    echo "Assembling succeed"
    touch --date="$DATE $HOUR:00:00" $DSTFILE
done
