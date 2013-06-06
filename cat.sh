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

DSTFILE=$1
shift 2
CATLIST="$@"
echo Catlist at cat.sh args: $CATLIST
CATLISTFILE=`mktemp`
for x in $CATLIST
do
    echo "file $x" >> $CATLISTFILE
done
$FFMPEG -f concat -i $CATLISTFILE -vcodec copy -y $DSTFILE
RET=$?
rm $CATLISTFILE
exit $RET
