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

if [[ $# -lt 3 ]]
then
    echo 'Usage: <output file> -- <input file 1> [... <input file N>]'
    echo 'Options are configured in config file, pass filename as $IMGS2VIDEO_CFGFILE env var'
    exit 1
fi

DSTFILE=$1
shift 2
CATLIST="$@"
echo Catlist at cat.sh args: $CATLIST
CATLISTFILE=`mktemp`
for x in $CATLIST
do
    x=`readlink -f $x`
    echo "# `ls -l $x`" >> $CATLISTFILE
    if $FFPROBE $x
    then
        echo "file $x" >> $CATLISTFILE
    else
        echo "# BROKEN, EXCLUDED" >> $CATLISTFILE
    fi
done

DSTFILE_SRC=${DSTFILE}.src
RET=0
if [[ ! -e $DSTFILE_SRC ]] \
       || ( [[ `wc -l $DSTFILE_SRC | awk '{ print $1 }'` -le `wc -l $CATLISTFILE | awk '{ print $1 }'` ]] \
               || [[ -n "$CAT_EVEN_IF_SHORTER" ]] ) && ! diff -Nurd $DSTFILE_SRC $CATLISTFILE
then
    $FFMPEG -f concat -i $CATLISTFILE -vcodec copy -movflags faststart -y $DSTFILE
    RET=$?
    if [[ $RET == 0 ]]
    then
        cp $CATLISTFILE $DSTFILE_SRC
    else
        echo "ERROR: Concatenation of $DSTFILE failed"
    fi
else
    echo 'Concatenation skipped due to source files matching'
fi

rm $CATLISTFILE
exit $RET
