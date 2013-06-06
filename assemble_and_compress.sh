#!/bin/bash

if [[ $# -lt 2 ]]
then
    echo 'Usage: <input images dir> <outfile> [filter]'
    echo 'Options are configured in config file, pass filename as $IMGS2VIDEO_CFGFILE env var'
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

IMGDIR=`readlink -f $1`
OUTFILE=`readlink -f $2`
FILTER=${3:-null}

TMPDIR=`mktemp --tmpdir --directory imgs2video.XXXXXXX`
TMPFILE1=$TMPDIR/imgs2video_out.$OFMT
pushd $TMPDIR

function cleanup {
    echo "Cleaning up temp files"
    rm -rfv $TMPDIR
    exit 0
}

trap cleanup INT TERM QUIT

echo Making $TMPFILE1 of $IMGDIR
$FFMPEG \
        -video_size ${IN_WIDTH}x${IN_HEIGHT} \
        -err_detect explode \
        -f image2 \
        -ts_from_file 1 \
        -pattern_type glob \
        -i $IMGDIR'/*.jpg' \
        -vf "$FILTER,settb=1/1000,setpts=(PTS-STARTPTS)/$SPEEDUP,fps=$FRAMERATE" \
        $VIDEO_ENCODING_OPTS \
        -y \
        $TMPFILE1
RET=$?
if [[ $RET == 0 ]]
then
    mv $TMPFILE1 $OUTFILE
    cleanup
else
    rm -rfv $TMPDIR
    exit $RET
fi
