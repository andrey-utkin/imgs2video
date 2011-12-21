#!/bin/bash

if [[ $# -lt 2 ]]
then
    echo 'Usage: <input images dir> <outfile> [filter]'
    echo 'Options are configured inside script body, in this version'
    exit 1
fi

IMGDIR=$1
OUTFILE=$2
FILTER=$3

IMGS2VIDEO=./imgs2video
FFMPEG=ffmpeg
BITRATE=2000k
VPRE=medium

TMPFILE1=`tempfile --suffix .flv`
PASSLOGFILE=`tempfile`
PASSLOGFILE_FIX=$PASSLOGFILE'-0.log'
X264PLF=`tempfile --suffix .log`

function cleanup {
    echo "Cleaning up temp files"
    rm -v $TMPFILE1
    rm -v $PASSLOGFILE
    rm -v $PASSLOGFILE_FIX
    rm -v $X264PLF
    exit 0
}

trap cleanup INT TERM QUIT

# Assemble video from images dir using our C util
if [[ -z $FILTER ]]
then
    $IMGS2VIDEO -i $IMGDIR -o $TMPFILE1
else
    $IMGS2VIDEO -i $IMGDIR -o $TMPFILE1 --filter $FILTER
fi

# Two-passes transcoding, with given average bitrate target
# If quality is not satisfying, try
# 1. Use VPRE 'slow', 'veryslow'
# 2. Increase desired bitrate
$FFMPEG -y -i $TMPFILE1 -pass 1 -passlogfile $PASSLOGFILE \
    -vcodec libx264 -vpre ${VPRE}_firstpass \
    -x264opts stats=$X264PLF \
    -b:v $BITRATE -f flv /dev/null
$FFMPEG -y -i $TMPFILE1 -pass 2 -passlogfile $PASSLOGFILE \
    -vcodec libx264 -vpre $VPRE \
    -x264opts stats=$X264PLF \
    -b:v $BITRATE $OUTFILE

cleanup
