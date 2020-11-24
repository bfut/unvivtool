#!/bin/bash

DESCRIPTION="extract all files from current_working_dir/$INPATH     NB: add option -p for dry run"


## https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_06_02

clear

SCRIPT_PATH="${0%/*}"

EXE="unvivtool"
OPTIONS="d -o -p"


INPATH='*/*'    # '*/*' looks for archives in subdirectories of current working directory
OUTPATH=$SCRIPT_PATH/out

mkdir $OUTPATH


for FILE in $INPATH
do
  if [ "${FILE##*/}" == "car.viv" ] || [ "${FILE##*/}" == "*.VIV" ]; then  # filter for filename
    FOLDER="${FILE%%/*}"  # get containing foldername
    echo ""
    echo "$folder" "$FILE"  "${FILE##/*}"
    mkdir $OUTPATH/$FOLDER
    $SCRIPT_PATH/../$EXE $OPTIONS $FILE "$OUTPATH/$FOLDER/car_viv"
  fi
done