#!/bin/bash
set -e

export IMGS2VIDEO_CFGFILE
`dirname $0`/build_all_from_hours.sh
`dirname $0`/remove_old.sh
