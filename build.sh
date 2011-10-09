#!/bin/bash
#gengetopt -i imgs2video.ggo -a args -F imgs2video_cmdline
gcc imgs2video.c imgs2video_cmdline.c -lavformat -o imgs2video -g -ggdb
gcc cat.c -o cat -lavformat -g -ggdb
