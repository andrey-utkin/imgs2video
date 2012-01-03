#!/bin/bash

if [[ $# -lt 2 ]]
then
    echo 'Usage: <input images dir> <outfile> [filter]'
    echo 'Options are configured inside script body, in this version'
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
FILTER=$3

TMPDIR=`mktemp --tmpdir --directory imgs2video.XXXXXXX`
TMPFILE1=$TMPDIR/imgs2video_out.$OFMT
pushd $TMPDIR

function cleanup {
    echo "Cleaning up temp files"
    rm -rfv $TMPDIR
    exit 0
}

trap cleanup INT TERM QUIT

if [[ "$TWOPASS" == true ]]
then
    I2V_OPTS="$I2V_OPTS --bitrate 0 "
else
    I2V_OPTS="$I2V_OPTS --bitrate $BITRATE "
fi

# Assemble video from images dir using our C util
if [[ -z $FILTER ]]
then
    $IMGS2VIDEO -i $IMGDIR -o $TMPFILE1 $I2V_OPTS
else
    $IMGS2VIDEO -i $IMGDIR -o $TMPFILE1 $I2V_OPTS --filter $FILTER
fi

if [[ "$TWOPASS" == true ]]
then
    # Two-passes transcoding, with given average bitrate target
    # If quality is not satisfying, try
    # 1. Use -vpre 'slow', 'veryslow' (also *_firstpass)
    # 2. Increase desired bitrate
    $FFMPEG -y -i $TMPFILE1 -pass 1 \
        -vcodec libx264  \
        -b $BITRATE -f $OFMT /dev/null
    $FFMPEG -y -i $TMPFILE1 -pass 2 \
        -vcodec libx264 \
        -b $BITRATE $OUTFILE
else
    mv $TMPFILE1 $OUTFILE
fi

cleanup
