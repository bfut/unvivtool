#!/bin/bash

# This file is distributed under: CC BY-SA 4.0 <https://creativecommons.org/licenses/by-sa/4.0/>

DESCRIPTION="extract all files from current_working_dir/$INPATH     NB: add option -p for dry run"

clear

SCRIPT_PATH="${0%/*}"

EXE="unvivtool"
OPTIONS="d -p"


INPATH='*/*'    # '*/*' looks for archives in subdirectories of current working directory
OUTPATH=$SCRIPT_PATH/out

#mkdir $OUTPATH


for FILE in $INPATH
do
#  if [ "${FILE##*/}" == "*.viv" ] || [ "${FILE##*/}" == "*.VIV" ]; then  # filter for filename
    FOLDER="${FILE%%/*}"  # get containing foldername
    echo ""
    echo "$folder" "$FILE"  "${FILE##/*}"
    mkdir -p "$OUTPATH/$FOLDER/car_viv"
    $SCRIPT_PATH/../$EXE $OPTIONS $FILE "$OUTPATH/$FOLDER/car_viv"
 # fi
done