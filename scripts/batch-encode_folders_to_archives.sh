#!/bin/bash

# This file is distributed under: CC BY-SA 4.0 <https://creativecommons.org/licenses/by-sa/4.0/>

DESCRIPTION="loop subdirectories of current_working_dir/$INPATH named car_viv, encode all files to respective car.viv archives     NB: add option -p for dry run"

# Example:
#
#   current working directory: carmodel
#
# carmodel/
# |     -- cout/
#          | -- car.viv
# |     -- ...
# After running batch-decode_archives_to_folders.sh, you may have this
# carmodel/
# |     -- cout/
#          | -- car.viv
#          | -- car_viv/
#               |    -- car.fce
#               |    -- car00.tga
#               |       ...
# |     -- ...
# Running this script, batch-encode_folders_to_archives.sh,
# all files from the car_viv directories will be encoded, overwriting car.viv
# archives, respectively.


clear

SCRIPT_PATH="${0%/*}"

EXE="unvivtool"
OPTIONS="e -v"


INPATH='*/*/*/'
##OUTPATH=$SCRIPT_PATH/out


for dir in $INPATH
do
    dir=${dir%*/}    # remove trailing slash
    if [ "${dir##*/}" == "car_viv" ]; then
      echo ""
      echo $dir
###      find $dir -maxdepth 1 -name '*' -type f -print0  | xargs -0 echo
      find $dir -maxdepth 1 -name '*' -type f -print0  | xargs -0 $SCRIPT_PATH/../$EXE $OPTIONS $dir/../car.viv
    fi
done
