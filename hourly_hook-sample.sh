#!/bin/bash

if [[ $# -ne 1 ]]
then
    echo "Usage: $0 <camera name>"
    exit 1
fi
NAME=$1
MASTER_SERVER=127.0.0.1
MASTER_SERVER_SSH_PORT=22

RSYNC_ATTEMPTS_MAX=10
SCRIPT_MAX_TIME=$(( 30 * 60 ))

RSYNC_ATTEMPT=0
SCRIPT_START_TIME=`date +%s`
RET=1
while [[ $RSYNC_ATTEMPT != $RSYNC_ATTEMPTS_MAX ]] && [[ $RET != 0 ]] && [[ "`date +%s`" -lt $(( SCRIPT_START_TIME + SCRIPT_MAX_TIME )) ]]
do
	RSYNC_ATTEMPT=$(( RSYNC_ATTEMPT + 1 ))
	rsync -e "ssh -p $MASTER_SERVER_SSH_PORT -o ConnectTimeout=10" --timeout 10 -av --partial /opt/imgs2video/$NAME/{video_hours,log} root@$MASTER_SERVER:/opt/imgs2video/$NAME
	RET=$?
	echo rsync attempt $RSYNC_ATTEMPT exited with $RET
done

if [[ $RET != 0 ]]
then
	echo "All rsync attempts failed, not launching remote build script"
	exit 1
fi
ssh -p $MASTER_SERVER_SSH_PORT root@$MASTER_SERVER "IMGS2VIDEO_CFGFILE=/opt/imgs2video/${NAME}.i2v nohup /opt/imgs2video/imgs2video/build_all_from_hours_and_remove_old.sh &> /opt/imgs2video/$NAME/log/build_all__`date +%F_%H`.log &"
