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
    echo "file `readlink -f $x`" >> $CATLISTFILE
done
$FFMPEG -f concat -i $CATLISTFILE -vcodec copy -y $DSTFILE
RET=$?
rm $CATLISTFILE
exit $RET
