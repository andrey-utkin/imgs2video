#!/bin/bash
set -e
#gengetopt -i imgs2video.ggo -a args -F imgs2video_cmdline

# with CFLAG -I and LDFLAG -L you can set custom path to ffmpeg stuff
# and with export'ing LD_LIBRARY_PATH, to use it at runtime
# with -DLIBAV you can tell that you use libav.org libs, not ffmpeg.org
CFLAGS="-g -ggdb -Wall -Wextra -DLIBAV"
LDFLAGS="-lavformat -lavfilter"
gcc imgs2video.c compat.c imgs2video_cmdline.c -o imgs2video $CFLAGS $LDFLAGS
gcc cat.c -o cat $CFLAGS $LDFLAGS
