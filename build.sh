#!/bin/bash
#gengetopt -i imgs2video.ggo -a args -F imgs2video_cmdline

# with CFLAG -I and LDFLAG -L you can set custom path to ffmpeg stuff
# and with export'ing LD_LIBRARY_PATH, to use it at runtime
CFLAGS="-g -ggdb"
LDFLAGS="-lavformat -lavfilter"
gcc imgs2video.c imgs2video_cmdline.c -o imgs2video $CFLAGS $LDFLAGS
gcc cat.c -o cat $CFLAGS $LDFLAGS
