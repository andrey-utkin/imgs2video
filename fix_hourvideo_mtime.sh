#!/bin/bash

while [[ $# != 0 ]]
do
    FULLPATH=$1
    # remove all the prefix until "/" character
    FILENAME=${FULLPATH##*/}
    # remove all the prefix unitl "." character
    FILEEXTENSION=${FILENAME##*.}
    FILENAMENOEXT=${FILENAME%*.$FILEEXTENSION}
    # remove a suffix, in our case, the filename, this will return the name of the directory that contains this file
    BASEDIRECTORY=${FULLPATH%$FILENAME}

    DATE="`echo $FILENAMENOEXT | sed 's/_/ /'`:00:00"
    touch $FULLPATH --date="$DATE"
    shift
done
