#!/bin/bash

RSYNC_LIST_FILE="$SRC_DIR/hv_sync.list"

RSYNC_OPTS="$RSYNC_OPTS --size-only --partial --partial-dir=.rsync-partial"

function hv_and_log_sync {
    if [[ -z "$DST_URI" ]]
    then
	    echo "DST_URI not set, aborting" >&2
	    exit
    fi

    while true
    do
        sleep 60  # give some rest to weak net channels
        # Get a list of files to sync in video_hours/ with --dry-run, on failure `continue` (go to next lap)
        # Sort this list, most recent modification time first
        # Used naming scheme allows simple lexicographical sorting to work as mtime sort
        touch "${RSYNC_LIST_FILE}.timestamp"
        sleep 1
        rsync -e "$RSYNC_E" $RSYNC_OPTS --archive --dry-run --verbose "$SRC_DIR/video_hours" "$DST_URI" | egrep 'video_hours/.+' | sort --reverse > "$RSYNC_LIST_FILE"
        if [[ $? != 0 ]]
        then
            continue
        fi

        if [[ `wc -l "$RSYNC_LIST_FILE" | awk '{ print $1 }'` == 0 ]]
        then
            while [[ `find "$SRC_DIR/video_hours" -maxdepth 0 -newer "${RSYNC_LIST_FILE}.timestamp" | wc -l` == 0 ]]
            do
                sleep 10
            done

            continue
        fi

        # Start with the most recent video_hours/ file.
        I=1
        MOST_RECENT_VIDEO_HOURS_FILE="$SRC_DIR/`head -n 1 $RSYNC_LIST_FILE`"
        while true
        do
            CURRENT_FILE=`tail -n +$I $RSYNC_LIST_FILE | head -n 1` # TODO Get i-th file in the list
            # If all entries are done, upload logs
            if [[ "$CURRENT_FILE" == "" ]]
            then
                break
            fi

            I=$(( I + 1 ))

            # rsync this file alone (look out for correct destination dirs)
            RET=1
            while true
            do
                sleep 1
                rsync -e "$RSYNC_E" $RSYNC_OPTS --archive --partial --verbose "$SRC_DIR/$CURRENT_FILE" "$DST_URI/video_hours/"
                # On success move on to next file;
                if [[ $? == 0 ]]
                then
                    break  # go to next $CURRENT_FILE
                fi

                # ...If newer file appears in video_hours (the dir itself gets modified, as checked here), rebuild the list and start from the beginning
		# Also restart if $MOST_RECENT_VIDEO_HOURS_FILE disappears
                if [[ ! -e $MOST_RECENT_VIDEO_HOURS_FILE ]] \
			|| [[ `find "$SRC_DIR/video_hours" -maxdepth 0 -newer $MOST_RECENT_VIDEO_HOURS_FILE | wc -l` != 0 ]]
                then
                    break 2
                fi
            done
        done

        # Sync log/
        # Retry only if rsync exited with failure and video_hours/ doesn't have newer files
        while true
        do
            sleep 1
            rsync -e "$RSYNC_E" $RSYNC_OPTS --archive --partial --verbose "$SRC_DIR/log" "$DST_URI"
            if [[ $? == 0 ]] || [[ `find "$SRC_DIR/video_hours" -maxdepth 0 -newer $MOST_RECENT_VIDEO_HOURS_FILE | wc -l` != 0 ]]
            then
                break
            fi
        done
    done
}
