#!/bin/bash

DESCRIPTION="encode all files in current working directory to new archive car.viv, which is created in current working directory    NB: add option -p for dry run"

"backups save!"


clear

SCRIPT_PATH="${0%/*}"
CURRENT_WORKING_DIRECTORY=$PWD

EXE="unvivtool"
OPTIONS="e -o -p"


INPATH='*'
##OUTPATH=$SCRIPT_PATH/out

find $PWD -maxdepth 1 -name '*' -type f -print0  | xargs -0 echo
find $PWD -maxdepth 1 -name '*' -type f -print0  | xargs -0 $SCRIPT_PATH/../$EXE $OPTIONS $PWD/car.viv
