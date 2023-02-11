#!/bin/bash

# This file is distributed under: CC BY-SA 4.0 <https://creativecommons.org/licenses/by-sa/4.0/>

DESCRIPTION="encode all files in current working directory to new archive car.viv, which is created in current working directory    NB: add option -p for dry run"


clear

SCRIPT_PATH="${0%/*}"
CURRENT_WORKING_DIRECTORY=$PWD

EXE="unvivtool"
OPTIONS="e -s"


INPATH='*'
##OUTPATH=$SCRIPT_PATH/out

find $PWD -maxdepth 1 -name '*' -type f -print0  | xargs -0 echo
find $PWD -maxdepth 1 -name '*' -type f -print0  | xargs -0 $SCRIPT_PATH/../$EXE $OPTIONS $PWD/car.viv
