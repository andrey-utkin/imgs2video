#!/bin/bash

if [[ $# -ne 2 ]]
then
    echo 'Usage: <input images dir> <outfile>'
    echo 'Options are configured inside script body, in this version'
    exit 1
fi

IMGDIR=$1
OUTFILE=$2

IMGS2VIDEO=./imgs2video
BITRATE=1500k
HQDN3D=40
VPRE=medium

TMPFILE1=`tempfile --suffix .flv`
TMPFILE2=`tempfile --suffix .flv`
PASSLOGFILE=`tempfile`
PASSLOGFILE_FIX=$PASSLOGFILE'-0.log'
X264PLF=`tempfile --suffix .log`

function cleanup {
    echo "Cleaning up temp files"
    rm -v $TMPFILE1
    rm -v $TMPFILE2
    rm -v $PASSLOGFILE
    rm -v $PASSLOGFILE_FIX
    rm -v $X264PLF
    exit 0
}

trap cleanup INT TERM QUIT

# Assemble video from images dir using our C util
$IMGS2VIDEO -i $IMGDIR -o $TMPFILE1

# Apply High-quality denoise filter, greatly improves compressability
ffmpeg -y -i $TMPFILE1 -filter:v hqdn3d=${HQDN3D} -vcodec libx264 -sameq $TMPFILE2

# Two-passes transcoding, with given average bitrate target
# If quality is not satisfying, try
# 1. Use VPRE 'slow', 'veryslow'
# 2. Increase desired bitrate
ffmpeg -y -i $TMPFILE2 -pass 1 -passlogfile $PASSLOGFILE \
    -vcodec libx264 -vpre ${VPRE}_firstpass \
    -x264opts stats=$X264PLF \
    -b:v $BITRATE -f flv /dev/null
ffmpeg -y -i $TMPFILE2 -pass 2 -passlogfile $PASSLOGFILE \
    -vcodec libx264 -vpre $VPRE \
    -x264opts stats=$X264PLF \
    -b:v $BITRATE $OUTFILE

cleanup